#!/usr/bin/env python3
"""Actual pinned public staging and fixed diagnostics. No guest/private reads."""
import importlib.util
from pathlib import Path
import tempfile,unittest
ROOT=Path(__file__).resolve().parent

def load(path,name):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
support=load(ROOT/'src/browser_support.py','owned_stage_support');adapter=load(ROOT/'adapter.py','owned_stage_adapter')
class Staging(unittest.TestCase):
 def test_actual_six_pinned_assets_and_fresh_log_in_owned_temp_c(self):
  with tempfile.TemporaryDirectory(prefix='screenplay-owned-stage-')as temp:
   c=Path(temp)/'c';(c/'Data/WebKit').mkdir(parents=True)
   rows=support.stage_candidate_in_isolated_clone(c,Path('/workspace/symbian-belle-work/candidate33'),isolation_verified=True);self.assertEqual(len(rows),6)
   reader=support.FreshPageLog(c/'Data/WebKit/log.txt','a'*32);self.assertIsNone(reader.error)
   self.assertEqual(reader.check.url_for('a'*32),'file:///C:/Data/WebKit/screenplay.html?run='+'a'*32+'&canvas2d=1&webgl=0&tap=0')
 def test_fixed_refusal_codes_only(self):
  self.assertEqual(adapter.diagnostic_refusal(support.Refusal('EMPTY_NEW_PROFILE_REQUIRED')),'EMPTY_NEW_PROFILE_REQUIRED')
  for sentinel in('PRIVATE_PATH_SENTINEL/x','SECRET_VALUE_SENTINEL'):self.assertEqual(adapter.diagnostic_refusal(support.Refusal(sentinel)),'UNCLASSIFIED_REFUSAL')
 def test_exception_text_never_appears(self):
  for e,expected in[(PermissionError('/PRIVATE_PATH_SENTINEL'),'PERMISSION_ERROR'),(TypeError('SECRET_VALUE_SENTINEL'),'TYPE_ERROR'),(AttributeError('SECRET_VALUE_SENTINEL'),'ATTRIBUTE_ERROR')]:self.assertEqual(adapter.diagnostic_exception(e),expected)
 def test_fixed_phase_only(self):
  self.assertEqual(adapter.diagnostic_phase('STAGE_CANDIDATE_AND_FIXTURE'),'STAGE_CANDIDATE_AND_FIXTURE');self.assertEqual(adapter.diagnostic_phase('SECRET_VALUE_SENTINEL'),'UNCLASSIFIED_PHASE')
if __name__=='__main__':unittest.main()
