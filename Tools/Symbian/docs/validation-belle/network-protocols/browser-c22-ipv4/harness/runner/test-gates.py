#!/usr/bin/env python3
"""Validator-only synthetic records; no display/guest/server/runtime activity."""
import copy,importlib.util,unittest
from pathlib import Path
p=Path(__file__).with_name('run.py');s=importlib.util.spec_from_file_location('network_wrapper',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
class GateTests(unittest.TestCase):
 def setUp(self):self.good={'host_exit':0,'wm_close_sent':True,'wm_close_after_functional':True,'forced_cleanup':False,'deadline_expired':False,'inputs_unchanged':True,'restoration':{'guest':{'restored':True},'configuration':{'restored':True}}}
 def check_bad(self,key,value):
  r=copy.deepcopy(self.good);r[key]=value
  with self.assertRaises(RuntimeError):m.completion_gate(r,b'',b'',{'status':'PASS'})
 def test_complete_synthetic(self):self.assertEqual(m.completion_gate(self.good,b'',b'',{'status':'PASS'})['status'],'PASS')
 def test_host_zero_cannot_hide_target_panic(self):
  with self.assertRaises(RuntimeError):m.completion_gate(self.good,b'Thread WebKit panicked with code 3',b'',{'status':'PASS'})
 def test_nonzero_guest_is_not_normal(self):
  with self.assertRaises(RuntimeError):m.completion_gate(self.good,b'Thread Main forcefully killed with category: None and exit code: 1',b'',{'status':'PASS'})
 def test_no_close_before_functional(self):self.check_bad('wm_close_after_functional',False)
 def test_forced_cleanup(self):self.check_bad('forced_cleanup',True)
 def test_deadline(self):self.check_bad('deadline_expired',True)
 def test_host_nonzero(self):self.check_bad('host_exit',1)
 def test_pins_changed(self):self.check_bad('inputs_unchanged',False)
 def test_restore_incomplete(self):
  self.good['restoration']['guest']['restored']=False
  with self.assertRaises(RuntimeError):m.completion_gate(self.good,b'',b'',{'status':'PASS'})
 def test_functional_fail(self):
  with self.assertRaises(RuntimeError):m.completion_gate(self.good,b'',b'',{'status':'FAIL'})
 def test_fresh_browser_result(self):
  self.assertIsNone(m.result_record(b'console: NETWORK-GUEST-RESULT {"run":"old","kind":"result","status":"PASS"}','fresh'))
  self.assertIsNone(m.result_record(b'console: NETWORK-GUEST-RESULT {"run":"fresh","kind":"ready","status":"READY"}','fresh'))
  self.assertEqual(m.result_record(b'console: NETWORK-GUEST-RESULT {"run":"fresh","kind":"result","status":"PASS"}','fresh')['status'],'PASS')
 def test_duplicate_result(self):
  line=b'console: NETWORK-GUEST-RESULT {"run":"fresh","kind":"result","status":"PASS"}\n'
  with self.assertRaises(RuntimeError):m.result_record(line+line,'fresh')
if __name__=='__main__':unittest.main()
