#!/usr/bin/env python3
# Copyright (C) 2026 The symbian-webkit authors.
# SPDX-License-Identifier: BSD-2-Clause

"""Embed WebCore's existing Composite HRTF PCM with deterministic validation."""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import wave


def pcm(path, frames):
    with wave.open(str(path), 'rb') as stream:
        expected = (2, 2, 44100, frames, 'NONE', 'not compressed')
        if tuple(stream.getparams()) != expected:
            raise ValueError(f'{path}: unexpected HRTF WAVE parameters')
        data = stream.readframes(frames)
        if len(data) != frames * 4 or stream.readframes(1):
            raise ValueError(f'{path}: invalid HRTF sample count')
        return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('resources', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--manifest', type=Path)
    args = parser.parse_args()
    composite = args.resources / 'Composite.wav'
    data = pcm(composite, 240 * 256)
    sources = []
    elevations = (0, 15, 30, 45, 60, 75, 90, 315, 330, 345)
    for azimuth in range(0, 360, 15):
        for elevation_index, elevation in enumerate(elevations):
            resource = args.resources / f'IRC_Composite_C_R0195_T{azimuth:03}_P{elevation:03}.wav'
            impulse = pcm(resource, 256)
            index = azimuth // 15 * 10 + elevation_index
            if data[index * 1024:(index + 1) * 1024] != impulse:
                raise ValueError(f'{resource}: does not match Composite range {index}')
            sources.append({'name': resource.stem, 'index': index,
                            'sha256': hashlib.sha256(resource.read_bytes()).hexdigest()})
    values = struct.unpack('<122880h', data)
    source_hash = hashlib.sha256(composite.read_bytes()).hexdigest()
    lines = ['// Generated from existing WebCore HRTF resources; do not edit.\n',
             f'// Composite.wav SHA256: {source_hash}\n',
             '#pragma once\n#include <array>\n#include <cstdint>\n',
             'namespace WebCore::SymbianHRTF {\n',
             'inline constexpr std::array<int16_t, 122880> samples {\n']
    for offset in range(0, len(values), 16):
        lines.append('    ' + ', '.join(str(value) for value in values[offset:offset + 16]) + ',\n')
    lines.append('};\n} // namespace WebCore::SymbianHRTF\n')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(''.join(lines))
    if args.manifest:
        args.manifest.write_text(json.dumps({'source': str(composite), 'sha256': source_hash,
                                            'sample_rate': 44100, 'channels': 2,
                                            'frames': 61440, 'pcm_bytes': len(data),
                                            'individual_responses': sources}, indent=2) + '\n')


if __name__ == '__main__':
    main()
