#!/usr/bin/env python3
"""Builds a SISX installation package for Symbian^3, optionally self-signed.

Usage:
  make-sis.py --uid UID --name NAME --vendor VENDOR [--version A.B.C]
              --file SOURCE=TARGET [--file SOURCE=TARGET ...]
              [--platform UID=NAME ...]
              [--certificate CERT.pem --key KEY.pem] [--no-compress]
              --output PACKAGE.sis

TARGET is an install path such as '!:\\sys\\bin\\browser.exe' ('!' lets the
user choose the drive) or 'c:\\resource\\icu\\icudt70l.dat'. E32 images get
their header's capabilities in the file description, which the installer
checks against the image. A package may hold data files only.

Each --platform is a target device, such as 0x20022E6D=S60ProductID for
Symbian^3 (S60 5.2); a phone that matches none of them warns that the
application is not compatible.

The layout follows the symbian-platform SDK's native writer (cpp/symbian/sis),
which is checked against the platform's installer: one install block, no
options, an uncompressed controller and, when signed, one
RSA/SHA-1 signature over the controller up to the install block. Beyond it,
this writer takes any files (it stores them zlib-compressed by default) and
has no 16 MiB limit. With --no-compress and an executable plus its
registration and caption resources, it produces the same bytes as the SDK.
"""

import argparse
import hashlib
import os
import struct
import subprocess
import sys
import tempfile
import zlib

SIS_UID1 = 0x10201A7A
E32_SIGNATURE = b"EPOC"


def crc16(data):
    crc = 0
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ (0x1021 if crc & 0x8000 else 0)) & 0xFFFF
    return crc


def uid_checksum(header):
    crcs = [0, 0]
    for i in range(12):
        value = crcs[i % 2] ^ (header[i] << 8)
        for _ in range(8):
            value = ((value << 1) ^ (0x1021 if value & 0x8000 else 0)) & 0xFFFF
        crcs[i % 2] = value
    return (crcs[1] << 16) | crcs[0]


def word(value):
    return struct.pack("<I", value)


def wide(value):
    return struct.pack("<Q", value)


def half(value):
    return struct.pack("<H", value)


def field(kind, payload, in_array=False):
    if len(payload) >= 0x80000000:
        sys.exit("make-sis: a field over 2 GB needs 63-bit lengths")
    data = b"" if in_array else word(kind)
    data += word(len(payload)) + payload
    return data + b"\0" * (-len(payload) % 4)


def array(kind, elements=b""):
    return field(2, word(kind) + elements)


def string(text):
    return field(1, text.encode("utf-16-le"))


def strings(text):
    return array(1, field(1, text.encode("utf-16-le"), True))


def compressed(data, compress):
    """A SISCompressed field and the length of the stored bytes."""
    stored = zlib.compress(data, 9) if compress else data
    return field(3, word(int(compress)) + wide(len(data)) + stored), len(stored)


def e32_capabilities(data):
    """The low capability word of an E32 image, or 0 for other files."""
    if len(data) < 0x90 or data[16:20] != E32_SIGNATURE:
        return 0
    return struct.unpack_from("<I", data, 0x88)[0]


def dependency(uid, name):
    """A SISDependency on any version of UID, as an array element."""
    return field(
        18,
        field(9, word(uid))
        + field(5, field(4, word(0) * 3))  # From version 0.0.0, no upper bound.
        + strings(name),
        True,
    )


