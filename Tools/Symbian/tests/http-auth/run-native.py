#!/usr/bin/env python3
"""Exercise the production native controller with a bounded AVKON test double."""
import argparse
import pathlib
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[4])
args = parser.parse_args()
tests = pathlib.Path(__file__).resolve().parent
browser = args.source / 'Tools/Symbian/browser'
with tempfile.TemporaryDirectory(prefix='symbian-http-auth-') as tmp:
    output = pathlib.Path(tmp) / 'native-ui'
    subprocess.run(['c++', '-std=c++20', '-g', '-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer', '-I' + str(tests / 'fakes'),
                    '-I' + str(browser), '-I' + str(args.source / 'Source/WebKitLegacy/symbian'),
                    str(tests / 'native-ui.cpp'), str(browser / 'AuthenticationUI.cpp'),
                    '-o', str(output)], check=True)
    subprocess.run([str(output)], check=True)
