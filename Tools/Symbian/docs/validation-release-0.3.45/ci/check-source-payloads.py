#!/usr/bin/env python3
from pathlib import Path
import hashlib,json
r=Path(__file__).resolve().parent
sha=lambda data:hashlib.sha256(data).hexdigest()
prepared=json.loads((r/'prepared-source-identities.json').read_text())
checks=[]
for entry in prepared['sources']:
 source=(r/'source-inputs'/entry['source_path']).read_bytes()
 name=Path(entry['source_path']).name;payload=(r/'payloads'/name).read_bytes()
 assert source==payload and sha(source)==entry['sha256']
 checks.append({'payload':name,**entry,'exact_bytes_match':True})
for name,count,expected,flag in [
 ('cacert.pem',188900,'a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505','pinned_release_ca_matches'),
 ('icudt70l.dat',10261824,'808fc9bd37589371288b409a8433cf3434ba792ca9d640109c5da41454895d0d','repaired_UTS46_data_matches')]:
 data=(r/'payloads'/name).read_bytes();assert len(data)==count and sha(data)==expected
 checks.append({'payload':name,'bytes':len(data),'sha256':sha(data),flag:True})
report={'status':'PASS','head_sha':prepared['head_sha'],'checks':checks}
(r/'source-payload-verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
