#!/usr/bin/env python3
"""Compile the actual download manager with host-only callback/type substitutes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
from importlib.machinery import SourceFileLoader


def main():
    tests = Path(__file__).resolve().parent
    checkout = tests.parents[3]
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    args.build.mkdir(parents=True, exist_ok=True)
    extract = SourceFileLoader('queue_extract', str(tests / 'run-deferred-queue.py')).load_module().definition
    sources = checkout / 'Source/WebKitLegacy/symbian'
    client = sources / 'SymbianDownload.h'
    header = sources / 'SymbianDownloadManager.h'
    source = sources / 'SymbianDownloadManager.cpp'
    (args.build / 'download-client-under-test.inc').write_text(extract(client.read_text(), 'class SymbianDownloadClient {') + '\n')
    (args.build / 'manager-header-under-test.inc').write_text(extract(header.read_text(), 'class SymbianDownloadManager final') + '\n')
    body = source.read_text().split('namespace WebKitSymbian {', 1)[1].rsplit('} // namespace WebKitSymbian', 1)[0]
    (args.build / 'manager-body-under-test.inc').write_text(body)
    identities = {'files': [{'name': p.name, 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()} for p in (client, header, source)],
                  'scope': 'Download manager callbacks and lifecycle'}
    (args.build / 'manager-source-identities.json').write_text(json.dumps(identities, indent=2) + '\n')
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++23', '-Wall', '-Wextra', '-Werror',
                    '-I', str(args.build), '-I', str(sources), str(tests / 'manager.cpp'), '-o', str(args.build / 'manager-tests')], check=True)
    subprocess.run([str(args.build / 'manager-tests')], check=True)


if __name__ == '__main__':
    main()
