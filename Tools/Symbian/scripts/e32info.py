#!/usr/bin/env python3
"""Prints the main E32ImageHeader fields of an uncompressed EKA2 image."""
import struct
import sys

FIELDS = [
    (0x00, "uid1"), (0x04, "uid2"), (0x08, "uid3"), (0x1C, "compression"),
    (0x2C, "flags"), (0x30, "code size"), (0x34, "data size"),
    (0x38, "heap min"), (0x3C, "heap max"), (0x40, "stack size"),
    (0x44, "bss size"), (0x48, "entry point"), (0x54, "imported DLLs"),
    (0x60, "text size"),
]


def main(path):
    image = open(path, "rb").read()
    word = lambda offset: struct.unpack_from("<I", image, offset)[0]
    if image[0x10:0x14] != b"EPOC":
        sys.exit(f"{path}: not an E32 image")
    print(f"{path}: {len(image)} bytes")
    for offset, name in FIELDS:
        value = word(offset)
        print(f"  {name:14} 0x{value:08x}  {value}")
    for offset, name in ((0x70, "code relocs"), (0x74, "data relocs")):
        section = word(offset)
        if section:
            print(f"  {name:14} {word(section + 4)} entries, {word(section)} bytes")
    print(f"  {'imports':14} {word(word(0x6C))} bytes")


if __name__ == "__main__":
    main(sys.argv[1])
