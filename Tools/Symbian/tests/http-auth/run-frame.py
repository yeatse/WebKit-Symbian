#!/usr/bin/env python3
"""Compile actual Legacy auth method bodies against a small lifecycle harness."""
import argparse
import pathlib
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[4])
args = parser.parse_args()
tests = pathlib.Path(__file__).resolve().parent
source = (args.source / 'Source/WebKitLegacy/symbian/SymbianFrameLoaderClient.cpp').read_text()
start = source.index('class AuthenticationTextResult final')
end = source.index('class SymbianFrameNetworkingContext final', start)
result = source[start:end]
start = source.index('void SymbianFrameLoaderClient::cancelAuthenticationChallenge()')
end = source.index('void SymbianFrameLoaderClient::dispatchDidChangeLocationWithinPage()', start)
methods = source[start:end]
with tempfile.TemporaryDirectory(prefix='symbian-http-auth-') as tmp:
    tmp = pathlib.Path(tmp)
    (tmp / 'production-frame.inc').write_text(result + methods)
    output = tmp / 'frame-contract'
    subprocess.run(['c++', '-std=c++20', '-g', '-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer', '-I' + str(tmp),
                    '-I' + str(args.source / 'Source/WebKitLegacy/symbian'),
                    str(tests / 'frame-contract.cpp'), '-o', str(output)], check=True)
    subprocess.run([str(output)], check=True)
