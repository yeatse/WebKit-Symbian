#!/usr/bin/env python3
"""Prepare only by default; --execute runs the unchanged seven touch cases."""
from pathlib import Path
import argparse,fcntl,hashlib,importlib.util,json,os,runpy,shutil,signal,subprocess,sys,uuid
BASE=Path(__file__).resolve().parent
DATA=Path('/workspace/eka-validation/xdg/data/EKA2L1/data')
SUITE_OUT=Path('/workspace/symbian-belle-work/p1-validation/touch-sequences-c17-regression-20261101')
DRIVER=BASE/'run-touch-c17.py'
SUPPORT=Path('/workspace/symbian-p1-system-runtime-work/runtime_support.py')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def need(ok,message):
 if not ok:raise RuntimeError(message)
def write(path,value):
 tmp=path.with_suffix(path.suffix+'.tmp');tmp.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n');tmp.replace(path)
def check_inputs():
 pins=json.loads((BASE/'pinned-inputs.json').read_text())['files']
 for entry in pins:
  path=Path(entry['path']);need(path.is_file() and sha(path)==entry['sha256'],'immutable input mismatch: '+str(path))
 return pins
def identity(path):
 if not path.exists():return {'exists':False}
 s=path.stat();v={'exists':True,'inode':s.st_ino,'device':s.st_dev,'mode':s.st_mode&0o777,'mtime_ns':s.st_mtime_ns}
 if path.is_file():v['sha256']=sha(path)
 return v
