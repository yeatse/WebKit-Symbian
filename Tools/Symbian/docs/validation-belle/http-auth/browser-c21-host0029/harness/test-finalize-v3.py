#!/usr/bin/env python3
"""Integrated validators on temporary public synthetic evidence; no runtime."""
from pathlib import Path
import ast,hashlib,json,struct,tempfile
import finalize,mask_review as m,revised_validator as revised
import heading_review as h
from support import load_module
heading_tests=load_module('public_heading_fake_device',Path(__file__).resolve().parent/'test-heading-review.py')
ROOT=Path(__file__).resolve().parent
def write(path,value):path.write_text(json.dumps(value)+'\n')
# Reuse only the public synthetic fixture builder, not its test runner.
tree=ast.parse((ROOT/'test-revised-validator.py').read_text())
builder=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name=='setup']
context={'Path':Path,'json':json,'v':revised,'write':write,'DOMINO':'T source/thread.cpp:542 [Kernel]: Thread 1234abcd panicked with category: Domino and exit code: 13 \n'}
exec(compile(ast.Module(body=builder,type_ignores=[]),'public_fixture_builder','exec'),context)
checks=0;original_sha=m.sha(ROOT/'upstream/validate.py')
class Device:
    def __init__(self):self.text='public-only';self.selected=False
    def fep_window(self):return 17
    def geometry(self,w):return [241,185,400,300]
    def check(self):pass
    def check_focus(self,w):assert w==17
    def click_window(self,*args):pass
    def key(self,w,name,modifier=None):
        if modifier=='Control_L':self.selected=True
        elif name=='BackSpace':self.text='';self.selected=False
        else:self.text+=name
with tempfile.TemporaryDirectory(prefix='public-auth-finalize-') as directory:
    root=Path(directory);observer,_,report,drive,_=context['setup'](root,'success','basic');device=Device();proofs=[];records=[]
    hd=heading_tests.Device();headings=[]
    def head_capture(w,path):
        assert hd.text=='' and hd.fep==17 and hd.fep_geometry==[880,185,400,300]
        path.write_bytes(b'\x89PNG\r\n\x1a\n'+struct.pack('>I',13)+b'IHDR'+struct.pack('>II',900,700)+b'public-heading-only')
    def head_record(event,**fields):
        records.append(dict(event=event,**fields))
        if event=='public-heading-captured':h.approve(root/fields['gate'],123,fields['image_sha256'],'http://localhost:8794')
    def wait(c,description):
        value=c();assert value;return value
    h.perform_heading(hd,hd.key,{'fep_geometry':[241,185,400,300],'edit_point':[80,40],'native_editor_point':[450,278]},drive,123,root,headings,head_record,wait,1,head_capture,hd.move,[270,77,360,532])
    records.extend([{'event':'private-username-input-begins'},{'event':'editor-submitted','role':'username'}])
    def capture(window,path):
        assert device.text==m.PROBE
        path.write_bytes(b'\x89PNG\r\n\x1a\n'+struct.pack('>I',13)+b'IHDR'+struct.pack('>II',400,300)+b'public-test-only')
    def record(event,**fields):
        records.append(dict(event=event,**fields))
        if event=='public-mask-probe-captured':m.approve(root/fields['gate'],123,fields['image_sha256'])
    m.perform_probe(device,device.key,{'fep_geometry':[241,185,400,300],'edit_point':[80,135]},drive,123,root,proofs,record,1,capture)
    records.extend([{'event':'private-password-input-begins','instance':1},{'event':'editor-submitted','role':'password'},*drive['records']])
    drive.update(pid=123,records=records,public_probe_only_screenshots=True,password_mask_reviews=proofs,heading_reviews=headings)
    write(root/'interaction.json',drive)
    ready=json.loads((root/'fixture/ready.json').read_text());pins=json.loads((ROOT/'pins.json').read_text())['files']
    observer.update(run=ready['run'],scheme='basic',case='success',inputs={'browser':pins['browser'],'host':pins['host']},curl_trace_disabled=True,finished_utc='2026-11-01T00:00:00Z',normal_close=True,inputs_unchanged=True,private_config_profile_restored=True,restoration={'restored':True},status='OBSERVED_PENDING_STRICT_VALIDATOR',driver_contract={'status':'PENDING_STRICT_HTTP_AND_UI_VALIDATOR'})
    write(root/'observer-result.json',observer);review={k:report[k] for k in ('run','url',*finalize.FLAGS)};review['native_query_dismissed_observation']='Synthetic public trusted parent callback and same empty FEP preserved and restored for this run';write(root/'review.json',review)
    originals={p:p.read_bytes() for p in (root/'interaction.json',root/'host.log',root/'guest.log')}
    # finalize expects this retained emulator filename, without filtering.
    (root/'emulator.log').write_bytes((root/'host.log').read_bytes())
    result=finalize.finish(root,root/'review.json','positive')
    assert result['status']=='PASS' and result['strict_validator_status']=='FAIL';checks+=1
    strict=json.loads((root/'strict-validation-positive.json').read_text());assert strict['status']=='FAIL' and len(strict['failures'])==2;checks+=1
    assert all(p.read_bytes()==data for p,data in originals.items());checks+=1
    (root/'emulator.log').write_text('Thread WebKit panicked with category: EIKCTL and exit code: 13\n')
    assert finalize.finish(root,root/'review.json','target-panic')['status']=='FAIL';checks+=1
    (root/'emulator.log').write_bytes((root/'host.log').read_bytes());proofs[0]['approved']=False;proofs[0]['cleared_before_private_input']=False;write(root/'interaction.json',drive)
    assert finalize.finish(root,root/'review.json','unreviewed')['status']!='PASS';checks+=1
assert m.sha(ROOT/'upstream/validate.py')==original_sha;checks+=1
print(json.dumps({'status':'PASS','checks':checks,'scope':'temporary public synthetic fixture; no actual credentials, server, display or guest'},indent=2))
