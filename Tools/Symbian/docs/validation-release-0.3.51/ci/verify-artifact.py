#!/usr/bin/env python3
"""Verify local exact-run metadata/ZIP/SIS and E32 imports; no networking or guest."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import zipfile


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('work', type=Path)
    p.add_argument('--icu-root', type=Path, required=True)
    a = p.parse_args()
    r = a.work.resolve()
    load = lambda name: json.loads((r / name).read_text())
    spec, run, jobs, artifacts = [load(name) for name in ['spec.json', 'run.json', 'jobs.json', 'artifacts.json']]
    assert run['id'] == spec['run_id'] and run['run_number'] == spec['run_number']
    assert run['head_sha'] == spec['head_sha'] and run['head_branch'] == spec['branch'] and run['event'] == spec['event']
    assert run['status'] == 'completed' and run['conclusion'] == 'success'
    selected = [job for job in jobs['jobs'] if job['name'] == spec['expected_job_name']]
    assert len(selected) == 1
    job = selected[0]
    assert job['status'] == 'completed' and job['conclusion'] == 'success'
    required = ['E32 converter', 'Rebuild and verify the 32-bit SDK allocator',
                'Rebuild and verify the SDK math member', 'Rebuild and verify the SDK complex members',
                'ScreenPlay buffer lifecycle regression', 'WebCore', 'browser.exe', 'SIS packages',
                'Upload SIS packages']
    for name in required:
        steps = [step for step in job['steps'] if step['name'] == name]
        assert len(steps) == 1 and steps[0]['status'] == 'completed' and steps[0]['conclusion'] == 'success'
    selected = [item for item in artifacts['artifacts'] if item['name'] == 'sis']
    assert len(selected) == 1
    artifact = selected[0]
    assert not artifact['expired']
    assert artifact['workflow_run']['id'] == spec['run_id'] and artifact['workflow_run']['head_sha'] == spec['head_sha']
    assert artifact['workflow_run']['head_branch'] == spec['branch']
    archive = r / 'sis.zip'
    assert archive.stat().st_size == artifact['size_in_bytes'] and 'sha256:' + sha(archive) == artifact['digest']
    with zipfile.ZipFile(archive) as zipped:
        names = zipped.namelist()
        expected = {'SHA256SUMS', 'browser.sis', 'jsc.sis', 'symbian-webkit-icu.sis'}
        assert len(names) == 4 and set(names) == expected
        assert all(zipped.getinfo(name).file_size < 100 * 1024 * 1024 for name in names)
        assert sum(zipped.getinfo(name).file_size for name in names) < 150 * 1024 * 1024
        assert zipped.testzip() is None
        target = r / 'sis'
        assert not target.exists()
        target.mkdir(mode=0o700)
        for name in names:
            assert zipped.getinfo(name).file_size < 100 * 1024 * 1024
            (target / name).write_bytes(zipped.read(name))
    (r / 'archive-verification.json').write_text(json.dumps({'status': 'PASS',
        'run_id': spec['run_id'], 'source_commit': spec['head_sha'], 'artifact_id': artifact['id'],
        'bytes': archive.stat().st_size, 'sha256': sha(archive), 'exact_members': sorted(expected),
        'zip_CRCs_verified': True}, indent=2) + '\n')
    commands = [
        [sys.executable, str(r / 'verify-payloads.py'), str(r), '--tools-root', str(r / 'verification-tools/Tools/Symbian')],
        [sys.executable, str(r / 'check-source-payloads.py')],
        [sys.executable, str(r / 'check-auth-resource.py')],
        [sys.executable, str(r / 'verification-tools/Tools/Symbian/tests/icu-idna/run.py'),
         '--icu-root', str(a.icu_root), '--existing-data', str(r / 'payloads/icudt70l.dat'),
         '--output', str(r / 'icu-payload-idna.json')],
    ]
    assert not (r / 'icu-payload-idna.json').exists()
    for index, command in enumerate(commands):
        with (r / ('check-' + str(index) + '.log')).open('wb') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
    icu = load('icu-payload-idna.json')
    assert icu['result'] == 'PASS' and len(icu['checks']) == 8
    assert all(row['result'] == 'PASS' for row in icu['checks'])
    existing = [row for row in icu['checks'] if row['name'] == 'existing package']
    assert len(existing) == 1 and existing[0]['expect_missing'] is False
    assert existing[0]['data_sha256'] == sha(r / 'payloads/icudt70l.dat')
    parser = load('parser-pin.json')
    tool = Path(parser['path'])
    assert sha(tool) == parser['sha256']
    image = r / 'payloads/browser.exe'
    with (r / 'e32-parser.stderr.log').open('wb') as error:
        parsed = json.loads(subprocess.check_output([str(tool), str(image)], stderr=error))
    assert parsed['status'] == 'PASS' and parsed['header_crc_verified'] and parsed['import_bounds_and_code_slots_verified']
    assert parsed['uid_checksum_verified_by_EKA'] and parsed['uid3'] == 0xE1A5C002 and parsed['capabilities'] == 0x2000
    dlls = sorted(row['dll'] for row in parsed['imports'])
    assert len(dlls) == len(set(dlls))
    assert {'mediaclientaudiostream.dll', 'surfacemanager.dll'} <= set(dlls)
    assert 'surfaceupdateclient.dll' not in dlls
    (r / 'e32-imports-verification.json').write_text(json.dumps({'status': 'PASS',
        'source_commit': spec['head_sha'], 'run_id': spec['run_id'], 'method': 'actual-e32-import-table',
        'browser_bytes': image.stat().st_size, 'browser_sha256': sha(image), 'dlls': dlls,
        'import_slots': sum(row['slots'] for row in parsed['imports']),
        'surfacemanager_required': True, 'surfaceupdateclient_forbidden': True,
        'mediaclientaudiostream_retained': True, 'parser': parser,
        'string_scan_used': False, 'guest_executed': False}, indent=2) + '\n')
    ci = load('ci-evidence.json')
    assert ci['status'] == 'PASS_EXACT_SOURCE_CI_SUMMARIES' and ci['source_commit'] == spec['head_sha'] and ci['run_id'] == spec['run_id']
    report = {'status': 'PASS_ACTUAL_CI_SIS_AND_SOURCE_CONTRACTS', 'source_commit': spec['head_sha'],
              'run_id': spec['run_id'], 'run_number': spec['run_number'], 'sis_version': spec['sis_version'],
              'artifact_id': artifact['id'], 'packages': 3, 'payloads': 13,
              'browser': {'bytes': image.stat().st_size, 'sha256': sha(image)},
              'source_snapshots_exact': True, 'ICU_actual_payload_host_checks_ran': True,
              'CI_ScreenPlay_host_suites': True, 'guest_executed': False,
              'phone_verified': False, 'GitHub_Release_published_by_branch_push': False}
    (r / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
