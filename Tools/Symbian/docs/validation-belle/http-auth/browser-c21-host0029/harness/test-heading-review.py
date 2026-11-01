#!/usr/bin/env python3
"""Synthetic empty public query only; no display, guest, network, or existing secrets."""
from pathlib import Path
from unittest.mock import patch
import json,struct,tempfile,unittest
import heading_review as h
from public_review import validate_public_evidence

class X:
 def XRaiseWindow(self,*a):pass
 def XSetInputFocus(self,*a):pass
 def XSync(self,*a):pass
class Device:
 def __init__(self):self.window=19;self.root=1;self.fep=17;self.fep_geometry=[241,185,400,300];self.moves=[];self.text='';self.selected=False;self.x=X();self.display=None;self.keys=[]
 def fep_window(self):return self.fep
 def geometry(self,w):return self.fep_geometry.copy() if w==17 else [0,0,1280,900] if w==1 else [1,25,900,700]
 def check(self):return [1,25,900,700]
 def check_focus(self,w):h.require(w==(self.fep or self.window),'focus changed')
 def click_window(self,w,g,*point):self.check_focus(w)
 def click(self,*p):raise AssertionError('native reopen must never be attempted')
 def move(self,device,w,g):h.require(device is self and w==17,'wrong move owner');self.fep_geometry=g.copy();self.moves.append(g.copy())
 def key(self,w,name,modifier=None):
  self.check_focus(w);self.keys.append((name,modifier))
  if name=='Escape':raise AssertionError('empty FEP must stay alive')
  elif modifier=='Control_L' and name=='a':self.selected=True
  elif name=='BackSpace':self.text='' if self.selected else self.text[:-1];self.selected=False
  else:self.text+=name
class Tests(unittest.TestCase):
 def setUp(self):
  self.t=tempfile.TemporaryDirectory();self.root=Path(self.t.name);self.x=Device();self.p=[];self.records=[];self.ready={'run':'publichead21','scheme':'basic','case':'cancel-user','url':'http://localhost:8794/?run=publichead21'};self.ui={'fep_geometry':[241,185,400,300],'edit_point':[80,40],'native_editor_point':[450,278]}
 def tearDown(self):self.t.cleanup()
 def capture(self,w,path):
  self.assertEqual(w,19);self.assertEqual(self.x.fep,17);self.assertEqual(self.x.fep_geometry,[880,185,400,300]);self.assertEqual(self.x.text,'');path.write_bytes(b'\x89PNG\r\n\x1a\n'+struct.pack('>I',13)+b'IHDR'+struct.pack('>II',900,700)+b'public-only')
 def record(self,event,**fields):
  self.records.append(dict(event=event,**fields))
  if event=='public-heading-captured':h.approve(self.root/fields['gate'],41,fields['image_sha256'],'http://localhost:8794')
 def wait(self,condition,description):v=condition();h.require(v,description);return v
 def perform(self,record=None):h.perform_heading(self.x,self.x.key,self.ui,self.ready,41,self.root,self.p,record or self.record,self.wait,1,self.capture,self.x.move,[270,77,360,532])
 def data(self):return dict(self.ready,pid=41,heading_reviews=self.p,records=self.records+[{'event':'private-username-input-begins'},{'event':'editor-submitted','role':'username'}])
 def test_fresh_empty_capture_move_approval_restore(self):
  self.perform();self.assertEqual(self.x.fep,17);self.assertEqual(h.validate_heading(self.data(),self.root)[:2],([],[]));self.assertNotIn(('Escape',None),self.x.keys);self.assertEqual(self.x.moves,[[880,185,400,300],[241,185,400,300]])
 def test_timeout_no_username_typed(self):
  with patch.object(h.time,'monotonic',side_effect=[0,2]):
   with self.assertRaisesRegex(RuntimeError,'no private username'):self.perform(lambda *a,**k:None)
  self.assertEqual(self.x.fep,17);self.assertEqual(self.x.fep_geometry,[241,185,400,300]);self.assertFalse(self.p[0]['approved'])
 def test_undersized_screen_refused(self):
  old=self.x.geometry;self.x.geometry=lambda w:[0,0,900,900] if w==1 else old(w)
  with self.assertRaisesRegex(RuntimeError,'no clear public'):self.perform()
  self.assertFalse(self.x.moves)
 def test_wrong_pid(self):
  def rec(e,**f):
   if e=='public-heading-captured':h.approve(self.root/f['gate'],42,f['image_sha256'],'http://localhost:8794')
  with self.assertRaisesRegex(RuntimeError,'differs'):self.perform(rec)
 def test_wrong_origin(self):
  def rec(e,**f):
   if e=='public-heading-captured':h.approve(self.root/f['gate'],41,f['image_sha256'],'http://unknown:8794')
  with self.assertRaisesRegex(RuntimeError,'differs'):self.perform(rec)
 def test_username_input_before_review(self):
  self.perform();d=self.data();d['records'].insert(0,{'event':'private-username-input-begins'});self.assertTrue(h.validate_heading(d,self.root)[0])
 def test_gate_changed(self):
  self.perform();p=self.root/self.p[0]['gate'];g=json.loads(p.read_text());g['pid']=42;p.write_text(json.dumps(g));self.assertTrue(h.validate_heading(self.data(),self.root)[0])
 def test_approval_replay(self):
  self.perform()
  with self.assertRaises(FileExistsError):h.approve(self.root/self.p[0]['gate'],41,self.p[0]['image_sha256'],'http://localhost:8794')
 def test_unknown_heading_field(self):
  self.perform();self.p[0]['unknown']=True;self.assertTrue(h.validate_heading(self.data(),self.root)[0])
 def test_public_heading_allowed_other_image_rejected(self):
  self.perform();d=self.data();d.update(credential_stage_screenshots=False,public_probe_only_screenshots=True,password_mask_reviews=[],screenshots_taken=True)
  # Exit bypasses closure, but would alter gate identity, so provide real synthetic closure.
  p=dict(kind='trusted-parent-button',run=self.ready['run'],pid=41,window='0x13',geometry=[1,25,900,700],fep_absent=True,trusted=True,source='top-parent',page_path='/',recorded_after_interaction=True);d['native_query_closure']=p;d['records'] += [{'event':'quiet-observation','seconds':5},{'event':'trusted-parent-probe-clicked'},dict(event='trusted-parent-probe',**p)]
  self.assertEqual(validate_public_evidence(d,self.root)['status'],'PASS');(self.root/'unexpected.png').write_bytes(b'public');self.assertEqual(validate_public_evidence(d,self.root)['status'],'FAIL')
if __name__=='__main__':unittest.main()
