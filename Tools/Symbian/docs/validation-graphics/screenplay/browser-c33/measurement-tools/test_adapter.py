#!/usr/bin/env python3
"""Pure AST/source conservation checks; never opens display or private inputs."""
import ast
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parent
BASE=Path('/workspace/symbian-screenplay-root-private-nodbus-host37-harness-work/adapter.py')
def defs(text):return{n.name:n for n in ast.parse(text).body if isinstance(n,(ast.FunctionDef,ast.ClassDef))}
class Conservation(unittest.TestCase):
 def test_core(self):
  old,new=defs(BASE.read_text()),defs((ROOT/'adapter.py').read_text())
  for name in('need','sha','module','canonical','no_host','safe_output','case_path','LockKeeper','configure_disabled_dbus'):
   self.assertEqual(ast.dump(old[name]),ast.dump(new[name]),name)
 def test_finally_preserved(self):
  old=BASE.read_text().replace("result['native_guest_launch_attempted']","result['browser_launch_attempted']").replace("'NORMAL_API_SNAPSHOT_COMPLETE'","'BROWSER_SCREENPLAY_DISPLAY_VERIFIED'")
  derived=(ROOT/'adapter.py').read_text()
  cleanup='''    if result['status']=='BROWSER_SCREENPLAY_DISPLAY_VERIFIED':
     need(stopped and result['host_stopped'] is True and not result['forced_cleanup'] and
          result['drives_restored'] is True and result['configuration_restored'] is True and
          not retained and result.get('inputs_unchanged') is True,'PRIVATE_PATCH_CLEANUP_GATE')
     no_host()
     support.prepare_private_patch_cleanup(private,patch_view)
     result['private_patch_view_cleanup_verified']=True
     shutil.rmtree(private)'''
  derived=derived.replace(cleanup,"    if result['status']=='BROWSER_SCREENPLAY_DISPLAY_VERIFIED':shutil.rmtree(private)")
  old,new=defs(old)['execute'],defs(derived)['execute']
  a=[n for n in old.body if isinstance(n,ast.Try)][0].finalbody;b=[n for n in new.body if isinstance(n,ast.Try)][0].finalbody
  self.assertEqual(ast.dump(ast.Module(body=a,type_ignores=[])),ast.dump(ast.Module(body=b,type_ignores=[])))
 def test_controlled_config_and_outer_gates(self):
  s=(ROOT/'adapter.py').read_text();self.assertNotIn('shutil.copytree(QT',s)
  for v in("RESERVE=512*1024*1024","stretchDisplay=false","'screen-scale-ratio':100","'screen-rotation':0","'screen-upscale':-1","'screen-upscale-method':0","for name in ['resources','scripts']:","'source':str(compat_view),'destination':str(BASE/'compat')","'source':str(patch_view),'destination':str(BASE/'patch')","sandbox.get('audit_ok')is True","not result['forced_cleanup']","pixels['mismatch_count']==0","geometry['render_widget_identity_verified']is True"):
   self.assertIn(v,s)
 def test_marker_is_exact_and_additional_to_ownership(self):
  s=(ROOT/'src/owned-native-leaf.cpp').read_text()
  for v in('actual==utf8&&format==8&&left==0&&count==marker.size()','if(!client_pid(d,w,pid))continue;','bool marked_render_area=render_marker(d,w);','!client_pid(d,leaves[0].window,pid)||!render_marker(d,leaves[0].window)'):
   self.assertIn(v,s)
  self.assertNotIn('XGetImage(',s)
  self.assertIn("geometry['render_widget_identity_verified'] is True",(ROOT/'src/browser_support.py').read_text())
if __name__=='__main__':unittest.main()
