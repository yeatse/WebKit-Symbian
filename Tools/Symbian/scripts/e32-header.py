#!/usr/bin/env python3
"""Sets memory limits and an optional floating-point requirement in E32.

Usage: e32-header.py IMAGE HEAP_MIN HEAP_MAX STACK [--fpu=vfpv2]

The SDK image flow writes the EKA2 defaults (4 KB to 1 MB heap, 64 KB stack),
far too small for JavaScriptCore. The header CRC is updated afterwards: a
phone's loader and installer reject an image whose header CRC does not match
(EKA2L1 does not check it).
"""
import argparse
import struct

HEADER_CRC = 0x14  # iHeaderCrc
HEAP_SIZE_MIN = 0x38  # iHeapSizeMin, then iHeapSizeMax and iStackSize
CODE_OFFSET = 0x64  # iCodeOffset: the header's size
IMAGE_FLAGS = 0x2C  # iFlags
HARDWARE_FLOATING_POINT_MASK = 0x00F00000
HARDWARE_FLOATING_POINT_VFPV2 = 0x00100000
CRC_INITIALISER = 0xC90FDAA2  # KImageCrcInitialiser, in place of the CRC


def header_crc(image):
    """Symbian's Mem::Crc32 of the header: reflected CRC-32 from zero, with no
    final complement, computed with the CRC field set to its initialiser."""
    header = bytearray(image[: struct.unpack_from("<I", image, CODE_OFFSET)[0]])
    struct.pack_into("<I", header, HEADER_CRC, CRC_INITIALISER)
    crc = 0
    for byte in header:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xEDB88320 if crc & 1 else 0)
    return crc


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("image")
for name in ("heap_min", "heap_max", "stack"):
    parser.add_argument(name, type=lambda value: int(value, 0))
parser.add_argument("--fpu", choices=("vfpv2",), help="declare the image's hardware FP requirement")
args = parser.parse_args()
path = args.image
image = bytearray(open(path, "rb").read())
if len(image) < CODE_OFFSET + 4 or image[0x10:0x14] != b"EPOC":
    parser.error(f"{path}: not an E32 image")
header_size = struct.unpack_from("<I", image, CODE_OFFSET)[0]
if not CODE_OFFSET + 4 <= header_size <= len(image):
    parser.error(f"{path}: invalid E32 header size")
struct.pack_into("<III", image, HEAP_SIZE_MIN, args.heap_min, args.heap_max, args.stack)
if args.fpu == "vfpv2":
    # The ARMv6 engine and generated JIT code use VFPv2 with base AAPCS.
    # Static-library attributes can be absent from the final ELF, and JIT
    # instructions have no ELF attributes at all, so declare this explicitly.
    flags = struct.unpack_from("<I", image, IMAGE_FLAGS)[0]
    flags = (flags & ~HARDWARE_FLOATING_POINT_MASK) | HARDWARE_FLOATING_POINT_VFPV2
    struct.pack_into("<I", image, IMAGE_FLAGS, flags)
struct.pack_into("<I", image, HEADER_CRC, header_crc(image))
open(path, "wb").write(image)
