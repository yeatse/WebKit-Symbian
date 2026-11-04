#!/usr/bin/env python3
"""Materialize a connector-owned HTTPS file reference without logging its URL."""
import getpass
import hashlib
import json
from pathlib import Path
import urllib.error
import urllib.parse
import urllib.request

root = Path(__file__).resolve().parent
artifact = next(row for row in json.loads((root / 'artifacts.json').read_text())['artifacts'] if row['name'] == 'sis')
target = root / 'sis.zip'
assert not target.exists()
value = getpass.getpass('Artifact file reference: ')
parsed = urllib.parse.urlsplit(value)
assert parsed.scheme == 'https' and parsed.hostname and parsed.username is None and parsed.password is None
report = {'artifact_id': artifact['id'], 'file_name': 'sis.zip', 'primary': 'GitHub plugin download_workflow_artifact',
          'credential_values_or_signed_download_query_archived': False,
          'temporary_download_reference_written_to_disk': False, 'ELF_downloaded': False}
try:
    digest = hashlib.sha256()
    count = 0
    with urllib.request.urlopen(value, timeout=60) as response, target.open('xb') as output:
        while True:
            data = response.read(1024 * 1024)
            if not data:
                break
            count += len(data)
            if count > artifact['size_in_bytes'] or count > 100 * 1024 * 1024:
                raise ValueError('archive size bound')
            digest.update(data)
            output.write(data)
    assert count == artifact['size_in_bytes'] and 'sha256:' + digest.hexdigest() == artifact['digest']
    report.update(status='PASS_MATERIALIZED_DIGEST_MATCH', materialized_bytes=count, sha256=digest.hexdigest())
except urllib.error.HTTPError as error:
    report.update(status='FAIL', error_code='FILE_REFERENCE_HTTP_ERROR', http_status=error.code)
except BaseException as error:
    report.update(status='FAIL', error_code='FILE_REFERENCE_MATERIALIZATION_FAILED', exception_class=type(error).__name__)
(root / 'acquisition.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
raise SystemExit(0 if report['status'] == 'PASS_MATERIALIZED_DIGEST_MATCH' else 1)
