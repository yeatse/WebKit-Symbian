#!/usr/bin/env python3
"""Pure host guards: temporary public data and fake device, no display."""
from pathlib import Path
from unittest.mock import patch
import json,struct,tempfile,unittest
import mask_review as m

class Device:
    def __init__(self):self.window=17;self.text='public-initial-draft';self.selected=False;self.keys=[]
    def fep_window(self):return self.window
    def geometry(self,window):return [241,185,400,300]
    def check(self):return None
    def check_focus(self,window):m.require(window==self.window,'foreign focus')
    def click_window(self,window,geometry,x,y):self.check_focus(window)
    def key(self,window,name,modifier=None):
        self.check_focus(window);self.keys.append((name,modifier))
        if modifier=='Control_L' and name=='a':self.selected=True
        elif name=='BackSpace':self.text='' if self.selected else self.text[:-1];self.selected=False
        else:
            if self.selected:self.text='';self.selected=False
            self.text+=name

class Guards(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name);self.x=Device();self.proofs=[];self.events=[];self.records=[]
        self.ready={'run':'public21','scheme':'basic','case':'success'};self.ui={'fep_geometry':[241,185,400,300],'edit_point':[80,135]}
    def tearDown(self):self.temp.cleanup()
    def capture(self,window,path):
        self.assertEqual(window,17);self.assertEqual(self.x.text,m.PROBE)
        path.write_bytes(b'\x89PNG\r\n\x1a\n'+struct.pack('>I',13)+b'IHDR'+struct.pack('>II',400,300)+b'public-test-only')
    def record(self,event,**fields):
        self.events.append(event)
        self.records.append(dict(event=event,**fields))
        if event=='public-mask-probe-captured':m.approve(self.root/fields['gate'],41,fields['image_sha256'])
    def perform(self,record=None):
        m.perform_probe(self.x,self.x.key,self.ui,self.ready,41,self.root,self.proofs,record or self.record,1,self.capture)
    def interaction(self,case=None):
        records=self.records+[{'event':'private-password-input-begins','instance':1},{'event':'editor-submitted','role':'password'}]
        return dict(self.ready,case=case or self.ready['case'],pid=41,screenshots_taken=bool(self.proofs),credential_stage_screenshots=False,public_probe_only_screenshots=True,password_mask_reviews=self.proofs,records=records)
    def gate(self):return self.root/self.proofs[0]['gate']
    def test_clear_capture_review_clear_order(self):
        self.perform();self.assertEqual(self.x.text,'');self.assertEqual(len(self.proofs),1)
        self.assertEqual(m.validate_probe_evidence(self.interaction(),self.root)['status'],'PASS')
        self.assertEqual(self.events,['public-mask-probe-cleared-before-capture','public-mask-probe-captured','public-mask-probe-approved-and-cleared'])
    def test_no_review_never_reaches_private_input(self):
        with patch.object(m.time,'monotonic',side_effect=[0,2]):
            with self.assertRaisesRegex(RuntimeError,'no credential entered'):self.perform(lambda *a,**k:None)
        self.assertEqual(self.x.text,m.PROBE);self.assertFalse(self.proofs[0]['approved'])
        self.assertEqual(m.validate_probe_evidence(self.interaction(),self.root)['status'],'INCOMPLETE')
    def test_wrong_pid(self):
        def record(event,**fields):
            if event=='public-mask-probe-captured':m.approve(self.root/fields['gate'],42,fields['image_sha256'])
        with self.assertRaisesRegex(RuntimeError,'PID'):self.perform(record)
        self.assertEqual(self.x.text,m.PROBE)
    def test_wrong_image_hash(self):
        def record(event,**fields):
            if event=='public-mask-probe-captured':m.approve(self.root/fields['gate'],41,'0'*64)
        with self.assertRaisesRegex(RuntimeError,'SHA'):self.perform(record)
    def test_changed_fep(self):
        def record(event,**fields):
            if event=='public-mask-probe-captured':self.x.window=18
        with self.assertRaisesRegex(RuntimeError,'FEP changed'):self.perform(record)
        self.assertEqual(self.x.text,m.PROBE)
    def test_changed_image_after_review(self):
        def record(event,**fields):
            self.record(event,**fields)
            if event=='public-mask-probe-captured':
                p=self.root/self.proofs[0]['image'];p.write_bytes(p.read_bytes()+b'changed')
        with self.assertRaisesRegex(RuntimeError,'image changed'):self.perform(record)
        self.assertEqual(self.x.text,m.PROBE)
    def test_unmasked_approval(self):
        def record(event,**fields):
            self.record(event,**fields)
            if event=='public-mask-probe-captured':
                p=self.gate().parent/'approval.json';a=json.loads(p.read_text());a['observed_masked']=False;p.write_text(json.dumps(a))
        with self.assertRaisesRegex(RuntimeError,'review does not match'):self.perform(record)
    def test_approval_replay(self):
        self.perform();gate,_=m.check_gate(self.gate())
        with self.assertRaises(FileExistsError):m.approve(self.gate(),41,gate['image_sha256'])
    def test_absolute_evidence_path(self):
        self.perform();self.proofs[0]['gate']=str(self.gate().resolve())
        self.assertEqual(m.validate_probe_evidence(self.interaction(),self.root)['status'],'FAIL')
    def test_symlinked_image(self):
        self.perform();p=self.root/self.proofs[0]['image'];other=self.root/'other-public.png';p.rename(other);p.symlink_to(other)
        self.assertEqual(m.validate_probe_evidence(self.interaction(),self.root)['status'],'FAIL')
    def test_retry_requires_three_current_reviews(self):
        self.perform();self.assertNotEqual(m.validate_probe_evidence(self.interaction('retry'),self.root)['status'],'PASS')
    def test_username_only_zero_images(self):
        data=dict(self.ready,case='cancel-user',pid=41,screenshots_taken=False,credential_stage_screenshots=False,public_probe_only_screenshots=True,password_mask_reviews=[])
        self.assertEqual(m.validate_probe_evidence(data,self.root)['status'],'PASS')
    def test_false_screenshot_claim(self):
        self.perform();data=self.interaction();data['screenshots_taken']=False
        self.assertEqual(m.validate_probe_evidence(data,self.root)['status'],'FAIL')
    def test_uncleared_probe_incomplete(self):
        self.perform();self.proofs[0]['cleared_before_private_input']=False
        self.assertEqual(m.validate_probe_evidence(self.interaction(),self.root)['status'],'INCOMPLETE')
    def test_unknown_approval_field(self):
        self.perform();p=self.gate().parent/'approval.json';a=json.loads(p.read_text());a['unknown']=True;p.write_text(json.dumps(a))
        self.assertEqual(m.validate_probe_evidence(self.interaction(),self.root)['status'],'FAIL')
    def test_private_input_before_review(self):
        self.perform();data=self.interaction();data['records'].insert(0,{'event':'private-password-input-begins','instance':1})
        self.assertEqual(m.validate_probe_evidence(data,self.root)['status'],'FAIL')
    def test_unrelated_image_rejected(self):
        self.perform();(self.root/'other-image.png').write_bytes(b'public-test-only')
        self.assertEqual(m.validate_probe_evidence(self.interaction(),self.root)['status'],'FAIL')

if __name__=='__main__':unittest.main()
