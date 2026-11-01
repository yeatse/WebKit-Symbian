#!/usr/bin/env python3
"""Root calls only after viewing actual opened/end/selected menu screenshots."""
from pathlib import Path
import argparse,json,hashlib
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--gate',type=Path,required=True);p.add_argument('--label',required=True);p.add_argument('--selected-sha256',required=True);a=p.parse_args();g=json.loads(a.gate.read_text());selected=g['frames']['selected']
if a.label!=g['expected_label'] or a.selected_sha256!=selected['sha256'] or hashlib.sha256(Path(selected['path']).read_bytes()).hexdigest()!=a.selected_sha256:raise SystemExit('review identity/observed label mismatch; refusing Return approval')
value={'run':g['run'],'step':g['step'],'operation':g['operation'],'observed_label':a.label,'selected_sha256':a.selected_sha256,'screenshots_reviewed':True}
with (a.gate.parent/'approval.json').open('x') as f:json.dump(value,f,indent=2);f.write('\n')
print('Approved exact screenshot for '+a.label+'; input driver still must prove trusted page operation')
