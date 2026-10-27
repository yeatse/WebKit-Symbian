#!/usr/bin/env python3
"""Sets the heap and stack sizes in an E32 image header.

Usage: e32-header.py IMAGE HEAP_MIN HEAP_MAX STACK

The SDK image flow writes the EKA2 defaults (4 KB to 1 MB heap, 64 KB stack),
far too small for JavaScriptCore. The header CRC is updated afterwards: a
phone's loader and installer reject an image whose header CRC does not match
(EKA2L1 does not check it).
"""
import struct
import sys

HEADER_CRC = 0x14  # iHeaderCrc
HEAP_SIZE_MIN = 0x38  # iHeapSizeMin, then iHeapSizeMax and iStackSize
CODE_OFFSET = 0x64  # iCodeOffset: the header's size
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


path = sys.argv[1]
heap_min, heap_max, stack = (int(arg, 0) for arg in sys.argv[2:5])
image = bytearray(open(path, "rb").read())
if image[0x10:0x14] != b"EPOC":
    sys.exit(f"{path}: not an E32 image")
struct.pack_into("<III", image, HEAP_SIZE_MIN, heap_min, heap_max, stack)
struct.pack_into("<I", image, HEADER_CRC, header_crc(image))
open(path, "wb").write(image)
