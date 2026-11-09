#!/usr/bin/env python3
"""Splits a large E32 executable into a small EXE and several DLLs.

On a phone, the loader refuses an image with more than about 12 MiB of code
from the mass-memory drive (E:): that drive is not demand paged, and the
loader reads the whole image at once. browser.exe has some 58 MiB of code, so
it could only be installed on C:. This tool cuts the linked program into
pieces that each stay well under that limit:

  EXE    a stub that holds all the program's writable data (.data and .bss),
         fills in the data's pointers to code at startup, and then jumps to
         the original entry point.
  DLLs   the read-only image (code, .rodata, unwind tables) in contiguous
         pieces of the original .text, each cut at a function boundary.

Nothing is recompiled. Each piece keeps the bytes of the linked ELF, and the
references that cross a cut are rewritten:

  * Absolute words (literal pools, vtables, ...) that point into another
    piece become E32 imports. Each DLL exports an anchor every 64 KiB, and an
    import word holds the anchor's ordinal and a 16-bit addend.
  * Branches (BL, B, BLX) into another piece go through a stub at either end
    of the calling piece: "ldr pc, [pc, #-4]" and an imported address.
    LLD's PC-relative long-branch thunks are rewritten in place the same way.
  * Branches into the PLT become stubs that import the system function.
  * Each DLL gets its own EHABI index (.ARM.exidx) and exception table copy,
    and its own exception descriptor in the E32 header.

The data stays in the EXE, which EKA2 always places at 0x00400000, so the
DLLs refer to it with plain constants and need no writable data of their
own. The stub checks that address at startup.

Inputs are the linked ELF (with --emit-relocs) and the E32 image that
symbian-native made from it: the image's relocations and imports say which
words are absolute and what each import resolves to.
"""

import argparse
import bisect
import struct
import sys
from collections import defaultdict
from pathlib import Path

R_ARM_ABS32 = 2
R_ARM_REL32 = 3
R_ARM_THM_CALL = 10
R_ARM_CALL = 28
R_ARM_JUMP24 = 29
R_ARM_TARGET1 = 38
R_ARM_TARGET2 = 41
R_ARM_PREL31 = 42
R_ARM_GOT_PREL = 96

EXE_DATA_BASE = 0x00400000
ANCHOR_SPACING = 0x10000
HEADER_SIZE = 156
CRC_INITIALISER = 0xC90FDAA2
E32_FLAGS = 0x12000028
UID_EXE = 0x1000007A
UID_DLL = 0x10000079
UID2_DLL = 0x1000008D
EXIDX_CANTUNWIND = 1
STUB_LDR_PC = 0xE51FF004  # ldr pc, [pc, #-4]


class SplitError(Exception):
    pass


def require(condition, message):
    if not condition:
        raise SplitError(message)


def u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def u16(data, offset):
    return struct.unpack_from("<H", data, offset)[0]


def sign_extend(value, bits):
    value &= (1 << bits) - 1
    return value - (1 << bits) if value & (1 << (bits - 1)) else value


def prel31(word, place):
    return (place + sign_extend(word, 31)) & 0xFFFFFFFF


def encode_prel31(target, place, high_bit=0):
    offset = target - place
    require(-(1 << 30) <= offset < (1 << 30), "PREL31 offset out of range")
    return (offset & 0x7FFFFFFF) | high_bit


def arm_immediate(instruction):
    imm8 = instruction & 0xFF
    rotate = ((instruction >> 8) & 0xF) * 2
    return ((imm8 >> rotate) | (imm8 << (32 - rotate))) & 0xFFFFFFFF


# --- ELF --------------------------------------------------------------------


class Elf:
    def __init__(self, data):
        require(data[:4] == b"\x7fELF" and data[4] == 1 and data[5] == 1, "not a 32-bit little-endian ELF")
        self.data = data
        shoff = u32(data, 0x20)
        shentsize, shnum, shstrndx = struct.unpack_from("<HHH", data, 0x2E)
        raw = []
        for index in range(shnum):
            raw.append(struct.unpack_from("<10I", data, shoff + index * shentsize))
        strtab = raw[shstrndx]
        self.sections = []
        self.by_name = {}
        for index, fields in enumerate(raw):
            name_offset, kind, flags, addr, offset, size, link, info, align, entsize = fields
            start = strtab[4] + name_offset
            name = data[start:data.index(b"\0", start)].decode()
            section = dict(index=index, name=name, type=kind, flags=flags, addr=addr,
                           offset=offset, size=size, link=link, info=info)
            self.sections.append(section)
            self.by_name.setdefault(name, section)

    def section(self, name):
        require(name in self.by_name, f"ELF has no {name} section")
        return self.by_name[name]

    def symbols(self):
        symtab = next(s for s in self.sections if s["type"] == 2)
        strtab = self.sections[symtab["link"]]
        base = strtab["offset"]
        data = self.data
        result = []
        for name_offset, value, size, info, other, shndx in struct.iter_unpack(
                "<IIIBBH", data[symtab["offset"]:symtab["offset"] + symtab["size"]]):
            end = data.index(b"\0", base + name_offset)
            result.append((value, size, info & 0xF, shndx, data[base + name_offset:end]))
        return result

    def relocations(self, name):
        section = self.by_name.get(name)
        if section is None:
            return []
        data = self.data[section["offset"]:section["offset"] + section["size"]]
        return [(offset, info & 0xFF, info >> 8) for offset, info in struct.iter_unpack("<II", data)]


# --- E32 --------------------------------------------------------------------


def e32_relocations(image, offset):
    if offset == 0:
        return {}
    size = u32(image, offset)
    end = offset + 8 + size
    position = offset + 8
    result = {}
    while position < end:
        page, block = u32(image, position), u32(image, position + 4)
        for entry in struct.unpack_from(f"<{(block - 8) // 2}H", image, position + 8):
            if entry:
                result[page + (entry & 0xFFF)] = entry >> 12
        position += block
    return result


