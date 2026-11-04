#!/usr/bin/env python3
"""Synthetic helper JSON only: no process/window calls are made."""
from pathlib import Path
import importlib.util,json,tempfile,unittest
from types import SimpleNamespace
from unittest.mock import patch
ROOT=Path(__file__).resolve().parent
sp=importlib.util.spec_from_file_location('helper_report_support',ROOT/'src/browser_support.py');s=importlib.util.module_from_spec(sp);sp.loader.exec_module(s)
class Reports(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory(prefix='owned-fixed-report-');self.page=s.FreshPageLog(Path(self.temp.name)/'log.txt','a'*32)
  self.wm=ROOT/'bin/owned-wm-close';self.roi=ROOT/'bin/owned-color-rois'
  self.controller=s.BrowserController(self.page,':95','/owned/fake-host',self.wm,s.sha(self.wm),self.roi,s.sha(self.roi),{'origin_x':0,'origin_y':0,'scale_numerator':1,'scale_denominator':1,'proof_sha256':'a'*64})
 def tearDown(self):self.temp.cleanup()
 def diagnostics(self,phase='WM_DELETE_CANDIDATES'):
  d=dict.fromkeys(s.HELPER_COUNT_FIELDS,0);d.update(dict.fromkeys(s.HELPER_BOOL_FIELDS,False));d['phase']=phase;d['wm_delete_candidate_count']=2;return d
 def call(self,record,rc=2):
  with patch.object(s.subprocess,'run',return_value=SimpleNamespace(returncode=rc,stdout=json.dumps(record).encode(),stderr=b'')):
   return self.controller.helper(str(self.wm),[])
 def test_fixed_failure_phase_and_counts_retained(self):
  self.call({'wm_close_sent':False,'error_code':'OWNER_OR_WINDOW_UNPROVEN','diagnostics':self.diagnostics()})
  r=self.controller.helper_reports['wm_close'];self.assertEqual(r['status'],'WM_CLOSE_REJECTED');self.assertEqual(r['error_code'],'OWNER_OR_WINDOW_UNPROVEN');self.assertEqual(r['diagnostics']['wm_delete_candidate_count'],2)
 def test_diagnostics_stripped_for_existing_caller_contract(self):
  _,r=self.call({'wm_close_sent':True,'window_owner_verified':True,'diagnostics':self.diagnostics('COMPLETE')},0);self.assertEqual(r,{'wm_close_sent':True,'window_owner_verified':True})
 def test_unknown_private_field_not_retained(self):
  with self.assertRaises(s.Refusal):self.call({'wm_close_sent':False,'error_code':'OWNER_OR_WINDOW_UNPROVEN','diagnostics':self.diagnostics(),'private_path':'SENTINEL_PRIVATE'})
  self.assertNotIn('SENTINEL_PRIVATE',str(self.controller.helper_reports))
 def test_unknown_phase_not_retained(self):
  with self.assertRaises(s.Refusal):self.call({'wm_close_sent':False,'error_code':'OWNER_OR_WINDOW_UNPROVEN','diagnostics':self.diagnostics('SENTINEL_PRIVATE')})
  self.assertNotIn('SENTINEL_PRIVATE',str(self.controller.helper_reports))
 def test_count_bound_rejected(self):
  d=self.diagnostics();d['windows_examined']=65537
  with self.assertRaises(s.Refusal):self.call({'wm_close_sent':False,'error_code':'OWNER_OR_WINDOW_UNPROVEN','diagnostics':d})
if __name__=='__main__':unittest.main()
