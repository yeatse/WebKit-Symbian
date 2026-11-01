"""Three public fixture files only; this module never opens a guest or server."""
import argparse
import hashlib
import json
import os
from pathlib import Path

MTIME = 1700000000
FILES = {
    'alpha.bin': bytes(range(256)) * 4,
    'zero.txt': b'',
    '字😀.dat': 'Belle 文件上传 😀\n'.encode('utf-8'),
}


def prepare(directory):
    directory = Path(directory)
    if directory.is_symlink() or directory.exists():
        raise ValueError('fixture directory must be fresh')
    directory.mkdir(parents=True, exist_ok=False)
    rows = []
    for index, (name, data) in enumerate(FILES.items()):
        path = directory / name
        with path.open('xb') as stream:
            stream.write(data)
        os.utime(path, (MTIME, MTIME))
        if path.read_bytes() != data or path.stat().st_mtime != MTIME:
            raise ValueError('controlled fixture bytes or mtime differ')
        rows.append({'file': index, 'name': name, 'bytes': len(data),
                     'sha256': hashlib.sha256(data).hexdigest(), 'mtime': MTIME})
    return rows


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps({'suite': 'wtf-file-owner-contract', 'version': 1,
                      'files': prepare(args.output)}, ensure_ascii=False, indent=2))
