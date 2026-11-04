#!/usr/bin/env python3
"""Match actual proxy export words to all official frozen ordinals."""
from pathlib import Path
import json
import struct

ROOT = Path('/workspace/symbian-screenplay-sdk-audit-work')
result = []
for record in json.loads((ROOT / 'stage-proxy-result.json').read_text()):
    data = Path(record['proxy']).read_bytes()
    assert data[:6] == b'\x7fELF\x01\x01'
    assert struct.unpack_from('<H', data, 18)[0] == 40
    start = struct.unpack_from('<I', data, 32)[0]
    entry_size, count = struct.unpack_from('<HH', data, 46)
    sections = [struct.unpack_from('<10I', data, start + i * entry_size)
                for i in range(count)]
    delivered = {}
    for section in sections:
        if section[1] != 11:
            continue
        strings = sections[section[6]]
        names = data[strings[4]:strings[4] + strings[5]]
        for offset in range(section[4], section[4] + section[5], section[9]):
            name, value, size, info, other, index = struct.unpack_from(
                '<IIIBBH', data, offset)
            if not name or not index or info >> 4 != 1:
                continue
            symbol = names[name:names.index(b'\0', name)].decode()
            storage = sections[index]
            word = storage[4] + value - storage[3]
            delivered[symbol] = struct.unpack_from('<I', data, word)[0]
    expected = {row['symbol']: row['ordinal']
                for row in record['frozen_exports']}
    assert delivered == expected
    result.append({'dll': record['name'] + '.dll',
                   'actual_export_words_matched': len(delivered),
                   'proxy_sha256': record['sha256']})
output = {'status': 'PASS_ACTUAL_FROZEN_ORDINAL_WORDS', 'libraries': result,
          'runtime_executed': False}
Path(__file__).with_name('proxy-word-check-result.json').write_text(
    json.dumps(output, indent=2) + '\n')
print(json.dumps(output))
