#!/usr/bin/env python3
"""Only pinned public candidate assets and a new synthetic temporary C tree."""
import importlib.util
import json
from pathlib import Path
import tempfile
ROOT=Path(__file__).resolve().parent
sp=importlib.util.spec_from_file_location('owned_alias_actual_support',ROOT/'src/browser_support.py');m=importlib.util.module_from_spec(sp);sp.loader.exec_module(m)
with tempfile.TemporaryDirectory(prefix='screenplay-owned-alias-stage-')as temporary:
 c=Path(temporary)/'c';profile=c/'Data/WebKit';profile.mkdir(parents=True)
 other=c/'data/WebKit';other.mkdir(parents=True);sentinel=other/'unrelated.txt';sentinel.write_bytes(b'owned synthetic private branch sentinel');sentinel.chmod(0o440);before=sentinel.stat()
 for folder in('resource','Resource'):
  for child in('apps','APPS'):(c/folder/child).mkdir(parents=True)
 for name in('browser_ui.rsc','BROWSER_UI.RSC'):
  p=c/'resource/apps'/name;p.write_bytes(b'owned old public asset');p.chmod(0o640)
 unchanged=c/'Resource/APPS/unrelated.txt';unchanged.write_bytes(b'unrelated owned public tree sentinel');unchanged.chmod(0o440);before2=unchanged.stat()
 phases=[];rows=m.stage_candidate_in_isolated_clone(c,Path('/workspace/symbian-belle-work/candidate33'),isolation_verified=True,fresh_profile=profile,diagnostic=lambda phase,asset:phases.append((phase,asset)))
 counts={r['name']:r['target_copy_count']for r in rows}
 assert len(rows)==6 and counts['browser_ui.rsc']==5 and counts['browser_loc.rsc']==4 and counts['icudt70l.dat']==2
 assert (c/'resource/apps/BROWSER_UI.RSC').stat().st_mode&0o777==0o640
 for name,public_before,expected in[(sentinel,before,b'owned synthetic private branch sentinel'),(unchanged,before2,b'unrelated owned public tree sentinel')]:
  after=name.stat();assert name.read_bytes()==expected;assert (after.st_ino,after.st_mtime_ns,after.st_mode)==(public_before.st_ino,public_before.st_mtime_ns,public_before.st_mode)
 assert (profile/'screenplay.html').is_file()and not(other/'screenplay.html').exists()
 assert all(asset is None or asset in m.PUBLIC_ASSET_DESTINATIONS for phase,asset in phases)
 print(json.dumps({'actual_public_alias_staging':'PASS','asset_copy_counts':counts,'unrelated_bytes_modes_inode_mtime_preserved':True,'explicit_fresh_profile_used':True}))
