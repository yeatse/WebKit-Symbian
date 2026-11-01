"""Strict seven-line log parser; log success alone is not runtime acceptance."""
import argparse
import json
import math
from pathlib import Path
import re

SIZES = [1024, 0, 24]
HASHES = [
    '785b0751fc2c53dc14a4ce3d800e69ef9ce1009eb327ccf458afe09c242c26c9',
    'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855',
    '8a4a90e74c3d9c754d0f3ab6b391646f7de56516d70a5ad20c05e49030a481da',
]
BOOLEAN_FIELDS = {
    'pass', 'exists', 'size_available', 'mtime_available', 'mtime_ok',
    'opened', 'read_ok', 'eof', 'bounded', 'exact', 'hash_ok',
}
INTEGER_FIELDS = {
    'file', 'exists_errno', 'size', 'size_errno', 'mtime_errno',
    'open_errno', 'read_errno', 'bytes',
}
CASE_FIELDS = BOOLEAN_FIELDS | INTEGER_FIELDS | {'api', 'mtime', 'sha256'}


def validate(raw, token, *unused):
    try:
        def require(value, message):
            if not value:
                raise ValueError(message)

        lines = (raw.decode('utf-8') if isinstance(raw, bytes) else raw).splitlines()
        require(len(lines) == 7, 'exact BEGIN/ABI/3 CASE/RESULT/exiting required')
        require(lines[0].startswith('BEGIN '), 'BEGIN missing')
        begin = json.loads(lines[0][6:])
        require(begin == {'suite': 'wtf-file-owner-contract', 'version': 1, 'run': token},
                'fresh BEGIN differs')
        require(lines[1].startswith('ABI '), 'ABI missing')
        abi = json.loads(lines[1][4:])
        require(abi == {'wchar': 2, 'wchar_align': 2, 'char16': 2,
                        'unicode_units': [23383, 55357, 56832]}, 'UTF16 ABI differs')
        rows = []
        for index, line in enumerate(lines[2:5]):
            require(line.startswith('CASE '), 'CASE missing')
            row = json.loads(line[5:])
            require(set(row) == CASE_FIELDS, 'CASE shape differs')
            require(row['api'] == 'WTF::FileSystem' and row['file'] == index,
                    'actual owner/case identity or order differs')
            require(all(type(row[key]) is bool for key in BOOLEAN_FIELDS),
                    'boolean results required')
            require(all(type(row[key]) is int for key in INTEGER_FIELDS),
                    'integer results required')
            require(type(row['mtime']) in (int, float) and math.isfinite(row['mtime']),
                    'finite numeric mtime required')
            require(isinstance(row['sha256'], str) and
                    (row['sha256'] == '' or re.fullmatch('[0-9a-f]{64}', row['sha256'])),
                    'hash encoding differs')
            measured_time_ok = row['mtime_available'] and abs(row['mtime'] - 1700000000) <= 2
            require(row['mtime_ok'] == measured_time_ok, 'mtime claim differs from measured value')
            expected = (row['exists'] and row['size_available'] and row['size'] == SIZES[index]
                        and measured_time_ok and row['opened'] and row['read_ok']
                        and row['eof'] and row['bounded'] and row['bytes'] == SIZES[index]
                        and row['exact'] and row['hash_ok'] and row['sha256'] == HASHES[index])
            require(row['pass'] == expected, 'pass disagrees with actual results')
            rows.append(row)
        passed = sum(row['pass'] for row in rows)
        failed = 3 - passed
        require(lines[5].startswith('RESULT '), 'RESULT missing')
        result = json.loads(lines[5][7:])
        require(result == {'suite': 'wtf-file-owner-contract', 'version': 1, 'run': token,
                           'passed': passed, 'failed': failed, 'total': 3}, 'RESULT differs')
        code = int(bool(failed))
        require(lines[6] == 'wtf-file-owner-contract exiting ' + str(code),
                'explicit exit disagrees')
        return {'status': ('LOG_CONTRACT_PASS_PENDING_TARGET_EXIT' if not failed else
                           'API_ASSERTIONS_FAIL_COMPLETE_PENDING_TARGET_EXIT'),
                'run': token, 'complete': True, 'cases': 3, 'passed': passed,
                'failed': failed, 'worker_exit': code, 'rows': rows, 'abi': abi}
    except (ValueError, TypeError, KeyError, UnicodeError) as error:
        return {'status': 'FAIL', 'run': token, 'complete': False, 'failures': [str(error)]}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--log', type=Path, required=True)
    parser.add_argument('--token', required=True)
    args = parser.parse_args()
    report = validate(args.log.read_bytes(), args.token)
    print(json.dumps(report, ensure_ascii=False, indent=2))
    raise SystemExit(0 if report.get('complete') and not report.get('failed') else 1)
