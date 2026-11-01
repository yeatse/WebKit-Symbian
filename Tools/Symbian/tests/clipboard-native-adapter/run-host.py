#!/usr/bin/env python3
"""Run production normalization and pre-allocation admission fragments on host."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, required=True)
args = parser.parse_args()
here = Path(__file__).resolve().parent
root = here.parents[3]
source = root / 'Tools/Symbian/browser/ClipboardNative.cpp'
text = source.read_text()

def function(signature):
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

write = function('void WriteL(Text value)')
prefix = write[:write.index('    CPlainText* text')]
prefix = prefix.replace('void WriteL(Text value)', 'std::u16string normalizedWrite(Text value)')
prefix += '    auto result = buffer.value();\n    delete normalized;\n    return result;\n}\n'
read = function('bool ReadL(TextResult* result)')
start = read.index('    TInt length = stream.ReadInt32L();')
end = read.index('    if (result)', start)
length_check = 'void checkSerializedLength(Reader& stream)\n{\n' + read[start:end] + '}\n'
store = function('CFileStore* PreflightStoreLC(RFs& fs)')
size_start = store.index('    if (size < 0')
size_end = store.index('    RStoreReadStream root;', size_start)
count_start = store.index('    if (static_cast<TInt>(formats)')
count_end = store.index('    CleanupStack::', count_start)
bounds = 'void checkStoreBounds(TInt size, TInt formats)\n{\n' + store[size_start:size_end] + store[count_start:count_end] + '}\n'
constants = '\n'.join(line for line in text.splitlines() if line.startswith('constexpr TInt KMaximum'))
fragments = constants + '\n' + function('void CheckDriveL()') + '\n' + function('void NormalizeForWeb(TDes& text)') + '\n' + prefix + length_check + bounds
args.build.mkdir(parents=True, exist_ok=True)
(args.build / 'production-boundaries.inc').write_text(fragments)
command = [os.environ.get('CXX', 'c++'), '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I' + str(args.build), '-I' + str(root / 'Source/WebKitLegacy/symbian'), str(here / 'host.cpp'), '-o', str(args.build / 'host')]
subprocess.run(command, check=True)
with (args.build / 'host.log').open('w') as stream:
    result = subprocess.run([str(args.build / 'host')], stdout=stream, stderr=subprocess.STDOUT)
manifest = {'scope': 'Exact production source fragments for normalization and admission, fake descriptor/HAL/Leave adapters only; not native store parsing or native cleanup proof', 'exit': result.returncode, 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(), 'fragments_sha256': hashlib.sha256(fragments.encode()).hexdigest(), 'log_sha256': hashlib.sha256((args.build / 'host.log').read_bytes()).hexdigest()}
(args.build / 'result.json').write_text(json.dumps(manifest, indent=2) + '\n')
print((args.build / 'host.log').read_text())
raise SystemExit(result.returncode)
