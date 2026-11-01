#!/usr/bin/env python3
"""Host-only guards, no DISPLAY/HTTP/guest. Synthetic inputs contain no credentials."""
import importlib.util,json,tempfile,types
from pathlib import Path
base=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('driver',base/'drive.py');d=importlib.util.module_from_spec(spec);spec.loader.exec_module(d)
checks=0
def rejects(fn,desc):
 global checks
 try:fn()
 except (ValueError,RuntimeError):checks+=1;return
 raise AssertionError('accepted invalid '+desc)
with tempfile.TemporaryDirectory(prefix='public-secret-contract-') as td:
 t=Path(td);rejects(lambda:d.pins(base/'pins.template.json'),'unset pins')
 reject=base/'calibration.template.json';rejects(lambda:d.calibration(reject),'unreviewed calibration')
 c=json.loads(reject.read_text());c['main_reviewed_actual_screenshot']=True;p=t/'cal.json';p.write_text(json.dumps(c));assert d.calibration(p)['main_size']==[900,700];checks+=1
 rejects(lambda:d.calibration(p,True),'unreviewed secret modal')
 c.update(fep_reviewed_actual_screenshot=True,fep_size=[400,100],edit_point=[30,25],submit_point=[200,75]);p.write_text(json.dumps(c));assert d.calibration(p,True)['edit_point']==[30,25];checks+=1
 c['edit_point']=[400,25];p.write_text(json.dumps(c));rejects(lambda:d.calibration(p,True),'out of bounds secret editor')
 c.update(edit_point=[30,25],content_rect=[270,77,999,532]);p.write_text(json.dumps(c));rejects(lambda:d.calibration(p),'out of bounds main content')
 log=t/'public.log';a=types.SimpleNamespace(run='public_contract',case='cancel',guest_log=log)
 log.write_text('SECRET-FEP stale cancel PAGE-PASS {}\n');assert d.events(a)==[];checks+=1
 log.write_text('SECRET-FEP public_contract cancel READY {}\nSECRET-FEP public_contract cancel FAIL {"error":"public assertion"}\n');rejects(lambda:d.events(a),'page failure')
 rows=[{'kind':'READY','payload':{}},{'kind':'READY','payload':{}}];rejects(lambda:d.event(rows,'READY'),'duplicate READY')
 # Review the actual deployed pins without opening X or touching a guest.
 actual=d.pins(base/'pins.c19-host0029.json');assert len(actual)==3;checks+=1
print(str(checks)+' host contract checks passed; no UI/server/guest started')
