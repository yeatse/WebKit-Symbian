#!/usr/bin/env python3
"""Reject portable CRT/heap dependencies in ROM-routed native MMF patches."""

import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

ALLOWED_DLLS = {
    "euser.dll",
    "efsrv.dll",
    "ws32.dll",
    "gdi.dll",
    "mediaclient.dll",
    "drtaeabi.dll",
}
# SDK compiler-rt arithmetic helpers used by native video scaling. These do
# not own allocation, TLS, process lifetime, global constructors or OS state.
ARITHMETIC = {
    "aeabi_fcmp.S.obj",
    "comparesf2.S.obj",
    "muldf3.S.obj",
    "divdf3.S.obj",
    "dnan2.c.obj",
    "dnorm2.c.obj",
    "dunder.c.obj",
    "aeabi_dcmp.S.obj",
    "cmpdf2.S.obj",
    "gedf2.S.obj",
    "fixdfsi.S.obj",
    "floatsidf.S.obj",
    "extendsfdf2.S.obj",
    "clzsi2.c.obj",
}


def verify(build, target):
    image = (build / (target + ".dll")).read_bytes()
    word = lambda offset: struct.unpack_from("<I", image, offset)[0]
    if image[16:20] != b"EPOC" or word(28) != 0:
        raise ValueError("Expected an uncompressed E32 DLL")
    if word(52) or word(68):
        raise ValueError("Native patch unexpectedly owns writable data/BSS")
    if not word(144) & 1:
        raise ValueError("Native patch lost its leave/unwind descriptor")
    imports = []
    base = word(108)
    end = base + word(base)
    position = base + 4
    for _ in range(word(84)):
        name = base + word(position)
        if not base <= name < end:
            raise ValueError("Import name outside its section")
        dll = image[name : image.index(b"\0", name, end)].decode("ascii")
        imports.append(dll)
        position += 8 + word(position + 4) * 4
    if not imports or not set(imports) <= ALLOWED_DLLS or position > end:
        raise ValueError(
            "Native patch imports non-ROM runtime libraries: " + repr(imports)
        )

    elf = (build / (target + "_elf.elf")).read_bytes()
    elf_word = lambda offset: struct.unpack_from("<I", elf, offset)[0]
    half = lambda offset: struct.unpack_from("<H", elf, offset)[0]
    if elf[:7] != b"\x7fELF\x01\x01\x01":
        raise ValueError("Expected the linked ARM ELF32 input")
    sections = [elf_word(32) + i * half(46) for i in range(half(48))]
    names = sections[half(50)]
    for section in sections:
        offset = elf_word(names + 16) + elf_word(section)
        name = elf[offset : elf.index(b"\0", offset)].decode("ascii")
        if name in {".init_array", ".fini_array"} and elf_word(section + 20):
            raise ValueError(
                "Native patch requires portable CRT initialization"
            )
    members = []
    for line in (
        (build / (target + "-extract.txt")).read_text().splitlines()[1:]
    ):
        fields = line.split("\t")
        if len(fields) != 3:
            raise ValueError("Unexpected LLD extraction record")
        member = re.search(
            r"libsymbian_guest_runtime\.a\(([^)]+)\)$", fields[1]
        )
        if member is None or member[1] not in ARITHMETIC:
            raise ValueError("Unexpected portable runtime member: " + fields[1])
        members.append(member[1])
    return {
        "target": target,
        "sha256": hashlib.sha256(image).hexdigest(),
        "bytes": len(image),
        "imports": imports,
        "data_bytes": word(52),
        "bss_bytes": word(68),
        "exception_descriptor_offset": word(144) & ~1,
        "stateless_arithmetic_members": sorted(set(members)),
        "portable_crt_heap_tls": False,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    args = parser.parse_args()
    results = [
        verify(args.build, target)
        for target in ("mediaclientaudio_general", "mediaclientvideo_v100")
    ]
    print(json.dumps({"status": "PASS", "images": results}, indent=2))


if __name__ == "__main__":
    main()
