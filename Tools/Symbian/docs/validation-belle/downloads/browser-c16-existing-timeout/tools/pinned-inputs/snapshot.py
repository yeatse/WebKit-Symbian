#!/usr/bin/env python3
"""Read a dedicated mapped guest directory; optionally archive its exact bytes."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil

def snapshot(directory, run, case, guest_directory):
    directory = directory.resolve(strict=True)
    if not directory.is_dir():
        raise ValueError('mapped destination must be an existing directory')
    files, directories = [], []
    for path in sorted(directory.rglob('*')):
        if path.is_symlink():
            raise ValueError('dedicated destination contains a symlink')
        relative = path.relative_to(directory).as_posix()
        if path.is_dir():
            directories.append(relative)
            continue
        before = path.stat()
        data = path.read_bytes()
        after = path.stat()
        if (before.st_size, before.st_mtime_ns, before.st_ino) != (after.st_size, after.st_mtime_ns, after.st_ino):
            raise ValueError('a destination file changed while hashing; snapshot after the terminal callback')
        files.append({'name': relative, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(),
                      'mtime_ns': after.st_mtime_ns, 'inode': after.st_ino})
    return {'run': run, 'case': case, 'guest_directory': guest_directory,
            'host_directory': str(directory), 'files': files, 'directories': directories,
            'captured_utc': datetime.now(timezone.utc).isoformat()}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, required=True)
    parser.add_argument('--guest-directory', required=True)
    parser.add_argument('--run', required=True)
    parser.add_argument('--case', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--copy-to', type=Path)
    args = parser.parse_args()
    root = args.directory.resolve(strict=True)
    for destination in [args.output, *([args.copy_to] if args.copy_to else [])]:
        if destination.resolve().is_relative_to(root):
            parser.error('evidence destinations must be outside the mapped guest directory')
        if destination.exists():
            parser.error('use fresh evidence paths; do not replace earlier snapshots')
    report = snapshot(root, args.run, args.case, args.guest_directory)
    if args.copy_to:
        args.copy_to.mkdir(parents=True, exist_ok=False)
        for item in report['files']:
            dest = args.copy_to / item['name']
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(root / item['name'], dest)
            if hashlib.sha256(dest.read_bytes()).hexdigest() != item['sha256']:
                raise ValueError('destination changed while archiving')
        report['archive_directory'] = str(args.copy_to.resolve())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))

if __name__ == '__main__':
    main()
