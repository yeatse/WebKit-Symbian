#!/usr/bin/env python3
"""Static guards on the actual fixture, not a substitute for browser runtime."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parent
OLD=Path('/workspace/symbian-screenplay-browser-runtime-harness-v7-work')
class Barrier(unittest.TestCase):
 def test_listener_registered_before_class_and_checks_real_target_name(self):
  s=(ROOT/'fixture/screenplay.html').read_text()
  self.assertLess(s.index("movingElement.addEventListener('animationend'"),s.index("movingElement.className = 'animate'"))
  self.assertIn("event.target !== movingElement || event.animationName !== 'slide'",s)
  self.assertEqual(s.count("check('css_motion',near(moving.left,144) && near(moving.top,168))"),1)
  self.assertIn('function near(a, b) { return Math.abs(a - b) < 0.75; }',s)
 def test_complete_waits_all_checks_and_finish_timer_only_arms(self):
  s=(ROOT/'fixture/screenplay.html').read_text()
  self.assertIn('if (finished || !finishReady) return;',s)
  self.assertIn("if (names.every(function (name) { return !!checks[name]; })) finish('complete');",s)
  self.assertIn("later(useTap ? 8500 : 2500,function () { finishReady = true; maybeFinish(); });",s)
  self.assertEqual(s.count("finish('complete')"),1)
  self.assertIn("later(9000,function () { finish('watchdog'); });",s)
 def test_public_protocol_and_roi_unchanged(self):
  import json
  self.assertEqual((ROOT/'fixture/check.py').read_bytes(),(OLD/'fixture/check.py').read_bytes())
  new=json.loads((ROOT/'fixture/manifest.json').read_text());old=json.loads((OLD/'fixture/manifest.json').read_text())
  for key in('screenshot_coordinates','named_checks','bounds_ms','query','default_scope'):
   self.assertEqual(new[key],old[key],key)
if __name__=='__main__':unittest.main()