class E32:
    def __init__(self, image):
        require(len(image) > HEADER_SIZE and image[16:20] == b"EPOC", "not an E32 image")
        require(u32(image, 0) == UID_EXE, "the input E32 image must be an executable")
        require(u32(image, 28) == 0, "the input E32 image must be uncompressed")
        self.image = image
        self.uid3 = u32(image, 8)
        self.flags = u32(image, 44)
        self.code_size = u32(image, 48)
        self.data_size = u32(image, 52)
        self.heap_min, self.heap_max, self.stack = struct.unpack_from("<III", image, 56)
        self.bss_size = u32(image, 68)
        self.entry = u32(image, 72)
        self.code_base = u32(image, 76)
        self.data_base = u32(image, 80)
        self.capabilities = struct.unpack_from("<Q", image, 136)[0]
        self.cpu = u16(image, 122)
        code_offset = u32(image, 100)
        self.code = image[code_offset:code_offset + self.code_size]
        data_offset = u32(image, 104)
        self.data = image[data_offset:data_offset + self.data_size] if self.data_size else b""
        # Code addresses (not offsets) of imported words: (DLL, word).
        self.imports = {}
        import_offset = u32(image, 108)
        position = import_offset + 4
        for _ in range(u32(image, 84)):
            name_offset, count = u32(image, position), u32(image, position + 4)
            start = import_offset + name_offset
            dll = image[start:image.index(b"\0", start)].decode()
            for code_offset_ in struct.unpack_from(f"<{count}I", image, position + 8):
                self.imports[self.code_base + code_offset_] = (dll, u32(self.code, code_offset_))
            position += 8 + 4 * count
        self.code_relocations = {self.code_base + k: v for k, v in e32_relocations(image, u32(image, 112)).items()}
        self.data_relocations = e32_relocations(image, u32(image, 116))


def header_crc(header):
    header = bytearray(header)
    struct.pack_into("<I", header, 20, CRC_INITIALISER)
    crc = 0
    table = header_crc.table
    for byte in header:
        crc = (crc >> 8) ^ table[(crc ^ byte) & 0xFF]
    return crc


def _crc_table():
    table = []
    for byte in range(256):
        crc = byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xEDB88320 if crc & 1 else 0)
        table.append(crc)
    return table


header_crc.table = _crc_table()


def crc_ccitt(data):
    crc = 0
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else (crc << 1)
            crc &= 0xFFFF
    return crc


def uid_checksum(uids):
    return crc_ccitt(uids[0:12:2]) | (crc_ccitt(uids[1:12:2]) << 16)


