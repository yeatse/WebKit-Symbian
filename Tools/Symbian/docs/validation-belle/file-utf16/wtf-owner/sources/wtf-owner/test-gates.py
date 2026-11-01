# Synthetic records only; this never opens a guest, display or server.
import copy
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).parent))
sys.path.insert(0, str(Path(__file__).parent / 'runtime'))
import validate
import gates

checks = 0
run = 'validator-only-owner'


def check(value):
    global checks
    assert value
    checks += 1


def record(failure=False):
    lines = ['BEGIN ' + json.dumps(dict(suite='wtf-file-owner-contract', version=1, run=run)),
             'ABI ' + json.dumps(dict(wchar=2, wchar_align=2, char16=2,
                                      unicode_units=[23383, 55357, 56832]))]
    for index in range(3):
        row = {key: True for key in validate.BOOLEAN_FIELDS}
        row.update({key: 0 for key in validate.INTEGER_FIELDS})
        row.update(api='WTF::FileSystem', file=index, size=validate.SIZES[index],
                   bytes=validate.SIZES[index], mtime=1700000000,
                   sha256=validate.HASHES[index])
        if failure and index == 2:
            row.update({'pass': False, 'opened': False, 'open_errno': 2,
                        'read_ok': False, 'eof': False, 'bytes': 0,
                        'exact': False, 'hash_ok': False, 'sha256': ''})
        lines.append('CASE ' + json.dumps(row))
    lines += ['RESULT ' + json.dumps(dict(suite='wtf-file-owner-contract', version=1,
                                        run=run, passed=2 if failure else 3,
                                        failed=int(failure), total=3)),
              'wtf-file-owner-contract exiting ' + str(int(failure))]
    return ('\n'.join(lines) + '\n').encode()


good = dict(run=run, host_exit=0, natural_host_exit=True, forced_cleanup=False,
            wm_close_sent=False, deadline_expired=False, inputs_unchanged=True,
            restoration={'guest': {'restored': True}, 'configuration': {'restored': True}})
zero = b'[Kernel]: Thread Main forcefully killed with category: None and exit code: 0\n'
one = zero.replace(b'code: 0', b'code: 1')
check(validate.validate(record(), run)['passed'] == 3)
check(validate.validate(record(True), run)['failed'] == 1)
check(gates.completion_gate(copy.deepcopy(good), zero, record(), validate)['status'] == 'PASS')
result = copy.deepcopy(good)
try:
    gates.completion_gate(result, one, record(True), validate)
    check(False)
except RuntimeError:
    check(result['normal_worker_completion'] and result['api_assertions_failed'])
for host, body in [(b'Target panicked', record()), (one, record()), (zero, record(True)),
                   (zero + b'Thread Unknown forcefully killed with category: None and exit code: 1', record()),
                   (b'', record()), (zero, record()[:-20])]:
    try:
        gates.completion_gate(copy.deepcopy(good), host, body, validate)
        check(False)
    except RuntimeError:
        check(True)
for key, value in [('host_exit', 1), ('forced_cleanup', True), ('deadline_expired', True),
                   ('wm_close_sent', True), ('inputs_unchanged', False)]:
    result = copy.deepcopy(good)
    result[key] = value
    try:
        gates.completion_gate(result, zero, record(), validate)
        check(False)
    except RuntimeError:
        check(True)
check(validate.validate(record(), 'stale')['status'] == 'FAIL')
print(json.dumps({'status': 'PASS', 'synthetic_checks': checks, 'guest_started': False}))
