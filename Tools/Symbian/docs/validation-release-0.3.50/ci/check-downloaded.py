#!/usr/bin/env python3
from pathlib import Path
import binascii, datetime, hashlib, json, zipfile

r=Path(__file__).resolve().parent
load=lambda n:json.loads((r/n).read_text())
sha=lambda data:hashlib.sha256(data).hexdigest()
run=load('run.json'); jobs=load('jobs.json'); artifacts=load('artifacts.json')
assert run['id']==41299027063 and run['head_sha']=='886d227f1780298cc17cf99695a3c78c0b7d9ff6'
assert run['head_branch']=='2.52.6-symbian' and run['event']=='push'
assert run['status']=='completed' and run['conclusion']=='success'
job=next(j for j in jobs['jobs'] if j['id']==124825909684)
assert job['status']=='completed' and job['conclusion']=='success'
artifact=next(a for a in artifacts['artifacts'] if a['name']=='sis')
assert not artifact['expired']
assert artifact['workflow_run']['id']==run['id']
assert artifact['workflow_run']['head_sha']==run['head_sha']
assert artifact['workflow_run']['head_branch']==run['head_branch']
archive=r/'ci50-sis.zip';data=archive.read_bytes()
assert len(data)==artifact['size_in_bytes'] and 'sha256:'+sha(data)==artifact['digest']
with zipfile.ZipFile(archive) as z:
 assert set(z.namelist())=={'SHA256SUMS','browser.sis','jsc.sis','symbian-webkit-icu.sis'}
 assert z.testzip() is None
 target=r/'sis';target.mkdir(exist_ok=True)
 for name in z.namelist():
  value=z.read(name);p=target/name
  assert not p.exists() or p.read_bytes()==value
  p.write_bytes(value)
 report={'status':'PASS','artifact_id':artifact['id'],'bytes':len(data),'sha256':sha(data),
  'matches_github_digest':True,'members':z.namelist(),'zip_member_crc_valid':True,
  'workflow_head_sha':run['head_sha'],'workflow_conclusion':run['conclusion'],
  'job_id':job['id'],'job_conclusion':job['conclusion']}
(r/'archive-verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