def encode_relocations(entries):
    """entries: {offset: 1 (code target) or 2 (data target)}."""
    offsets = sorted(entries)
    out = bytearray(8)
    first = 0
    while first < len(offsets):
        page = offsets[first] & ~0xFFF
        end = first
        while end < len(offsets) and (offsets[end] & ~0xFFF) == page:
            require(offsets[end] % 4 == 0, "unaligned relocation")
            end += 1
        count = end - first
        block = 8 + ((count + 1) // 2) * 4
        chunk = bytearray(block)
        struct.pack_into("<II", chunk, 0, page, block)
        for i in range(first, end):
            struct.pack_into("<H", chunk, 8 + (i - first) * 2,
                             (0x2000 if entries[offsets[i]] == 2 else 0x1000) | (offsets[i] & 0xFFF))
        out += chunk
        first = end
    struct.pack_into("<II", out, 0, len(out) - 8, len(offsets))
    return bytes(out)


def encode_imports(imports):
    """imports: {dll name: [code offsets]}, written in name order."""
    blocks = sorted(imports.items())
    size = 4 + sum(8 + 4 * len(offsets) for _, offsets in blocks)
    out = bytearray(size)
    position = 4
    for name, offsets in blocks:
        struct.pack_into("<II", out, position, len(out), len(offsets))
        struct.pack_into(f"<{len(offsets)}I", out, position + 8, *sorted(offsets))
        position += 8 + 4 * len(offsets)
        out += name.encode() + b"\0"
    out += b"\0" * ((4 - len(out) % 4) % 4)
    struct.pack_into("<I", out, 0, len(out))
    return bytes(out)


def write_e32(*, dll, uid3, flags, code, code_base, entry, data=b"", data_base=0, bss=0,
              imports, code_relocations, data_relocations=None, capabilities, cpu,
              heap=(0x1000, 0x100000), stack=0x10000, exports=None, export_offset=0,
              exception_descriptor=0, version=0x000A0000):
    header = bytearray(HEADER_SIZE)
    struct.pack_into("<III", header, 0, UID_DLL if dll else UID_EXE, UID2_DLL if dll else 0, uid3)
    struct.pack_into("<I", header, 12, uid_checksum(header[:12]))
    header[16:20] = b"EPOC"
    struct.pack_into("<I", header, 24, version)
    header[33] = 1
    struct.pack_into("<I", header, 44, flags)
    struct.pack_into("<IIIII", header, 48, len(code), len(data), heap[0], heap[1], stack)
    struct.pack_into("<IIII", header, 68, bss, entry, code_base, data_base)
    struct.pack_into("<I", header, 96, len(code))
    struct.pack_into("<I", header, 100, HEADER_SIZE)
    struct.pack_into("<HH", header, 120, 350, cpu)
    struct.pack_into("<I", header, 128, uid3)
    struct.pack_into("<Q", header, 136, capabilities)
    struct.pack_into("<I", header, 144, exception_descriptor)
    image = bytearray(header) + code
    if exports:
        struct.pack_into("<II", image, 88, HEADER_SIZE + export_offset, exports)
    if data:
        struct.pack_into("<I", image, 104, len(image))
        image += data
    if imports:
        struct.pack_into("<I", image, 84, len(imports))
        struct.pack_into("<I", image, 108, len(image))
        image += encode_imports(imports)
    if code_relocations:
        struct.pack_into("<I", image, 112, len(image))
        image += encode_relocations(code_relocations)
    if data_relocations:
        struct.pack_into("<I", image, 116, len(image))
        image += encode_relocations(data_relocations)
    struct.pack_into("<I", image, 124, len(image) - HEADER_SIZE)
    struct.pack_into("<I", image, 20, header_crc(image[:HEADER_SIZE]))
    return bytes(image)


# --- The split --------------------------------------------------------------


class Piece:
    """One DLL: a contiguous range of .text, plus (for the first) the rest of
    the read-only image."""

    def __init__(self, index, start, end):
        self.index = index
        self.start = start
        self.end = end
        self.regions = [(start, end)]  # original ranges placed in this DLL
        self.stubs = {"head": {}, "tail": {}}  # key -> address, per island
        self.base = 0
        self.code = None
        self.relocations = {}
        self.imports = defaultdict(list)
        self.name = ""
        self.uid3 = 0


class Splitter:
    def __init__(self, elf, e32, *, max_code, prefix, uid_base, log):
        self.elf = elf
        self.e32 = e32
        self.max_code = max_code
        self.prefix = prefix
        self.uid_base = uid_base
        self.log = log
        self.text = elf.section(".text")
        self.text_lo = self.text["addr"]
        self.text_hi = self.text_lo + self.text["size"]
        self.plt = elf.by_name.get(".plt")
        self.rodata = elf.section(".rodata")
        self.extab = elf.by_name.get(".ARM.extab")
        # A program built without C++ exceptions (jsc) has no unwind index;
        # its DLLs still get the two CANTUNWIND entries of their own.
        self.exidx = elf.by_name.get(".ARM.exidx") or {"addr": None, "size": 0}
        self.got = elf.by_name.get(".got")
        self.image_lo = e32.code_base
        self.image_hi = e32.code_base + e32.code_size
        require(e32.data_base == elf.section(".data")["addr"], "E32 and ELF data bases differ")
        # The E32 code must be the ELF's read-only segment, apart from imports.
        elf_code = elf.data[self.image_lo:self.image_hi]
        require(self.section_file_matches(), "ELF file offsets are not its addresses")
        mismatched = 0
        for offset in range(0, len(elf_code), 4):
            if elf_code[offset:offset + 4] != e32.code[offset:offset + 4]:
                address = self.image_lo + offset
                if address not in e32.imports:
                    mismatched += 1
        require(mismatched == 0, f"E32 code differs from the ELF in {mismatched} words besides imports")
        self.code = bytearray(e32.code)

    def section_file_matches(self):
        for section in self.elf.sections:
            if section["flags"] & 2 and section["type"] != 8 and section["addr"] < 0x20000000:
                if section["addr"] and section["offset"] != section["addr"]:
                    return False
        return True

    # Original image access.
    def word(self, address):
        return u32(self.code, address - self.image_lo)

    def half(self, address):
        return u16(self.code, address - self.image_lo)

    # -- analysis --
    def load_symbols(self):
        text_index = self.text["index"]
        self.functions = []
        self.thunks = []
        self.short_thunks = []
        mapping = []
        self.symbol_value = {}
        self.symbol_name = {}
        symbols = self.elf.symbols()
        for index, (value, size, kind, shndx, name) in enumerate(symbols):
            self.symbol_value[index] = value
            self.symbol_name[index] = name
            if shndx != text_index:
                continue
            if name in (b"$a", b"$t", b"$d") or name.startswith((b"$a.", b"$t.", b"$d.")):
                mapping.append((value, name[1:2].decode()))
            elif kind == 2:
                start = value & ~1
                if name.startswith(b"__ARMv4PILongBXThunk_"):
                    # A thunk whose target is in range is a single B.
                    (self.short_thunks if size == 4 else self.thunks).append(start)
                elif size:
                    self.functions.append((start, start + size))
                if name.startswith(b"__") and b"Thunk_" in name and not name.startswith(b"__ARMv4PILongBXThunk_"):
                    raise SplitError(f"unsupported linker thunk {name.decode()}")
        mapping.sort()
        self.mapping_addresses = [a for a, _ in mapping]
        self.mapping_kinds = [k for _, k in mapping]
        self.functions.sort()

    def code_kind(self, address):
        index = bisect.bisect_right(self.mapping_addresses, address) - 1
        return self.mapping_kinds[index] if index >= 0 else "d"

    def load_exidx(self):
        lo = self.exidx["addr"]
        self.exidx_entries = []
        for place in range(lo or 0, (lo or 0) + self.exidx["size"], 8):
            function = prel31(self.word(place), place)
            self.exidx_entries.append((function, place, self.word(place + 4)))
        self.exidx_functions = [f for f, _, _ in self.exidx_entries]
        require(self.exidx_functions == sorted(self.exidx_functions), ".ARM.exidx is not sorted")

    def load_relocations(self):
        self.branches = []  # (place, type)
        self.pc_relative = []  # (place, type, symbol)
        for place, kind, symbol in self.elf.relocations(".rel.text"):
            if kind in (R_ARM_CALL, R_ARM_JUMP24, R_ARM_THM_CALL):
                self.branches.append((place, kind))
            elif kind in (R_ARM_REL32, R_ARM_GOT_PREL):
                self.pc_relative.append((place, kind, symbol))
            elif kind != R_ARM_ABS32:
                raise SplitError(f"unsupported .text relocation type {kind} at {place:#x}")
        for thunk in self.short_thunks:
            require((self.word(thunk) & 0xFF000000) == 0xEA000000, f"unexpected short thunk at {thunk:#x}")
            self.branches.append((thunk, R_ARM_JUMP24))
        self.branch_places = {place for place, _ in self.branches}
        self.extab_relocations = []
        for place, kind, symbol in self.elf.relocations(".rel.ARM.extab"):
            require(kind in (R_ARM_PREL31, R_ARM_TARGET2), f"unsupported .ARM.extab relocation {kind}")
            self.extab_relocations.append((place, kind))
        # Symbols name what an absolute word means where its value alone does
        # not: __exidx_end and __init_array_start are the same address.
        self.word_symbols = {}
        for name in (".rel.rodata", ".rel.init_array", ".rel.fini_array", ".rel.data"):
            for place, kind, symbol in self.elf.relocations(name):
                require(kind in (R_ARM_ABS32, R_ARM_TARGET1), f"unsupported {name} relocation {kind} at {place:#x}")
                self.word_symbols[place] = self.symbol_name[symbol]
        got = self.elf.by_name.get(".got")
        if got:
            for place, kind, symbol in self.pc_relative:
                if kind != R_ARM_GOT_PREL:
                    continue
                for slot in range(got["addr"], got["addr"] + got["size"], 4):
                    if self.word(slot) == self.symbol_value[symbol]:
                        self.word_symbols.setdefault(slot, self.symbol_name[symbol])

    # -- cuts --
    def cut_candidates(self):
        # Without an unwind index any function start will do.
        exidx = set(self.exidx_functions) if self.exidx_functions else None
        candidates = []
        previous_end = 0
        for start, end in self.functions:
            if (start >= previous_end and start % 4 == 0 and (exidx is None or start in exidx)
                    and self.code_kind(start) in "at"):
                candidates.append(start)
            previous_end = max(previous_end, end)
        return candidates

    def check_cut(self, cut, window=0x100000):
        """No unrelocated PC-relative instruction may reach across a cut."""
        lo, hi = max(self.text_lo, cut - window), min(self.text_hi, cut + window)
        index = bisect.bisect_right(self.mapping_addresses, lo) - 1
        position = lo
        while position < hi:
            kind = self.mapping_kinds[index] if index >= 0 else "d"
            next_start = self.mapping_addresses[index + 1] if index + 1 < len(self.mapping_addresses) else hi
            end = min(next_start, hi)
            if kind == "a":
                start = (position + 3) & ~3
                words = struct.unpack_from(f"<{(end - start) // 4}I", self.code, start - self.image_lo) if end > start else ()
                for i, instruction in enumerate(words):
                    place = start + 4 * i
                    target = None
                    if (instruction & 0x0E000000) == 0x0A000000:
                        target = place + 8 + sign_extend(instruction & 0xFFFFFF, 24) * 4
                        if (instruction >> 28) == 0xF:
                            target += (instruction >> 23) & 2
                        if place in self.branch_places:
                            target = None
                    elif (instruction & 0x0F7F0000) == 0x051F0000 and (instruction >> 28) != 0xF:
                        offset = instruction & 0xFFF
                        target = place + 8 + (offset if instruction & 0x00800000 else -offset)
                    elif (instruction & 0x0FFF0000) in (0x028F0000, 0x024F0000) and (instruction >> 28) != 0xF:
                        value = arm_immediate(instruction)
                        target = place + 8 + (value if (instruction & 0x00F00000) == 0x00800000 else -value)
                    elif (instruction & 0x0F3F0E00) == 0x0D1F0A00:  # vldr/vstr, pc-relative
                        offset = (instruction & 0xFF) * 4
                        target = place + 8 + (offset if instruction & 0x00800000 else -offset)
                    if target is not None and (place < cut) != (target < cut):
                        raise SplitError(f"cut {cut:#x}: unrelocated reference from {place:#x} to {target:#x}")
            elif kind == "t":
                start = (position + 1) & ~1
                count = (end - start) // 2
                halves = struct.unpack_from(f"<{count}H", self.code, start - self.image_lo) if count > 0 else ()
                i = 0
                while i < count:
                    half = halves[i]
                    place = start + 2 * i
                    target = None
                    if (half & 0xF800) == 0xF000 and i + 1 < count:
                        second = halves[i + 1]
                        if (second & 0xE800) == 0xE800:
                            offset = sign_extend(((half & 0x7FF) << 12) | ((second & 0x7FF) << 1), 23)
                            target = place + 4 + offset
                            if place in self.branch_places:
                                target = None
                        i += 1
                    elif (half & 0xF800) == 0xE000:
                        target = place + 4 + sign_extend(half & 0x7FF, 11) * 2
                    elif (half & 0xF000) == 0xD000 and (half & 0x0F00) < 0x0E00:
                        target = place + 4 + sign_extend(half & 0xFF, 8) * 2
                    elif (half & 0xF800) in (0x4800, 0xA000):
                        target = ((place + 4) & ~3) + (half & 0xFF) * 4
                    if target is not None and (place < cut) != (target < cut):
                        raise SplitError(f"cut {cut:#x}: unrelocated reference from {place:#x} to {target:#x}")
                    i += 1
            position = end
            index += 1

    def tail_regions(self):
        """The read-only image after .text that the first DLL carries: .rodata
        and the small sections the code refers to by absolute address."""
        regions = []
        lo = self.rodata["addr"]
        hi = lo + self.rodata["size"]
        for name in (".ARM.extab", ".eh_frame"):
            section = self.elf.by_name.get(name)
            if section and section["addr"] == hi:
                hi += section["size"]
        regions.append((lo, hi))
        init = self.elf.by_name.get(".init_array")
        fini = self.elf.by_name.get(".fini_array")
        if init:
            end = init["addr"] + init["size"]
            if fini and fini["addr"] == end:
                end += fini["size"]
            regions.append((init["addr"], end))
        if self.got:
            regions.append((self.got["addr"], self.got["addr"] + self.got["size"]))
        return regions

    def choose_cuts(self):
        candidates = self.cut_candidates()
        tail = sum(hi - lo for lo, hi in self.tail_regions())
        # The first piece holds the runtime code that refers to .rodata and the
        # GOT with PC-relative words: those cannot cross into another DLL.
        first_end = max([place + 4 for place, _, _ in self.pc_relative] + [self.text_lo + 4])
        budget0 = self.max_code - tail - self.overhead(0x100000)
        require(first_end - self.text_lo <= budget0,
                f"the read-only data ({tail:#x} bytes) and its code do not fit in {self.max_code:#x}")
        index = bisect.bisect_left(candidates, first_end)
        # Take as much code as fits beside the read-only data.
        limit = bisect.bisect_right(candidates, self.text_lo + budget0) - 1
        require(index <= limit, "no cut fits the first DLL")
        cuts = [self.text_lo]
        cut = candidates[limit]
        while True:
            try:
                self.check_cut(cut)
                break
            except SplitError as error:
                self.log(f"  skip: {error}")
                limit -= 1
                require(limit >= index, "no safe cut for the first DLL")
                cut = candidates[limit]
        cuts.append(cut)
        remaining = self.text_hi - cut
        budget = self.max_code - self.overhead(self.max_code)
        count = -(-remaining // budget)
        for piece in range(1, count):
            target = cut + -(-(self.text_hi - cut) // (count - piece + 1))
            position = bisect.bisect_right(candidates, min(target, cut + budget)) - 1
            while True:
                require(candidates[position] > cut, "no cut fits a DLL")
                try:
                    self.check_cut(candidates[position])
                    break
                except SplitError as error:
                    self.log(f"  skip: {error}")
                    position -= 1
            cut = candidates[position]
            cuts.append(cut)
        require(self.text_hi - cut <= budget, "the last DLL is too large; lower the size or allow more DLLs")
        cuts.append(self.text_hi)
        return cuts

    def overhead(self, text):
        # Unwind index (~1.8% of code), exception table copy, stubs, header bits.
        return int(text * 0.025) + (self.extab["size"] if self.extab else 0) + 0x30000

    # -- placement --
    def piece_of(self, address):
        index = bisect.bisect_right(self.cuts, address) - 1
        if 0 <= index < len(self.pieces):
            return self.pieces[index]
        return None

    def build_pieces(self):
        self.cuts = self.choose_cuts()
        self.pieces = [Piece(i, self.cuts[i], self.cuts[i + 1]) for i in range(len(self.cuts) - 1)]
        for piece in self.pieces:
            piece.name = f"{self.prefix}{piece.index}.dll"
            piece.uid3 = self.uid_base + piece.index
        self.pieces[0].regions += self.tail_regions()

    def plt_slot(self, entry):
        """The GOT slot an LLD PLT entry jumps through."""
        first, second, third = self.word(entry), self.word(entry + 4), self.word(entry + 8)
        if (first & 0xFFFFF000) == 0xE28FC000 and (second & 0xFFFFF000) == 0xE28CC000 and (third & 0xFFFFF000) == 0xE5BCF000:
            return entry + 8 + arm_immediate(first) + arm_immediate(second) + (third & 0xFFF)
        raise SplitError(f"unrecognised PLT entry at {entry:#x}")

    def in_plt(self, address):
        return self.plt is not None and self.plt["addr"] + 32 <= address < self.plt["addr"] + self.plt["size"]

    def plt_import(self, address):
        entry = self.plt["addr"] + 32 + ((address - self.plt["addr"] - 32) // 16) * 16
        slot = self.plt_slot(entry)
        require(slot in self.e32.imports, f"PLT entry {entry:#x} has no import")
        return self.e32.imports[slot]

    def decode_branch(self, place, kind):
        """Returns (target, target_is_thumb)."""
        if kind == R_ARM_THM_CALL:
            first, second = self.half(place), self.half(place + 2)
            require((first & 0xF800) == 0xF000 and (second & 0xE800) == 0xE800, f"not a Thumb BL at {place:#x}")
            offset = sign_extend(((first & 0x7FF) << 12) | ((second & 0x7FF) << 1), 23)
            if second & 0x1000:
                return place + 4 + offset, True
            return (place + 4 + offset) & ~3, False
        instruction = self.word(place)
        require((instruction & 0x0E000000) == 0x0A000000, f"not an ARM branch at {place:#x}")
        target = place + 8 + sign_extend(instruction & 0xFFFFFF, 24) * 4
        if (instruction >> 28) == 0xF:
            return target + ((instruction >> 23) & 2), True
        return target, False

    def analyse_branches(self):
        """Finds the branches that cross a cut and the stubs each piece needs."""
        self.crossing = []  # (piece, place, kind, island, key)
        for place, kind in self.branches:
            piece = self.piece_of(place)
            require(piece is not None, f"branch outside .text at {place:#x}")
            target, thumb = self.decode_branch(place, kind)
            if piece.start <= target < piece.end:
                continue
            if self.in_plt(target):
                key = ("import",) + self.plt_import(target)
            else:
                other = self.piece_of(target)
                require(other is not None, f"branch from {place:#x} to {target:#x} leaves .text")
                key = ("piece", other.index, target | (1 if thumb else 0))
            island = "head"
            # The first piece's tail island comes after the read-only data.
            beyond = sum(hi - lo for lo, hi in piece.regions[1:])
            if kind == R_ARM_THM_CALL and place - piece.start > piece.end + beyond - place:
                island = "tail"
            piece.stubs[island].setdefault(key, None)
            self.crossing.append((piece, place, kind, island, key))
        # LLD's long-branch thunks: ldr ip, [pc, #4]; add ip, pc, ip; bx ip; .word
        self.crossing_thunks = []
        for thunk in self.thunks:
            words = [self.word(thunk + 4 * i) for i in range(4)]
            require(words[:3] == [0xE59FC004, 0xE08FC00C, 0xE12FFF1C], f"unexpected thunk at {thunk:#x}")
            target = (thunk + 12 + words[3]) & 0xFFFFFFFF
            piece = self.piece_of(thunk)
            if piece.start <= (target & ~1) < piece.end:
                continue
            if self.in_plt(target):
                key = ("import",) + self.plt_import(target)
            else:
                other = self.piece_of(target & ~1)
                require(other is not None, f"thunk {thunk:#x} leaves .text")
                key = ("piece", other.index, target)
            self.crossing_thunks.append((piece, thunk, key))
        # Exception tables: personality routines in another piece are reached
        # through a stub.
        self.personalities = set()
        for place, kind in self.extab_relocations:
            if kind == R_ARM_PREL31:
                self.personalities.add(prel31(self.word(place), place))
        for piece in self.pieces:
            for target in self.personalities:
                if not (piece.start <= (target & ~1) < piece.end):
                    piece.stubs["head"].setdefault(self.personality_key(target), None)

    def personality_key(self, target):
        if self.in_plt(target):
            return ("import",) + self.plt_import(target)
        other = self.piece_of(target & ~1)
        require(other is not None, f"personality routine {target:#x} is outside .text")
        return ("piece", other.index, target)

    def layout(self):
        """Gives every piece its addresses. Each piece's .text keeps its
        original addresses; everything else is placed around it."""
        self.moved = []  # (lo, hi, piece, new lo) for regions placed elsewhere
        got_size = self.got["size"] if self.got else 0
        extab_size = self.extab["size"] if self.extab else 0
        for piece in self.pieces:
            head = 24 + 8 * len(piece.stubs["head"])
            piece.base = (piece.start - head) & ~0xFFF
            require(piece.base > 0, "a DLL would start at address zero")
            position = piece.base
            piece.entry = position
            position += 24
            for key in piece.stubs["head"]:
                piece.stubs["head"][key] = position
                position += 8
            require(position <= piece.start, "head island overlaps the code")
            position = piece.end
            for lo, hi in piece.regions[1:]:
                # Keep each region's alignment within a 4 KiB page.
                position = ((position + 0xFFF) & ~0xFFF) + (lo & 0xFFF)
                self.moved.append((lo, hi, piece, position))
                position += hi - lo
            position = (position + 7) & ~7
            piece.tail_island = position
            for key in piece.stubs["tail"]:
                piece.stubs["tail"][key] = position
                position += 8
            if piece.index == 0 and self.got:
                piece.got = next(new + self.got["addr"] - lo for lo, hi, owner, new in self.moved
                                 if owner is piece and lo == self.got["addr"])
            else:
                piece.got = position
                position += got_size
            piece.extab = position
            position += extab_size
            piece.exidx = position
            entries = sum(1 for f in self.exidx_functions if piece.start <= f < piece.end)
            position += 8 * (entries + 2)
            piece.exidx_end = position
            piece.descriptor = position
            position += 16
            piece.export_prefix = position - piece.base
            # One anchor for every 64 KiB of code, the export table included.
            piece.exports = -(-(piece.export_prefix + 4) // ANCHOR_SPACING)
            while -(-(piece.export_prefix + 4 + 4 * piece.exports) // ANCHOR_SPACING) > piece.exports:
                piece.exports += 1
            position += 4 + 4 * piece.exports
            piece.code_end = position
            require(piece.code_end - piece.base <= self.max_code,
                    f"{piece.name} has {piece.code_end - piece.base:#x} bytes of code, over {self.max_code:#x}")
        self.moved.sort()
        self.moved_starts = [lo for lo, _, _, _ in self.moved]

    def moved_address(self, address):
        index = bisect.bisect_right(self.moved_starts, address) - 1
        if index >= 0:
            lo, hi, piece, new = self.moved[index]
            if lo <= address <= hi:
                return new + address - lo
        return None

    def locate(self, value, symbol=None):
        """Where an original read-only address ends up: ("piece", piece, new
        address) or ("import", dll, word) for a PLT entry. The symbol, when
        known, settles an address where two regions meet."""
        address = value
        if self.in_plt(address):
            return ("import",) + self.plt_import(address)
        first = self.pieces[0]
        if address in (self.image_lo, self.image_lo + 1):  # __ro_base
            return ("piece", first, first.base + address - self.image_lo)
        if address == self.image_hi:  # __ro_end
            return ("piece", first, first.code_end)
        if address == self.exidx["addr"]:
            return ("piece", first, first.exidx)
        if address == self.exidx["addr"] + self.exidx["size"] and symbol in (None, b"__exidx_end"):
            return ("piece", first, first.exidx_end)
        if self.text_lo <= address < self.text_hi:
            return ("piece", self.piece_of(address), address)
        if address == self.text_hi:
            return ("piece", self.pieces[-1], address)
        moved = self.moved_address(address)
        if moved is not None:
            return ("piece", first, moved)
        raise SplitError(f"no placement for read-only address {value:#x}")

    def import_word(self, piece, address):
        offset = address - piece.base
        require(0 <= offset < piece.exports * ANCHOR_SPACING, f"{address:#x} is outside {piece.name}'s anchors")
        return (offset // ANCHOR_SPACING + 1) | ((offset % ANCHOR_SPACING) << 16)

    # -- emission --
    def emit(self):
        for piece in self.pieces:
            piece.code = bytearray(piece.code_end - piece.base)
        # Copy every region, then resolve its absolute words.
        absolute = sorted(set(self.e32.code_relocations) | set(self.e32.imports))
        for piece in self.pieces:
            for lo, hi in piece.regions:
                new = lo if (lo, hi) == piece.regions[0] else self.moved_address(lo)
                piece.code[new - piece.base:new - piece.base + hi - lo] = self.code[lo - self.image_lo:hi - self.image_lo]
                first = bisect.bisect_left(absolute, lo)
                last = bisect.bisect_left(absolute, hi)
                for address in absolute[first:last]:
                    self.put_word(piece, new + address - lo, address)
        self.emit_branches()
        self.emit_pc_relative()
        for piece in self.pieces:
            self.emit_entry(piece)
            self.emit_got(piece)
            self.emit_extab(piece)
            self.emit_exidx(piece)
            self.emit_descriptor(piece)
            self.emit_exports(piece)
            for island in ("head", "tail"):
                for key, address in piece.stubs[island].items():
                    self.put32(piece, address, STUB_LDR_PC)
                    self.put_reference(piece, address + 4, key)

    def put32(self, piece, address, value):
        struct.pack_into("<I", piece.code, address - piece.base, value & 0xFFFFFFFF)

    def put_reference(self, piece, address, key):
        """Writes an absolute reference: ("import", dll, word) or ("piece",
        index, original address)."""
        if key[0] == "import":
            self.put32(piece, address, key[2])
            piece.imports[key[1]].append(address - piece.base)
            piece.relocations.pop(address - piece.base, None)
            return
        location = self.locate(key[2])
        self.put_located(piece, address, location)

    def put_located(self, piece, address, location):
        if location[0] == "import":
            self.put32(piece, address, location[2])
            piece.imports[location[1]].append(address - piece.base)
            return
        _, target_piece, target = location
        if target_piece is piece:
            self.put32(piece, address, target)
            piece.relocations[address - piece.base] = 1
        else:
            self.put32(piece, address, self.import_word(target_piece, target))
            piece.imports[target_piece.name].append(address - piece.base)

    def put_word(self, piece, address, original):
        if original in self.e32.imports:
            dll, word = self.e32.imports[original]
            self.put32(piece, address, word)
            piece.imports[dll].append(address - piece.base)
            return
        kind = self.e32.code_relocations[original]
        value = self.word(original)
        if kind == 2:
            self.put32(piece, address, self.data_address(value))
            return
        self.put_located(piece, address, self.locate(value, self.word_symbols.get(original)))

    def data_address(self, value):
        data_base = self.e32.data_base
        require(data_base <= value <= data_base + self.e32.data_size + self.e32.bss_size,
                f"{value:#x} is outside the data")
        return EXE_DATA_BASE + value - data_base

    def emit_branches(self):
        for piece, place, kind, island, key in self.crossing:
            stub = piece.stubs[island][key]
            offset = piece.code
            if kind == R_ARM_THM_CALL:
                delta = stub - ((place + 4) & ~3)
                require(-(1 << 22) <= delta < (1 << 22), f"Thumb call at {place:#x} cannot reach its stub")
                struct.pack_into("<HH", offset, place - piece.base,
                                 0xF000 | ((delta >> 12) & 0x7FF), 0xE800 | ((delta >> 1) & 0x7FF))
            else:
                instruction = self.word(place)
                delta = stub - (place + 8)
                require(-(1 << 25) <= delta < (1 << 25), f"branch at {place:#x} cannot reach its stub")
                if (instruction >> 28) == 0xF:  # BLX to Thumb: BL to the stub, which interworks
                    instruction = 0xEB000000
                self.put32(piece, place, (instruction & 0xFF000000) | ((delta >> 2) & 0xFFFFFF))
        for piece, thunk, key in self.crossing_thunks:
            self.put32(piece, thunk, 0xE59FC000)  # ldr ip, [pc]
            self.put32(piece, thunk + 4, 0xE12FFF1C)  # bx ip
            self.put_reference(piece, thunk + 8, key)
            self.put32(piece, thunk + 12, 0xE1A00000)

    def emit_pc_relative(self):
        first = self.pieces[0]
        for place, kind, symbol in self.pc_relative:
            require(first.start <= place < first.end, f"PC-relative word outside the first DLL at {place:#x}")
            value = self.word(place)
            if kind == R_ARM_REL32:
                target = self.symbol_value[symbol]
            else:
                # The word is relative to a later instruction, so find the
                # slot by its content; the GOT moves as one block.
                require(self.got is not None, "GOT_PREL without a GOT")
                slots = [self.got["addr"] + offset for offset in range(0, self.got["size"], 4)
                         if self.word(self.got["addr"] + offset) == self.symbol_value[symbol]]
                require(slots, f"GOT_PREL at {place:#x}: no GOT slot holds its symbol")
                target = slots[0]
            location = self.locate(target)
            require(location[0] == "piece" and location[1] is first, f"PC-relative target {target:#x} moved away")
            self.put32(first, place, value + location[2] - target)

    def emit_entry(self, piece):
        # The EKA2 entry-point form; a DLL entry returns KErrNone.
        for i, word in enumerate((0xE31F0000, 0xE3540001, 0xEA000000, 0, 0xE3A00000, 0xE12FFF1E)):
            self.put32(piece, piece.entry + 4 * i, word)

    def emit_got(self, piece):
        if not self.got or piece.index == 0:
            return
        for offset in range(0, self.got["size"], 4):
            original = self.got["addr"] + offset
            if original in self.e32.code_relocations or original in self.e32.imports:
                self.put_word(piece, piece.got + offset, original)

    def emit_extab(self, piece):
        if not self.extab:
            return
        lo = self.extab["addr"]
        size = self.extab["size"]
        piece.code[piece.extab - piece.base:piece.extab - piece.base + size] = self.code[lo - self.image_lo:lo - self.image_lo + size]
        for place, kind in self.extab_relocations:
            new_place = piece.extab + place - lo
            value = self.word(place)
            if kind == R_ARM_PREL31:
                target = prel31(value, place)
                if piece.start <= (target & ~1) < piece.end:
                    new_target = target
                else:
                    new_target = piece.stubs["head"][self.personality_key(target)]
                self.put32(piece, new_place, encode_prel31(new_target, new_place, value & 0x80000000))
            else:  # R_ARM_TARGET2: GOT-relative
                slot = (place + value) & 0xFFFFFFFF
                require(self.got and self.got["addr"] <= slot < self.got["addr"] + self.got["size"],
                        f"TARGET2 at {place:#x} does not point into the GOT")
                self.put32(piece, new_place, piece.got + slot - self.got["addr"] - new_place)

    def emit_exidx(self, piece):
        lo = self.extab["addr"] if self.extab else 0
        hi = lo + (self.extab["size"] if self.extab else 0)
        position = piece.exidx

        def entry(function, word):
            nonlocal position
            self.put32(piece, position, encode_prel31(function, position))
            self.put32(piece, position + 4, word)
            position += 8

        entry(piece.base, EXIDX_CANTUNWIND)
        first = bisect.bisect_left(self.exidx_functions, piece.start)
        last = bisect.bisect_left(self.exidx_functions, piece.end)
        for function, place, word in self.exidx_entries[first:last]:
            if word != EXIDX_CANTUNWIND and not word & 0x80000000:
                table = prel31(word, place + 4)
                require(lo <= table < hi, f"exidx entry at {place:#x} points outside .ARM.extab")
                word = encode_prel31(piece.extab + table - lo, position + 4)
            entry(function, word)
        entry(piece.tail_island, EXIDX_CANTUNWIND)
        require(position == piece.exidx_end, "exidx size mismatch")

    def emit_descriptor(self, piece):
        address = piece.descriptor
        for i, value in enumerate((piece.exidx, piece.exidx_end, piece.base + 1, piece.code_end)):
            self.put32(piece, address + 4 * i, value)
            piece.relocations[address + 4 * i - piece.base] = 1

    def emit_exports(self, piece):
        address = piece.base + piece.export_prefix
        self.put32(piece, address, piece.exports)
        for i in range(piece.exports):
            self.put32(piece, address + 4 + 4 * i, piece.base + i * ANCHOR_SPACING)
            piece.relocations[address + 4 + 4 * i - piece.base] = 1

    def dll_images(self):
        flags = (self.e32.flags & 0x00F00000) | E32_FLAGS | 1
        images = {}
        for piece in self.pieces:
            for offsets in piece.imports.values():
                offsets.sort()
            images[piece.name] = write_e32(
                dll=True, uid3=piece.uid3, flags=flags, code=bytes(piece.code), code_base=piece.base,
                entry=piece.entry - piece.base, imports=dict(piece.imports),
                code_relocations=piece.relocations, capabilities=self.e32.capabilities, cpu=self.e32.cpu,
                exports=piece.exports, export_offset=piece.export_prefix + 4,
                exception_descriptor=(piece.descriptor - piece.base) | 1)
        return images

    def exe_image(self):
        """The stub executable: all data, a patcher for the data's pointers to
        code, and a jump to the original entry point."""
        e32 = self.e32
        data = bytearray(e32.data)
        data_relocations = {}
        patches = []  # (data offset, key)
        for offset, kind in sorted(e32.data_relocations.items()):
            value = u32(data, offset)
            if kind == 2:
                struct.pack_into("<I", data, offset, self.data_address(value))
                data_relocations[offset] = 2
            else:
                struct.pack_into("<I", data, offset, 0)
                patches.append((offset, self.locate(value, self.word_symbols.get(e32.data_base + offset))))
        base = 0x8000
        code = []
        relocations = {}
        imports = defaultdict(list)
        entry_location = self.locate(self.image_lo + e32.entry)
        # Literal addresses are filled once the code is laid out.
        program = [
            0xE31F0000,  # 00 EKA2 entry marker
            0xE3540001,  # 04 cmp r4, #1
            0xEA000000,  # 08 b 10
            0x00000000,  # 0c
            0xE3540000,  # 10 cmp r4, #0: only the process's first thread patches
            0x1A00000D,  # 14 bne 50
            0xE92D500F,  # 18 push {r0-r3, r12, lr}
            0xE59F0034,  # 1c ldr r0, [pc, #52] -> 58: where the kernel put the data
            0xE3A01501,  # 20 mov r1, #0x400000
            0xE1500001,  # 24 cmp r0, r1
            0x13A00000,  # 28 movne r0, #0: the DLLs need the data at 0x400000,
            0x15800000,  # 2c strne r0, [r0]  so stop here (KERN-EXEC 3) if not
            0xE59F0028,  # 30 ldr r0, [pc, #40] -> 60: the table
            0xE59F1028,  # 34 ldr r1, [pc, #40] -> 64: its length
            0xE8B0000C,  # 38 ldmia r0!, {r2, r3}
            0xE5823000,  # 3c str r3, [r2]
            0xE2511001,  # 40 subs r1, r1, #1
            0x1AFFFFFB,  # 44 bne 38
            0xE8BD500F,  # 48 pop {r0-r3, r12, lr}
            0xE1A00000,  # 4c nop
            STUB_LDR_PC,  # 50 ldr pc, [pc, #-4]
            0,  # 54 the original entry point
            EXE_DATA_BASE,  # 58 data base (data relocation)
            0,  # 5c
            0,  # 60 table address (code relocation)
            len(patches),  # 64
        ]
        require(patches, "the program has no pointers from data to code")
        code = program
        table = base + 4 * len(code)
        relocations[0x58] = 2
        code[0x60 // 4] = table
        relocations[0x60] = 1

        def reference(index, location):
            if location[0] == "import":
                code[index] = location[2]
                imports[location[1]].append(4 * index)
            else:
                _, target_piece, target = location
                code[index] = self.import_word(target_piece, target)
                imports[target_piece.name].append(4 * index)

        reference(0x54 // 4, entry_location)
        for offset, location in patches:
            code.append(EXE_DATA_BASE + offset)
            code.append(0)
            reference(len(code) - 1, location)
        # Import every DLL, so that each is loaded even if the EXE does not
        # refer to it directly.
        for piece in self.pieces:
            if piece.name not in imports:
                code.append(self.import_word(piece, piece.base))
                imports[piece.name].append(4 * (len(code) - 1))
        image = write_e32(
            dll=False, uid3=e32.uid3, flags=e32.flags, code=struct.pack(f"<{len(code)}I", *code),
            code_base=base, entry=0, data=bytes(data), data_base=EXE_DATA_BASE, bss=e32.bss_size,
            imports=dict(imports), code_relocations=relocations, data_relocations=data_relocations,
            capabilities=e32.capabilities, cpu=e32.cpu, heap=(e32.heap_min, e32.heap_max), stack=e32.stack,
            version=u32(e32.image, 24))
        return image, len(patches)

    def run(self):
        self.load_symbols()
        self.load_exidx()
        self.load_relocations()
        self.build_pieces()
        self.analyse_branches()
        self.layout()
        self.emit()
        return self.exe_image(), self.dll_images()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--elf", required=True, type=Path, help="the linked ELF, with --emit-relocs")
    parser.add_argument("--e32", required=True, type=Path, help="the E32 executable made from it")
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--prefix", required=True, help="DLL names: PREFIX0.dll, PREFIX1.dll, ...")
    parser.add_argument("--uid-base", required=True, type=lambda v: int(v, 0), help="UID3 of the first DLL")
    parser.add_argument("--max-code", type=lambda v: int(v, 0), default=0x800000,
                        help="most code bytes in one DLL (default 8 MiB)")
    args = parser.parse_args()
    log = lambda message: print(message, file=sys.stderr)
    try:
        splitter = Splitter(Elf(args.elf.read_bytes()), E32(args.e32.read_bytes()),
                            max_code=args.max_code, prefix=args.prefix, uid_base=args.uid_base, log=log)
        (exe, patches), dlls = splitter.run()
    except SplitError as error:
        sys.exit(f"split-e32: {error}")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    exe_path = args.output_dir / args.e32.name
    exe_path.write_bytes(exe)
    print(f"{exe_path.name}: {len(exe)} bytes, {patches} data pointers patched at startup")
    for piece in splitter.pieces:
        image = dlls[piece.name]
        (args.output_dir / piece.name).write_bytes(image)
        stubs = len(piece.stubs["head"]) + len(piece.stubs["tail"])
        print(f"{piece.name}: {len(image)} bytes, code {piece.code_end - piece.base:#x} "
              f"(.text {piece.start:#x}-{piece.end:#x}), {stubs} stubs, "
              f"{sum(len(v) for v in piece.imports.values())} imports, {len(piece.relocations)} relocations")


if __name__ == "__main__":
    main()
