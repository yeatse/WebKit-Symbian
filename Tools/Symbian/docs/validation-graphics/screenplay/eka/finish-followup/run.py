#!/usr/bin/env python3
"""Run owned host contracts against the actual Finish body and queue header."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument('--source-root', type=Path, default=Path('/workspace/EKA2L1-validation'))
parser.add_argument('--cxx', default='c++')
args = parser.parse_args()
source = args.source_root / 'src/emu/services/src/window/window.cpp'
queue = args.source_root / 'src/emu/common/include/common/queue.h'
text = source.read_text()
start = text.index('    void window_server::finish_rendering(service::ipc_context &context) {')
end = text.index('    void window_server::send_to_command_buffer(', start)
body = text[start:end]
(ROOT / 'actual-finish-body.cpp').write_text(body)
(ROOT / 'actual-queue.h').write_bytes(queue.read_bytes())
prelude = (ROOT / 'contract-prelude.cpp').read_text()
tests = (ROOT / 'contract-tests.cpp').read_text()
assert tests.count('int main() {') == 1
fixture = tests[:tests.index('int main() {')]
(ROOT / 'contract.cpp').write_text(prelude + body + tests)
(ROOT / 'backpressure.cpp').write_text(
    '#include "actual-queue.h"\n' + prelude + body + fixture +
    (ROOT / 'backpressure-tests.cpp').read_text())

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

results = []
for name, cases in [('contract', 15), ('backpressure', 1)]:
    command = [args.cxx, '-std=c++17', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
               '-pthread', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
               '-fno-omit-frame-pointer', str(ROOT / (name + '.cpp')), '-o', str(ROOT / name)]
    with (ROOT / (name + '-compile.log')).open('wb') as log:
        compiled = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=60).returncode
    ran = None
    if compiled == 0:
        with (ROOT / (name + '-run.log')).open('wb') as log:
            ran = subprocess.run([str(ROOT / name)], stdout=log, stderr=subprocess.STDOUT, timeout=15).returncode
    results.append({'name': name, 'cases': cases, 'compile_exit': compiled, 'test_exit': ran})
result = {
    'status': 'HOST_CONTRACT_PASS_WITH_DOCUMENTED_BACKPRESSURE_LIMITATION'
              if all(row['compile_exit'] == row['test_exit'] == 0 for row in results) else 'FAIL',
    'source_sha256': sha(source), 'actual_body_sha256': sha(ROOT / 'actual-finish-body.cpp'),
    'actual_queue_sha256': sha(queue), 'results': results,
    'sanitizers': ['address', 'undefined'], 'guest_executed': False,
    'display_verified': False, 'strict_whole_call_2000ms_guarantee': False,
    'scope': 'Actual method body and actual queue, with owned API/lock/session stand-ins; no HLE or graphics backend execution',
    'limitation': 'Normal legacy scheduler submission can block on a full queue while holding kernel; Finish then waits beyond its polling budget when reacquiring kernel. The bounded host counterexample releases the queue after 2250 ms.',
}
(ROOT / 'test-result.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
raise SystemExit(0 if result['status'] != 'FAIL' else 1)
