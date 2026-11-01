#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,re
r=Path(__file__).resolve().parent
load=lambda name:json.loads((r/name).read_text())
source=load('native-patch-stack-source-identities.json')
assert source['head_sha']=='e0e2f321d4e2d4eac98c33c3e51eca0f43ffaac5'
jobs=load('jobs.json');job=next(j for j in jobs['jobs'] if j['id']==124767283934)
step=next(s for s in job['steps'] if s['name']=='E32 converter')
assert step['status']=='completed' and step['conclusion']=='success'
raw=(r/'job.log').read_text()
text=re.sub(r'^\d{4}-\d\d-\d\dT[0-9:.]+Z ?', '',raw,flags=re.M)
found=[];decoder=json.JSONDecoder()
for match in re.finditer(r'\{\s*\n\s*"status": "(?:APPLIED|ALREADY_APPLIED)"',text):
 value,count=decoder.raw_decode(text[match.start():])
 if value.get('patch_count')==6 and value.get('patches')==source['patches']:
  assert value['base_commit']=='b105f752f84d1b5640052f64c4a703017810d7b8'
  assert value['verified_files']==source['verified_files']
  found.append((match.start(),match.start()+count,value))
assert len(found)==2, 'Expected both full-stack helper reports in actual CI logs'
first,last=(value for _,_,value in found)
assert first['status'] in ['APPLIED','ALREADY_APPLIED'] and 0<=first['from_prefix']<=6
assert last['status']=='ALREADY_APPLIED' and last['from_prefix']==6
between=text[found[0][1]:found[1][0]]
assert '100% tests passed, 0 tests failed out of 1' in between
assert re.search(r'symbian_e32_test.*Passed',between)
excerpt=text[found[0][0]:found[1][1]]+'\n'
(r/'native-patch-stack-ci.log').write_text(excerpt)
report={'status':'PASS','head_sha':source['head_sha'],'run_id':41279144446,'job_id':124767283934,
 'step':'E32 converter','step_conclusion':'success','patch_count':6,
 'base_commit':last['base_commit'],'verified_files':last['verified_files'],
 'first_build_status':first['status'],'first_build_from_prefix':first['from_prefix'],
 'native_E32_ctest_passed':True,'post_build_status':last['status'],'post_build_from_prefix':last['from_prefix'],
 'patches':last['patches'],'actual_CI_log_excerpt_sha256':hashlib.sha256(excerpt.encode()).hexdigest(),
 'scope':'Actual CI native converter step and repeated full postimage verification; no guest or SDK mutation'}
(r/'native-patch-stack-ci-verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
