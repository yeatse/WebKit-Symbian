#!/usr/bin/env python3
"""Own temporary public copies only; no original XDG/private/guest data."""
from pathlib import Path
import ast,hashlib,importlib.util,os,shutil,tempfile,unittest
ROOT=Path(__file__).resolve().parent
OLD=Path('/workspace/symbian-screenplay-browser-runtime-harness-v8-work')
sp=importlib.util.spec_from_file_location('private_cleanup_test_support',ROOT/'src/browser_support.py');s=importlib.util.module_from_spec(sp);sp.loader.exec_module(s)
class Cleanup(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory(prefix='screenplay-owned-patch-cleanup-');self.private=Path(self.temp.name)/'private';self.private.mkdir(mode=0o700);self.patch=self.private/'mmf-patch-view'
  s.create_curated_patch_view(self.patch,Path('/workspace/EKA2L1-validation/build/bin/patch'))
 def tearDown(self):
  if self.patch.is_dir()and not self.patch.is_symlink():self.patch.chmod(0o700)
  self.temp.cleanup()
 def test_verified_555_directory_becomes_removable_files_stay_444(self):
  self.assertEqual(self.patch.stat().st_mode&0o777,0o555)
  modes={p.name:p.stat().st_mode&0o777 for p in self.patch.iterdir()}
  s.prepare_private_patch_cleanup(self.private,self.patch)
  self.assertEqual(self.patch.stat().st_mode&0o777,0o700)
  self.assertEqual({p.name:p.stat().st_mode&0o777 for p in self.patch.iterdir()},modes)
  shutil.rmtree(self.private);self.assertFalse(self.private.exists())
 def test_unexpected_file_rejected_before_chmod(self):
  self.patch.chmod(0o700);(self.patch/'unrelated.txt').write_bytes(b'owned synthetic unexpected');self.patch.chmod(0o555)
  with self.assertRaises(s.Refusal):s.prepare_private_patch_cleanup(self.private,self.patch)
  self.assertEqual(self.patch.stat().st_mode&0o777,0o555)
 def test_changed_public_payload_rejected_before_chmod(self):
  target=self.patch/'mediaclientaudio.dll.map';target.chmod(0o600);target.write_bytes(b'owned changed public payload');target.chmod(0o444)
  with self.assertRaises(s.Refusal):s.prepare_private_patch_cleanup(self.private,self.patch)
  self.assertEqual(self.patch.stat().st_mode&0o777,0o555)
 def test_symlink_target_rejected_before_chmod(self):
  self.patch.chmod(0o700);target=self.patch/'mediaclientaudio.dll.map';target.unlink();outside=Path(self.temp.name)/'owned-outside';outside.write_bytes(b'owned synthetic');target.symlink_to(outside);self.patch.chmod(0o555)
  with self.assertRaises(s.Refusal):s.prepare_private_patch_cleanup(self.private,self.patch)
  self.assertEqual(self.patch.stat().st_mode&0o777,0o555)
 def test_outside_directory_scope_rejected_untouched(self):
  outside=Path(self.temp.name)/'outside';outside.mkdir(mode=0o555);outside.chmod(0o555)
  with self.assertRaises(s.Refusal):s.prepare_private_patch_cleanup(self.private,outside)
  self.assertEqual(outside.stat().st_mode&0o777,0o555);outside.chmod(0o700)
 def test_only_finally_delta_is_guarded_cleanup(self):
  old=(OLD/'adapter.py').read_text();new=(ROOT/'adapter.py').read_text()
  before="    if result['status']=='BROWSER_SCREENPLAY_DISPLAY_VERIFIED':shutil.rmtree(private)"
  after="""    if result['status']=='BROWSER_SCREENPLAY_DISPLAY_VERIFIED':
     need(stopped and result['host_stopped'] is True and not result['forced_cleanup'] and
          result['drives_restored'] is True and result['configuration_restored'] is True and
          not retained and result.get('inputs_unchanged') is True,'PRIVATE_PATCH_CLEANUP_GATE')
     no_host()
     support.prepare_private_patch_cleanup(private,patch_view)
     result['private_patch_view_cleanup_verified']=True
     shutil.rmtree(private)"""
  self.assertEqual(new.count(after),1);self.assertEqual(new.replace(after,before),old)
if __name__=='__main__':unittest.main()
