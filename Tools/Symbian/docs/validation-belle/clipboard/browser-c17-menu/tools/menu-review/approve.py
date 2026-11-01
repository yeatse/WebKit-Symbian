#!/usr/bin/env python3
"""Root calls only after viewing this gate's actual opened menu screenshot."""
from pathlib import Path
import argparse,json,hashlib
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--gate',type=Path,required=True);p.add_argument('--label',required=True);p.add_argument('--opened-sha256',required=True);p.add_argument('--pid',type=int,required=True);p.add_argument('--point',nargs=2,type=int,required=True);a=p.parse_args();g=json.loads(a.gate.read_text());opened=g['opened'];geometry=g['geometry']
if a.label!=g['expected_label'] or a.opened_sha256!=opened['sha256'] or hashlib.sha256(Path(opened['path']).read_bytes()).hexdigest()!=a.opened_sha256 or a.pid!=g['pid']:raise SystemExit('review identity/observed label/PID mismatch; refusing click approval')
if not (0<=a.point[0]<geometry[2] and 0<=a.point[1]<geometry[3]):raise SystemExit('point outside captured main window')
value={'run':g['run'],'step':g['step'],'operation':g['operation'],'observed_label':a.label,'opened_sha256':a.opened_sha256,'pid':a.pid,'window':g['window'],'geometry':geometry,'point':a.point,'screenshots_reviewed':True}
with (a.gate.parent/'approval.json').open('x') as f:json.dump(value,f,indent=2);f.write('\n')
print('Approved exact screenshot point for '+a.label+'; page must still prove trusted operation')
