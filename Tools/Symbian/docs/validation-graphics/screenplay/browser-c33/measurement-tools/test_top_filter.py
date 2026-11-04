#!/usr/bin/env python3
"""Exact delta checks for all actual helper sources; no X/display calls."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parent
OLD=Path('/workspace/symbian-screenplay-browser-runtime-harness-v5-work')
class Delta(unittest.TestCase):
 def test_only_requested_filter_changes(self):
  added='''if(!tree_ok||!client_pid(d,w,pid)||!declared_pid(d,w,pid,inner))continue;
            // Qt native children also expose WM_DELETE. Only visible owned
            // top clients, whose parent is not this owned client, qualify.
            diagnostics.phase="WINDOW_TREE";
            XWindowAttributes top_info{};
            if(!XGetWindowAttributes(d,w,&top_info)||top_info.c_class!=InputOutput||top_info.map_state!=IsViewable)continue;
            if(parent!=None&&client_pid(d,parent,pid))continue;'''
  for name in('owned-wm-close.cpp','owned-color-rois.cpp'):
   with self.subTest(helper=name):
    s=(ROOT/'src'/name).read_text();old=(OLD/'src'/name).read_text()
    self.assertEqual(s.count(added),1)
    s=s.replace(added,'if(!client_pid(d,w,pid)||!declared_pid(d,w,pid,inner))continue;')
    s=s.replace('bool tree_ok=XQueryTree(d,w,&rootWindow,&parent,&children,&count);\n            if(tree_ok){','if(XQueryTree(d,w,&rootWindow,&parent,&children,&count)){')
    self.assertEqual(s,old)
if __name__=='__main__':unittest.main()
