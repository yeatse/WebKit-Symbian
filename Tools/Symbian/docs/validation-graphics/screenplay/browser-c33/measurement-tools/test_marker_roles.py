#!/usr/bin/env python3
"""Exact geometry-role delta against actual v6 code; no X/display calls."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parent
OLD=Path('/workspace/symbian-screenplay-browser-runtime-harness-v6-work')
class RoleDelta(unittest.TestCase):
 def test_only_remove_leaf_requirement_and_count_marked_nodes(self):
  s=(ROOT/'src/owned-native-leaf.cpp').read_text()
  changed='''if(!client_pid(d,w,pid))continue;
            bool marked_render_area=render_marker(d,w);
            if(marked_render_area){increment(diagnostics.marker_all_nodes);if(count)increment(diagnostics.marker_nonleaf_nodes);}
            // The pinned native render widget may itself own native children.
            if(w==main||!marked_render_area)continue;'''
  self.assertEqual(s.count(changed),1)
  self.assertEqual(s.replace(changed,'if(w==main||count||!client_pid(d,w,pid)||!render_marker(d,w))continue;'),(OLD/'src/owned-native-leaf.cpp').read_text())
 def test_python_only_accepts_two_additional_fixed_counters(self):
  s=(ROOT/'src/browser_support.py').read_text()
  start=s.index('PRIVATE_MMF_COPY_NAMES = ');end=s.index('class FreshPageLog:',start);s=s[:start]+s[end:]
  s=s.replace('78a6f3f4df3cdc37b6b634d104bab3d783b4c56749b0c340b96142b6b91b882f','caa21e68570910720a994b120c7e499124a9f7a5786f59200a2b54bb9e6efabc')
  s=s.replace("'render_marker_count','marker_all_nodes','marker_nonleaf_nodes'))","'render_marker_count'))")
  self.assertEqual(s,(OLD/'src/browser_support.py').read_text())
if __name__=='__main__':unittest.main()