def build(uid, name, vendor, version, files, compress, platforms=()):
    info = field(
        14,
        field(9, word(uid))
        + string(vendor)
        + strings(name)
        + strings(vendor)
        + field(4, b"".join(word(part) for part in version))
        # Install date and time: fixed, as in the SDK, for reproducible output.
        + field(8, field(6, half(2004) + b"\0\1") + field(7, b"\0\0\0"))
        + b"\0\0",
    )
    descriptions = b""
    payloads = b""
    for index, (target, data) in enumerate(files):
        stored, stored_length = compressed(data, compress)
        capabilities = e32_capabilities(data)
        description = (
            string(target)
            + string("")
            + (field(41, word(capabilities)) if capabilities else b"")
            + field(25, word(1) + field(37, hashlib.sha1(data).digest()))
            + word(1)  # EOpInstall
            + word(0)
            + wide(stored_length)
            + wide(len(data))
            + word(index)
        )
        descriptions += field(24, description, True)
        payloads += field(32, stored, True)
    block = field(28, array(24, descriptions) + array(13) + array(26))
    controller = field(
        13,
        info
        + field(16, array(33))
        + field(15, array(11, field(11, word(1), True)))
        + field(17, array(18, b"".join(dependency(*p) for p in platforms)) + array(18))
        + field(19, array(20))
        + block
        + field(40, word(0)),
    )
    controller_field = compressed(controller, False)[0]
    data_field = field(30, array(31, field(31, array(32, payloads), True)))
    return controller_field, data_field


def sign(controller_field, certificate, key):
    """Adds an RSA/SHA-1 signature chain, as the SDK's SignPackage does."""
    controller = controller_field[20:]  # SISCompressed header, uncompressed.
    payload_length = struct.unpack_from("<I", controller, 4)[0]
    payload = controller[8 : 8 + payload_length]
    # Hash the controller through the install block: everything but the
    # trailing SISDataIndex field (type 40, 4-byte payload).
    prefix, index = payload[:-12], payload[-12:]
    if struct.unpack_from("<I", index, 0)[0] != 40:
        sys.exit("make-sis: unexpected controller layout")
    with tempfile.TemporaryDirectory() as temp:
        prefix_path = os.path.join(temp, "controller")
        with open(prefix_path, "wb") as stream:
            stream.write(prefix)
        signature = subprocess.run(
            ["openssl", "dgst", "-sha1", "-sign", key, prefix_path],
            check=True,
            capture_output=True,
        ).stdout
        der = subprocess.run(
            ["openssl", "x509", "-in", certificate, "-outform", "DER"],
            check=True,
            capture_output=True,
        ).stdout
    chain = field(
        39,
        array(
            36,
            field(
                36,
                field(38, string("1.2.840.113549.1.1.5"))
                + field(37, signature),
                True,
            ),
        )
        + field(22, field(37, der)),
    )
    return compressed(field(13, prefix + chain + index), False)[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--uid", required=True, type=lambda v: int(v, 0))
    parser.add_argument("--name", required=True)
    parser.add_argument("--vendor", required=True)
    parser.add_argument("--version", default="1.0.0")
    parser.add_argument("--file", action="append", required=True)
    parser.add_argument("--platform", action="append", default=[])
    parser.add_argument("--certificate")
    parser.add_argument("--key")
    parser.add_argument("--no-compress", action="store_true")
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    version = [int(part) for part in args.version.split(".")]
    if len(version) != 3 or not all(0 <= part <= 32767 for part in version):
        sys.exit("make-sis: version is MAJOR.MINOR.BUILD, each 0..32767")
    files = []
    for spec in args.file:
        source, _, target = spec.partition("=")
        if not target:
            sys.exit(f"make-sis: --file needs SOURCE=TARGET: {spec}")
        with open(source, "rb") as stream:
            files.append((target, stream.read()))

    platforms = []
    for spec in args.platform:
        uid, _, name = spec.partition("=")
        platforms.append((int(uid, 0), name or "S60ProductID"))
    controller_field, data_field = build(
        args.uid, args.name, args.vendor, version, files, not args.no_compress, platforms
    )
    if args.certificate or args.key:
        if not (args.certificate and args.key):
            sys.exit("make-sis: signing needs both --certificate and --key")
        controller_field = sign(controller_field, args.certificate, args.key)

    header = bytearray(word(SIS_UID1) + word(0) + word(args.uid) + word(0))
    struct.pack_into("<I", header, 12, uid_checksum(header))
    contents = field(
        12,
        field(34, half(crc16(controller_field)))
        + field(35, half(crc16(data_field)))
        + controller_field
        + data_field,
    )
    with open(args.output, "wb") as stream:
        stream.write(bytes(header) + contents)
    print(f"{args.output}: {len(header) + len(contents)} bytes, "
          f"{len(files)} files{', signed' if args.certificate else ''}")


if __name__ == "__main__":
    main()
