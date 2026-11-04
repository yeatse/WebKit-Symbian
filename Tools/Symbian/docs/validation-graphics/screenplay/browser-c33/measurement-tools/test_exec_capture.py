#!/usr/bin/env python3
"""Synthetic successful-exec capture only; no processes, display or trace reads."""
import importlib.util,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parent
sp=importlib.util.spec_from_file_location('exec_capture_synthetic',ROOT/'os_sandbox.py');m=importlib.util.module_from_spec(sp);sp.loader.exec_module(m)
class Capture(unittest.TestCase):
 def setUp(self):self.result={};self.capture=m.ExecCandidates('/owned/fixed/host',self.result)
 def test_exact_success(self):
  self.capture.feed(b'501 execve("/owned/fixed/host", ["host"], 0x123 /* 0 vars */) = 0\n');self.assertEqual(self.capture.snapshot(),(501,))
 def test_failed_exec_ignored(self):
  self.capture.feed(b'501 execve("/owned/fixed/host", ["host"], 0x123) = -1 ENOENT (No such file)\n');self.assertEqual(self.capture.snapshot(),())
 def test_other_path_and_unframed_ignored(self):
  for line in(b'501 execve("/owned/other/host", [], 0x0) = 0\n',b'execve("/owned/fixed/host", [], 0x0) = 0\n'):self.capture.feed(line)
  self.assertEqual(self.capture.snapshot(),())
 def test_paired_success(self):
  self.capture.feed(b'501 execve("/owned/fixed/host", [], 0x0 <unfinished ...>\n');self.capture.feed(b'501 <... execve resumed>) = 0\n');self.assertEqual(self.capture.snapshot(),(501,))
 def test_wrong_pid_resume_not_accepted(self):
  self.capture.feed(b'501 execve("/owned/fixed/host", [], 0x0 <unfinished ...>\n');self.capture.feed(b'502 <... execve resumed>) = 0\n');self.capture.finish();self.assertEqual(self.capture.snapshot(),());self.assertEqual(self.result['exec_candidate_parse_errors'],1)
 def test_bounded_set(self):
  for pid in range(1,18):self.capture.feed(str(pid).encode()+b' execve("/owned/fixed/host", [], 0x0) = 0\n')
  self.assertEqual(len(self.capture.snapshot()),16);self.assertEqual(self.result['exec_candidate_parse_errors'],1)
 def test_result_has_only_counts(self):
  self.test_exact_success();self.assertEqual(set(self.result),{'exec_candidate_count','exec_candidate_parse_errors'});self.assertEqual(self.result['exec_candidate_count'],1)
if __name__=='__main__':unittest.main()
