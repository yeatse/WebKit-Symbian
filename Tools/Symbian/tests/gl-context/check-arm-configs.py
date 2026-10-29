#!/usr/bin/env python3
"""Check disabled-feature boundaries using real ARM commands, without Ninja.

Only a private copy of cmakeconfig.h and syntax-only outputs are produced.
This is not a CMake configuration/link/run matrix; USE_TEXTURE_MAPPER=OFF is
currently a preprocessor boundary, not a supported Symbian CMake option.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compile-commands', required=True, type=Path)
parser.add_argument('--output', required=True, type=Path)
args = parser.parse_args()
database = args.compile_commands.resolve()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
entries = json.loads(database.read_text())
config = database.parent / 'cmakeconfig.h'
original_config = config.read_text()
repo = Path(__file__).resolve().parents[4]

cases = {
    'webgl-off': ({'ENABLE_WEBGL': 0}, [
        'GLContext.cpp', 'GLContextWrapper.cpp', 'BitmapTexture.cpp',
        'GraphicsLayerTextureMapper.cpp', 'SymbianCompositor.cpp']),
    'texture-mapper-off-boundary': ({'ENABLE_WEBGL': 0, 'USE_TEXTURE_MAPPER': 0,
                                   'USE_GRAPHICS_LAYER_TEXTURE_MAPPER': 0}, [
        'GLContext.cpp', 'GLContextWrapper.cpp', 'BitmapTexture.cpp',
        'BitmapTexturePool.cpp', 'SymbianCompositor.cpp']),
}
report = {
    'kind': 'real ARM syntax-only feature boundary checks',
    'limitations': 'No CMake reconfiguration, archive link, guest, EGL or driver execution. TextureMapper OFF is not a supported Symbian CMake option.',
    'compile_commands_sha256': hashlib.sha256(database.read_bytes()).hexdigest(),
    'original_cmakeconfig_sha256': hashlib.sha256(config.read_bytes()).hexdigest(),
    'cases': [],
}
failed = False
with tempfile.TemporaryDirectory(prefix='symbian-context-configs-') as temporary:
    for case, (macros, names) in cases.items():
        include_dir = Path(temporary) / case
        include_dir.mkdir()
        content = original_config
        for macro, value in macros.items():
            content, count = re.subn(rf'^#define {macro} \d+$', f'#define {macro} {value}', content, flags=re.M)
            if count != 1:
                raise RuntimeError(f'{macro}: expected one generated definition, got {count}')
        # Make the checks fail if a later include or command-line definition
        # accidentally restores a feature we intended to disable.
        content += '\n' + '\n'.join(f'#if {macro} != {value}\n#error incorrect {macro}\n#endif' for macro, value in macros.items()) + '\n'
        (include_dir / 'cmakeconfig.h').write_text(content)
        for name in names:
            matches = [entry for entry in entries if Path(entry['file']).name == name]
            if len(matches) != 1:
                raise RuntimeError(f'{name}: expected one compile command, got {len(matches)}')
            entry = matches[0]
            command = entry.get('arguments') or shlex.split(entry['command'])
            command = list(command)
            for option in ['-o', '-MF', '-MT', '-MQ']:
                while option in command:
                    index = command.index(option)
                    del command[index:index + 2]
            command = [item for item in command if item not in ('-c', '-MD', '-MMD')]
            command[1:1] = ['-I', str(include_dir)]
            command += ['-fsyntax-only', '-fno-color-diagnostics']
            result = subprocess.run(command, cwd=entry['directory'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            (output / f'{case}-{name}.log').write_bytes(result.stdout)
            source = Path(entry['file'])
            record = {
                'case': case, 'defines': macros, 'source': str(source.relative_to(repo)),
                'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                'config_sha256': hashlib.sha256(content.encode()).hexdigest(),
                'exit_code': result.returncode,
            }
            report['cases'].append(record)
            failed |= bool(result.returncode)
            print(f'{case}: {name}: {"FAIL" if result.returncode else "PASS"}', flush=True)
            if result.returncode:
                print(result.stdout.decode(errors='replace')[-12000:], flush=True)
(output / 'summary.json').write_text(json.dumps(report, indent=2) + '\n')
raise SystemExit(1 if failed else 0)
