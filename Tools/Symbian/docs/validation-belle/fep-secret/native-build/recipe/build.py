#!/usr/bin/env python3
"""Build the EKA general FEP from an explicitly supplied, patched source tree.

Only a fresh work directory is written. No emulator, SDK installation, assets
or user configuration is changed. --prepare-only does not invoke a toolchain.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

HERE = Path(__file__).resolve().parent


def digest(path):
    """Returns a source or build artifact identity."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition, message):
    """Rejects an unsupported input before compiling or writing assets."""
    if not condition:
        raise ValueError(message)


def copy(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    require(digest(source) == digest(destination), 'Input changed while copying: ' + str(source))


def run(command, cwd, log):
    """Executes one normal build tool and preserves its complete output."""
    with log.open('wb') as stream:
        subprocess.run(list(map(str, command)), cwd=cwd, stdout=stream,
                       stderr=subprocess.STDOUT, check=True)


def prepare(eka, sdk, work, converter):
    """Stages the declared FEP source and exact official import interface."""
    source = work / 'source'
    overlay = work / 'sdk-overlay'
    sources = json.loads((HERE / 'official/sources.json').read_text())
    for name, identity in sources.items():
        original = HERE / 'official' / name
        require(digest(original) == identity['sha256'], 'Changed official input: ' + name)
        if 'blob_sha' in identity:
            raw = original.read_bytes()
            require(hashlib.sha1(b'blob ' + str(len(raw)).encode() + b'\0' + raw).hexdigest()
                    == identity['blob_sha'], 'Official Git blob mismatch: ' + name)
    patch = eka / 'src/patch'
    guest = patch / 'avkonfep'
    mmp = guest / 'group/general/avkonfep_general.mmp'
    require(re.search(r'\bUID\s+0x10009d8d\s+0x100056de\b', mmp.read_text(), re.I),
            'Unexpected general FEP identity')
    text = (guest / 'src/HostBridgedImeFEP.cpp').read_text()
    header = (guest / 'inc/HostBridgedImeFEP.h').read_text()
    require('void CHostBridgedImeFEP::CancelChangedTextL()' in text
            and 'CancelChangedTextL' in header and 'iStatus.Int() == KErrNone' in text,
            'Apply the EKA 0028 FEP cancel patch before this build')
    originals = []
    for original, name in [
        (guest / 'src/HostBridgedImeFEP.cpp', 'HostBridgedImeFEP.cpp'),
        (guest / 'inc/HostBridgedImeFEP.h', 'HostBridgedImeFEP.h'),
        (guest / 'inc/Dispatch.h', 'Dispatch.h'),
        (guest / 'inc/aknextendedinputcapabilities.h', 'aknextendedinputcapabilities.h'),
        (patch / 'priv/src/Log.cpp', 'Log.cpp'),
        (patch / 'priv/inc/Log.h', 'Log.h'),
        (patch / 'priv/inc/sv.s', 'sv.s'),
    ]:
        copy(original, source / name)
        originals.append({'source': str(original), 'staged': 'source/' + name,
                          'sha256': digest(original)})
    dispatch = guest / 'src/Dispatch.s'
    assembly = dispatch.read_text()
    old = '.include "../../../priv/inc/sv.S"'
    require(assembly.count(old) == 1, 'Unexpected dispatch include')
    assembly = assembly.replace(old, '.include "' + str(source / 'sv.s') + '"')
    assembly = re.sub(r'([A-Za-z_][A-Za-z0-9_]*):(\n[ \t]+CallHleDispatch[^\n]+)',
                      r'.type \1, %function\n\1:\2\n.size \1, .-\1', assembly)
    (source / 'Dispatch.S').write_text(assembly)
    originals.append({'source': str(dispatch), 'staged': 'source/Dispatch.S',
                      'source_sha256': digest(dispatch),
                      'staged_sha256': digest(source / 'Dispatch.S'),
                      'changes': 'Local include path and ARM function metadata only'})
    for name in ('CMakeLists.txt', 'native-patch-compat.h'):
        copy(HERE / name, source / name)
    script = sdk / 'cmake/exe_unwind_image.ld'
    linker = script.read_text()
    for name in ('__ro_base', '__ro_end', '__exidx_start', '__exidx_end'):
        if 'HIDDEN(' + name + ' = .)' in linker:
            continue
        require(linker.count(name + ' = .;') == 1, 'Unexpected SDK linker label: ' + name)
        linker = linker.replace(name + ' = .;', 'HIDDEN(' + name + ' = .);')
    (source / 'fep_unwind_image.ld').write_text(linker)
    (source / 'avkonfep_general.def').write_text(
        'EXPORTS\n\t_Z24ImplementationGroupProxyRi @ 1 NONAME\n')
    for name, target in [('fepitfr.h', 'include/native/fepitfr.h'),
                         ('FEPBASU.DEF', 'share/symbian/native/defs/fepbase.dll.def'),
                         ('EPL-1.0.html', 'licenses/native/EPL-1.0.html')]:
        copy(HERE / 'official' / name, overlay / target)
    copy(HERE / 'converter-launch.py', work / 'fep-converter')
    (work / 'fep-converter').chmod(0o700)
    config = {'eka_source': str(eka), 'sdk_prefix': str(sdk),
              'converter': str(converter), 'uid2': '0x10009d8d', 'uid3': '0x100056de',
              'capabilities': 0, 'export_definition': 'source/avkonfep_general.def',
              'originals': originals, 'sdk_linker_sha256': digest(script),
              'identity_mmp': {'source': str(mmp), 'sha256': digest(mmp)},
              'official_sources': sources,
              'recipe_files': {name: digest(HERE / name) for name in
                               ('build.py', 'converter-launch.py', 'CMakeLists.txt',
                                'native-patch-compat.h')},
              'prepared_files': {str(path.relative_to(work)): digest(path)
                                 for directory in (source, overlay)
                                 for path in sorted(directory.rglob('*'))
                                 if path.is_file()}}
    (work / 'build-config.json').write_text(json.dumps(config, indent=2) + '\n')
    return config


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--eka-source', type=Path, required=True)
    parser.add_argument('--sdk', type=Path, default=os.environ.get('SYMBIAN_SDK_PREFIX'))
    parser.add_argument('--work', type=Path, help='Fresh directory; default WORK/fep-native')
    parser.add_argument('--converter', type=Path, default=os.environ.get('SYMBIAN_NATIVE'))
    parser.add_argument('--jobs', type=int, default=1)
    parser.add_argument('--prepare-only', action='store_true')
    args = parser.parse_args()
    try:
        require(args.sdk is not None, 'Set SYMBIAN_SDK_PREFIX or --sdk')
        require(1 <= args.jobs <= 16, '--jobs must be 1..16')
        sdk = args.sdk.resolve()
        eka = args.eka_source.resolve()
        work = args.work
        if work is None:
            require(bool(os.environ.get('WORK')), 'Set WORK or --work')
            work = Path(os.environ['WORK']) / 'fep-native'
        work = work.resolve()
        converter = (args.converter or sdk / 'bin/symbian-native').resolve()
        require(not work.exists(), 'Use a fresh --work directory')
        require(not work.is_relative_to(eka) and not work.is_relative_to(sdk),
                'Keep build output outside the SDK and EKA source tree')
        for path in (sdk / 'bin/clang++', sdk / 'bin/cmake', sdk / 'bin/ninja',
                     sdk / 'bin/ld.lld', sdk / 'cmake/SymbianApp.cmake', converter):
            require(path.is_file(), 'Missing SDK/tool input: ' + str(path))
        work.mkdir(parents=True)
        config = prepare(eka, sdk, work, converter)
        if args.prepare_only:
            config['status'] = 'PREPARED_NOT_BUILT'
            (work / 'result.json').write_text(json.dumps(config, indent=2) + '\n')
            print('Prepared inputs only: ' + str(work))
            return 0
        help_text = subprocess.check_output([str(converter), '--help'], text=True)
        require('--uid2' in help_text, 'Build the native converter with patch 0006 first')
        proxy = work / 'sdk-overlay/proxies/fepbase'
        run([converter, 'proxy-sources', '--definition', HERE / 'official/FEPBASU.DEF',
             '--target-dll', 'fepbase.dll', '--output', proxy], work, work / 'proxy-generate.log')
        run([sdk / 'bin/clang++', '--target=armv5t-none-eabi', '-march=armv5t',
             '-c', proxy / 'exports.S', '-o', proxy / 'exports.o'], work, work / 'proxy-compile.log')
        run([sdk / 'bin/ld.lld', '-m', 'armelf', '-shared', '--hash-style=sysv',
             '--build-id=none', '--soname=fepbase.dso',
             '--version-script=' + str(proxy / 'exports.map'), '-T', proxy / 'proxy.ld',
             proxy / 'exports.o', '-o', proxy / 'fepbase.dso'], work, work / 'proxy-link.log')
        run([sdk / 'bin/cmake', '-S', work / 'source', '-B', work / 'build', '-G', 'Ninja',
             '-DCMAKE_TOOLCHAIN_FILE=' + str(sdk / 'cmake/symbian-arm.cmake'),
             '-DSYMBIAN_SDK_PREFIX=' + str(sdk), '-DCMAKE_MAKE_PROGRAM=' + str(sdk / 'bin/ninja'),
             '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
             '-DSYMBIAN_NATIVE_CONVERTER=' + str(work / 'fep-converter')], work, work / 'configure.log')
        run([sdk / 'bin/ninja', '-C', work / 'build', '-j' + str(args.jobs)], work, work / 'build.log')
        for name, expected in config['prepared_files'].items():
            require(digest(work / name) == expected, 'Prepared source changed during build: ' + name)
        config['status'] = 'BUILD_COMPLETED_GUEST_NOT_RUN'
        config['artifacts'] = {}
        for name in ('avkonfep_general.dll', 'avkonfep_general_elf.elf', 'avkonfep_general.elf.map'):
            path = work / 'build' / name
            config['artifacts'][name] = {'bytes': path.stat().st_size, 'sha256': digest(path)}
        config['converter_sha256'] = digest(converter)
        (work / 'result.json').write_text(json.dumps(config, indent=2) + '\n')
        print(json.dumps(config['artifacts'], indent=2))
        return 0
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + '\n')


if __name__ == '__main__':
    raise SystemExit(main())
