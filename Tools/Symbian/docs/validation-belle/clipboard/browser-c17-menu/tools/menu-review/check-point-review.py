#!/usr/bin/env python3
"""Pure guard checks: never starts X11, EKA or guest code."""
import copy,importlib.util,json
from pathlib import Path
root=Path(__file__).parent;spec=importlib.util.spec_from_file_location('point_review',root/'review-drive.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
gate={'run':'host-point-review','step':1,'operation':'paste','expected_label':'Paste','opened':{'sha256':'1'*64},'pid':600,'window':'0x60000b','geometry':[1,25,900,700]}
value={'run':gate['run'],'step':1,'operation':'paste','observed_label':'Paste','opened_sha256':'1'*64,'pid':600,'window':'0x60000b','geometry':gate['geometry'],'point':[320,444],'screenshots_reviewed':True}
assert module.validate_point_review(value,gate,600,0x60000b,gate['geometry'])==[320,444]
checks=1
for changes in [{'pid':601},{'geometry':[2,25,900,700]},{'opened_sha256':'2'*64},{'window':'0x60000c'},{'observed_label':'Exit'},{'run':'different-run'},{'point':[900,444]},{'point':[320,-1]},{'point':[320.0,444]},{'point':[True,444]},{'screenshots_reviewed':False},{'key':'Return'}]:
 candidate=copy.deepcopy(value);candidate.update(changes)
 try:module.validate_point_review(candidate,gate,600,0x60000b,gate['geometry'])
 except RuntimeError:checks+=1
 else:raise AssertionError('bad review was accepted: '+str(changes))
print(json.dumps({'status':'PASS','checks':checks,'scope':'pure screenshot/PID/geometry/point guard; no X11/emulator/guest used'}))