def execute(args,pins):
 need(args.output.is_absolute() and not args.output.exists(),'use a fresh absolute preservation output')
 need(not args.output.resolve().is_relative_to(DATA),'preservation output must be outside guest DATA')
 need(not SUITE_OUT.exists(),'retain existing suite output; prepare a new identity-only clone for a retry')
 need(Path(os.environ.get('XDG_DATA_HOME',''))/'EKA2L1/data'==DATA,'source recorded EKA environment')
 need(os.environ.get('DISPLAY'),'DISPLAY must already be running')
 locks=[];saved=None;settings=[];child_code=None;primary=None
 report={'status':'FAIL','inputs':pins,'suite_output':str(SUITE_OUT),'clipboard_backup':False,'restoration':{}}
 real_popen=subprocess.Popen
 args.output.mkdir(mode=0o700,parents=True)
 try:
  need(subprocess.run(['pgrep','-x','eka2l1_qt'],stdout=subprocess.DEVNULL).returncode!=0,'root must own an idle runtime')
  for name in ('.jsc-run.lock','.browser-run.lock'):
   f=(DATA/name).open('a');fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB);locks.append(f)
  spec=importlib.util.spec_from_file_location('touch_preservation_support',SUPPORT)
  module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
  saved=module.SavedFiles(DATA,'touch-c17-'+uuid.uuid4().hex,report['restoration'])
  saved.reserve(DATA/'drives/c/Data/WebKit',directory=True)
  for name in ('sys/bin/browser.exe','private/10003a3f/import/apps/browser_reg.rsc','resource/apps/browser_loc.rsc','resource/apps/browser_ui.rsc','resource/icu/icudt70l.dat','resource/fontconfig/fonts.conf'):
   saved.reserve(DATA/'drives/c'/name)
  private=args.output/'settings.private';private.mkdir(mode=0o700)
  for name in ('config.yml','bindings'):
   source=DATA.parent/name;need(not source.is_symlink(),'unexpected settings symlink')
   if source.is_dir():need(not any(p.is_symlink() for p in source.rglob('*')),'symlinked keybind settings')
   before=identity(source);backup=private/name
   entry={'source':str(source),'backup':str(backup),'before':before,'restored':False};settings.append(entry)
   write(args.output/'settings-restoration.json',settings)
   if before['exists']:
    source.rename(backup)
    if backup.is_dir():shutil.copytree(backup,source)
    else:shutil.copy2(backup,source)
  # FD plumbing only. The clone's assertions, commands and observations stay
  # unchanged; its subprocesses must inherit the two outer lock descriptions.
  inherited=tuple(f.fileno() for f in locks)
  def popen_with_locks(*positional,**kw):
   kw['pass_fds']=tuple(dict.fromkeys(tuple(kw.get('pass_fds',()))+inherited))
   return real_popen(*positional,**kw)
  subprocess.Popen=popen_with_locks
  old_fd=os.environ.get('EKA2L1_BROWSER_LOCK_FD')
  os.environ['EKA2L1_BROWSER_LOCK_FD']=str(locks[1].fileno())
  try:
   runpy.run_path(str(DRIVER),run_name='__main__');child_code=0
  except SystemExit as error:child_code=error.code if isinstance(error.code,int) else 1
  finally:
   if old_fd is None:os.environ.pop('EKA2L1_BROWSER_LOCK_FD',None)
   else:os.environ['EKA2L1_BROWSER_LOCK_FD']=old_fd
 except BaseException as error:
  primary=error;report['error']=type(error).__name__+': '+str(error)
 finally:
  subprocess.Popen=real_popen
  for sig in (signal.SIGINT,signal.SIGTERM):signal.signal(sig,signal.SIG_IGN)
  active=subprocess.run(['pgrep','-x','eka2l1_qt'],stdout=subprocess.DEVNULL).returncode==0
  report['no_guest_running_at_restore']=not active;errors=[]
  if not active:
   for entry in reversed(settings):
    source=Path(entry['source']);backup=Path(entry['backup'])
    try:
     if backup.exists() or not entry['before']['exists']:
      if source.exists():
       if source.is_dir():shutil.rmtree(source)
       else:source.unlink()
      if backup.exists():backup.rename(source)
     entry['after']=identity(source);entry['restored']=entry['after']==entry['before']
     need(entry['restored'],'settings restore mismatch: '+str(source))
    except Exception as error:errors.append(str(error))
   if saved:
    try:saved.restore()
    except Exception as error:errors.append(str(error))
  else:errors.append('guest still active; private originals left in restoration journals')
  report.update(child_exit=child_code,settings=settings,restoration_errors=errors)
  try:check_inputs()
  except Exception as error:errors.append(str(error))
  if SUITE_OUT.is_dir() and (SUITE_OUT/'result.json').is_file():
   suite=json.loads((SUITE_OUT/'result.json').read_text());report['suite_status']=suite.get('status');rows=suite.get('cases',[])
   report['seven_cases_passed']=len(rows)==7 and all(row.get('status')=='PASS' and row.get('input_exit')==row.get('launcher_exit')==0 for row in rows)
  else:report['seven_cases_passed']=False
  if child_code==0 and report['seven_cases_passed'] and saved and report['restoration'].get('restored') and all(e['restored'] for e in settings) and not errors and not active and primary is None:report['status']='PASS'
  if (args.output/'settings.private').exists() and not any((args.output/'settings.private').iterdir()):(args.output/'settings.private').rmdir()
  write(args.output/'result.json',report);write(args.output/'settings-restoration.json',settings)
  for f in locks:f.close()
 print(json.dumps({'status':report['status'],'child_exit':child_code,'suite_output':str(SUITE_OUT),'preservation_result':str(args.output/'result.json'),'restoration_errors':report['restoration_errors']},indent=2))
 return 0 if report['status']=='PASS' else 1
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--execute',action='store_true');p.add_argument('--output',type=Path,default=Path('/workspace/symbian-belle-work/p1-validation/touch-c17-preservation-20261101'));a=p.parse_args()
 pins=check_inputs()
 if not a.execute:
  print(json.dumps({'status':'PREPARED_NOT_EXECUTED','suite_output':str(SUITE_OUT),'preservation_output':str(a.output),'inputs':pins},indent=2));return 0
 def interrupted(signum,_frame):
  for sig in (signal.SIGTERM,signal.SIGINT):signal.signal(sig,signal.SIG_IGN)
  raise RuntimeError('touch wrapper cancelled by signal '+str(signum))
 for sig in (signal.SIGTERM,signal.SIGINT):signal.signal(sig,interrupted)
 return execute(a,pins)
if __name__=='__main__':raise SystemExit(main())
