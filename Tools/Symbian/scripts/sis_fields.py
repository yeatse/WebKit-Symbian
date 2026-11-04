"""Read SIS fields, arrays and compressed payloads."""

import struct
import zlib


def field(data, position=0):
    kind, length = struct.unpack_from("<II", data, position)
    end = position + 8 + length
    padded = position + 8 + ((length + 3) & ~3)
    if length >= 0x80000000 or padded > len(data):
        raise ValueError("Truncated or unsupported SIS field")
    return kind, data[position + 8:end], padded


def fields(data):
    position = 0
    result = []
    while position < len(data):
        start = position
        kind, payload, position = field(data, position)
        result.append((kind, payload, data[start:position]))
    return result


def array(data, expected_kind):
    kind, payload, end = field(data)
    if kind != 2 or end > len(data) or struct.unpack_from("<I", payload)[0] != expected_kind:
        raise ValueError("Unexpected SIS array")
    position = 4
    result = []
    while position < len(payload):
        length = struct.unpack_from("<I", payload, position)[0]
        end = position + 4 + length
        padded = position + 4 + ((length + 3) & ~3)
        if padded > len(payload):
            raise ValueError("Truncated SIS array item")
        result.append(payload[position + 4:end])
        position = padded
    return result


def decompress(payload):
    algorithm, size = struct.unpack_from("<IQ", payload)
    if algorithm not in (0, 1):
        raise ValueError("Unsupported SIS compression")
    data = zlib.decompress(payload[12:]) if algorithm else payload[12:]
    if len(data) != size:
        raise ValueError("SIS uncompressed size mismatch")
    return data
