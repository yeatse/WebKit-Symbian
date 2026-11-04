#!/usr/bin/env python3
"""Read public CI job stdout from stdin; persist only whitelisted summaries."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('work', type=Path)
    a = p.parse_args()
    r = a.work.resolve()
    load = lambda name: json.loads((r / name).read_text())
    spec = load('spec.json')
    raw = sys.stdin.buffer.read(64 * 1024 * 1024 + 1)
    assert len(raw) <= 64 * 1024 * 1024
    # No raw CI log is written to disk or printed.
    clean = re.sub(r'^\d{4}-\d\d-\d\dT[0-9:.]+Z ?', '', raw.decode('utf-8'), flags=re.M)
    decoder = json.JSONDecoder()
    records = []
    for match in re.finditer(r'^\s*\{', clean, flags=re.M):
        try:
            value, length = decoder.raw_decode(clean[match.start():].lstrip())
        except json.JSONDecodeError:
            continue
        if isinstance(value, dict):
            records.append((match.start(), value))
    native = load('native-patch-stack-source-identities.json')
    assert native['head_sha'] == spec['head_sha'] and len(native['patches']) == 10 and native['verified_files'] == 20
    stacks = [(pos, obj) for pos, obj in records if obj.get('status') in ['APPLIED', 'ALREADY_APPLIED']
              and obj.get('patch_count') == 10 and obj.get('patches') == native['patches']]
    assert len(stacks) == 2
    first, last = stacks[0][1], stacks[1][1]
    for obj in [first, last]:
        assert obj['verified_files'] == 20 and obj['base_commit'] == 'b105f752f84d1b5640052f64c4a703017810d7b8'
    assert 0 <= first['from_prefix'] <= 10 and last['status'] == 'ALREADY_APPLIED' and last['from_prefix'] == 10
    between = clean[stacks[0][0]:stacks[1][0]]
    assert '100% tests passed, 0 tests failed out of 1' in between
    assert re.search(r'symbian_e32_test.*Passed', between)
    assert re.search(r'Ran 21 tests in [0-9.]+s', clean)
    math = [(pos, obj) for pos, obj in records if obj.get('status') == 'INSTALLED'
            and obj.get('profiles') == ['default', 'streams', 'native_atomic64'] and obj.get('installed') is True]
    complex_rows = [(pos, obj) for pos, obj in records if obj.get('status') == 'INSTALLED'
                    and obj.get('upstream_revision') == '85ac560262434c9ccfc0c183ec22d4138ed647fb' and obj.get('installed') is True]
    assert len(math) == 1 and len(complex_rows) == 1 and math[0][0] < complex_rows[0][0]
    complex_manifest = json.loads((r / 'build-source/Tools/Symbian/patches/sdk-0.3.0/complex/manifest.json').read_text())
    assert complex_rows[0][1]['source_files'] == complex_manifest['upstream_files']
    assert complex_rows[0][1]['original_members_unchanged'] == 296
    source_rows = load('screenplay-source-identities.json')['files']
    source = {row['path']: row['sha256'] for row in source_rows}
    presenters = [obj for _, obj in records if obj.get('status') == 'PASS_ACTUAL_SOURCE_OWNED_HOST_ADAPTER']
    clients = [obj for _, obj in records if obj.get('result') == 'pass' and obj.get('cases') == 8 and obj.get('checks') == 75]
    assert len(presenters) == 1 and len(clients) == 1
    presenter, client = presenters[0], clients[0]
    assert presenter['cases'] == 47 and presenter['assertions'] == 298 and presenter['sanitizer'] == 'undefined'
    assert presenter['build_exit'] == 0 and presenter['test_exit'] == 0
    assert presenter['guest_run'] is False and presenter['display_verified'] is False
    assert isinstance(presenter['source_files'], list) and len(presenter['source_files']) == 2
    assert {row['name'] for row in presenter['source_files']} == {'ScreenPlaySurface.h', 'ScreenPlaySurface.cpp'}
    for row in presenter['source_files']:
        assert row['sha256'] == source['Tools/Symbian/browser/' + row['name']]
    assert set(client['source_files']) == {'ScreenPlayUpdateSession.cpp', 'ScreenPlayUpdateSession.h', 'ScreenPlayUpdateProtocol.h'}
    assert client['guest_executed'] is False
    for name, digest in client['source_files'].items():
        assert digest == source['Tools/Symbian/browser/' + name]
    assert client['sanitizers'] == []  # CI default has no --sanitize; local ASan is separate evidence.
    installed = [obj for _, obj in records if obj.get('status') == 'PASS_INSTALLED'
                 and 'files_added' in obj and 'app_include_added' in obj
                 and obj.get('runtime_archives_changed') is False and obj.get('native_target_executed') is False]
    assert len(installed) == 1
    network = {name: clean.splitlines().count('== ' + name) for name in ['nghttp2', 'curl']}
    assert all(count in [0, 1] for count in network.values())
    report = {'status': 'PASS_EXACT_SOURCE_CI_SUMMARIES', 'source_commit': spec['head_sha'], 'run_id': spec['run_id'],
        'native_stack': {'patches': 10, 'owned_files': 20, 'host_tests': 21,
            'first_status': first['status'], 'first_prefix': first['from_prefix'],
            'post_status': last['status'], 'post_prefix': last['from_prefix'], 'native_E32_ctest': True},
        'math_profiles': math[0][1]['profiles'], 'complex_original_members_retained': 296,
        'complex_sources_match_exact_manifest': True, 'math_before_complex': True,
        'source_gated_math_complex_install': True, 'raw_nm_or_member_tables_audited': False,
        'presenter': {'cases': 47, 'assertions': 298, 'sanitizer': 'undefined', 'source_hashes_match_exact_commit': True},
        'private_client': {'cases': 8, 'assertions': 75, 'sanitizers': [], 'source_hashes_match_exact_commit': True},
        'SDK_ScreenPlay_interfaces': {key: installed[0][key] for key in ['status', 'files_added', 'app_include_added', 'runtime_archives_changed', 'native_target_executed']},
        'dependency_build_invocation_markers': network,
        'dependency_build_invocation_observed': all(value == 1 for value in network.values()),
        'fresh_object_rebuild_verified': False,
        'dependency_cache_or_skip_is_not_fresh_build_proof': True,
        'raw_log_persisted': False, 'guest_executed': False, 'phone_verified': False}
    (r / 'ci-evidence.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
