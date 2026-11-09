#!/usr/bin/env python3
"""Sets bits in an E32 image's iFlags and updates the header CRC.

Usage: set-e32-flags.py MASK IMAGE...   (e.g. 0x2, KImageNoCallEntryPoint)
"""
import struct
import sys

HEADER_CRC = 0x14
IMAGE_FLAGS = 0x2C
CODE_OFFSET = 0x64
CRC_INITIALISER = 0xC90FDAA2


def header_crc(image):
    header = bytearray(image[: struct.unpack_from("<I", image, CODE_OFFSET)[0]])
    struct.pack_into("<I", header, HEADER_CRC, CRC_INITIALISER)
    crc = 0
    for byte in header:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xEDB88320 if crc & 1 else 0)
    return crc


mask = int(sys.argv[1], 0)
for path in sys.argv[2:]:
    image = bytearray(open(path, "rb").read())
    if image[0x10:0x14] != b"EPOC":
        sys.exit(f"{path}: not an E32 image")
    flags = struct.unpack_from("<I", image, IMAGE_FLAGS)[0] | mask
    struct.pack_into("<I", image, IMAGE_FLAGS, flags)
    struct.pack_into("<I", image, HEADER_CRC, header_crc(image))
    open(path, "wb").write(image)
