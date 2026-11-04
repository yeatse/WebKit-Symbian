#!/usr/bin/env python3
"""Synthetic page failure export plus one mocked normal close; no display/guest."""
import importlib.util
from pathlib import Path
import unittest,ast
ROOT=Path(__file__).resolve().parent
sp=importlib.util.spec_from_file_location('page_failure_test_base',ROOT/'test_host.py');base=importlib.util.module_from_spec(sp);sp.loader.exec_module(base)
class PageFailure(base.NormalClose):
 def test_valid_failed_terminal_is_exportable_and_still_closes_once(self):
  rows=base.examples.example()
  for row in rows:
   if row['event']=='check'and row['name']=='canvas2d':row['outcome']='fail'
  rows[-1].update(result='PAGE_REQUESTS_FAIL',failed=1,passed=rows[-1]['passed']-1)
  self.log.write_text(base.examples.wrapped(rows))
  self.controller.helper=self.helper;self.drive()
  report=self.controller.finish(self.closed_sandbox)
  self.assertEqual([c[0]for c in self.calls],[str(self.wm)])
  self.assertTrue(report['normal_close_sent']);self.assertEqual(report['page_status'],'PAGE_REQUESTS_FAIL')
  self.assertEqual(report['public_page_report']['checks']['canvas2d'],'fail')
  self.assertEqual(self.page.check.validate(self.page.public_text(),base.examples.RUN)['page_status'],'PAGE_REQUESTS_FAIL')
 def test_export_precedes_outer_gates(self):
  text=(ROOT/'adapter.py').read_text()
  exported=text.index("result['public_page_report']=page_log.report")
  gate=text.index("need(sandbox.get('exit_code')==0")
  self.assertLess(exported,gate)
if __name__=='__main__':unittest.main()
