#!/usr/bin/env python3
"""Extract the actual production copy function for the tiny real OpenC probe."""
import argparse
import hashlib
import json
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[4]
source = root / 'Source/WTF/wtf/symbian/FileSystemSymbian.cpp'
text = source.read_text()
start = text.index('static bool copy(')
end = text.index('\nbool copyFile(', start)
body = text[start:end]
assert body.count('static bool copy(') == 1
args.build.mkdir(parents=True, exist_ok=True)
(args.build / 'production-copy.inc').write_text(body)
(args.build / 'production-source.json').write_text(json.dumps({
    'source': str(source.relative_to(root)),
    'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
    'function_sha256': hashlib.sha256(body.encode()).hexdigest(),
    'scope': 'Exact production copy function; std::string substitutes WTF path storage, actual OpenC APIs unchanged'
}, indent=2) + '\n')
