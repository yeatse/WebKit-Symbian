#!/usr/bin/env python3
"""Prepare or execute root-reviewed C17 clipboard menu validation."""
import argparse,datetime,fcntl,hashlib,importlib.util,json,os,re,shutil,signal,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parent
REPO=Path('/workspace/WebKit-Symbian-graphics');DATA=Path('/workspace/eka-validation/xdg/data/EKA2L1/data')
BROWSER=Path('/workspace/symbian-belle-work/candidate17/browser.exe');HOST=Path('/workspace/EKA2L1-validation/build/bin/eka2l1_qt')
PINS={'browser.exe':'0d91d91ea1e016b7725097bac0768a9aa2fdf9710b765795769d2da07608a1c8','browser_ui.rsc':'251cfbede521cc5d0a3e36400cf108b12b6bb8eb7956e33a6781d8ef4bf5bc3d','browser_reg.rsc':'b0a97bcbaa79c250f78611f8a0e976c44e085cdb19dbbfe83fa8fbaae78e8056','browser_loc.rsc':'30724f35615879bf281c61e1a079497b0c938822d3fc25098b180ee275c58a00','host':'277db2b3e44c69d4b860b2f76cf194178e820556671ad32f4891fcae2cb4c2ff','fep':'aad1a4c6a4a52c70d3ac430d45dce671a8ff664071c921146bc65f754a612f6b'}
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def require(c,m):
 if not c:raise RuntimeError(m)
def identity(p):
 if not p.exists():return {'exists':False}
 s=p.stat();digest=sha(p);os.utime(p,ns=(s.st_atime_ns,s.st_mtime_ns));return {'exists':True,'sha256':digest,'bytes':s.st_size,'inode':s.st_ino,'mode':s.st_mode&0o777,'mtime_ns':s.st_mtime_ns,'atime_ns':s.st_atime_ns}
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--execute',action='store_true');p.add_argument('--run',required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--reports',type=Path,default=Path('/workspace/symbian-clipboard-browser-work/reports'));p.add_argument('--through-step',type=int,choices=range(1,10),default=9);a=p.parse_args()
 require(re.fullmatch('[A-Za-z0-9_-]{1,80}',a.run),'invalid run');require(a.output.is_absolute() and not a.output.exists(),'fresh absolute output required');require(not (a.reports/(a.run+'-menu.json')).exists(),'run report already exists')
 inputs={}
 for name in ('browser.exe','browser_ui.rsc','browser_reg.rsc','browser_loc.rsc'):
  path=BROWSER.parent/name;require(path.is_file() and sha(path)==PINS[name],name+' identity changed');inputs[name]={'path':str(path),'sha256':PINS[name]}
 for name,path in [('host',HOST),('fep',HOST.parent/'patch/avkonfep_general.dll'),('deployed_fep',DATA.parent/'patch/avkonfep_general.dll')]:
  expected=PINS['host' if name=='host' else 'fep'];require(path.is_file() and sha(path)==expected,name+' identity changed');inputs[name]={'path':str(path),'sha256':expected}
 for name,path in [('runner',REPO/'Tools/Symbian/eka2l1/run-browser.sh'),('base_driver',Path('/workspace/symbian-clipboard-browser-work/drive.py')),('review_driver',ROOT/'review-drive.py'),('menu_plan',ROOT/'menu-candidate.json'),('menu_profile',ROOT/'webkit-menu-review.yml'),('fixture',REPO/'Tools/Symbian/tests/belle-clipboard/suite.js'),('verifier',REPO/'Tools/Symbian/tests/belle-clipboard/verify.py')]:inputs[name]={'path':str(path),'sha256':sha(path)}
 result={'status':'PREPARED_NOT_EXECUTED','run':a.run,'mode':'menu','through_step':a.through_step,'inputs':inputs,'ui_review':'Every menu operation requires root approval of exact selected screenshot before Return'}
 if not a.execute:print(json.dumps(result,indent=2));return 0
 require(os.environ.get('DISPLAY'),'DISPLAY missing');require(subprocess.run(['pgrep','-x','eka2l1_qt'],stdout=subprocess.DEVNULL).returncode!=0,'guest already running')
 a.output.mkdir(mode=0o700);private=a.output/'private';private.mkdir(mode=0o700);locks=[];moves=[];absent_clipboard_parents=[];saved=None;runner=driver=None;pid=None;result['status']='FAIL';result['started_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat()
 def reserve_file(path,label,install=None):
  require(path.is_relative_to(DATA.parent),'unscoped preservation path');ancestor=path
  while ancestor!=DATA.parent:
   require(not ancestor.is_symlink(),'symlink refused');ancestor=ancestor.parent
  require(not path.exists() or path.is_file(),'non-file preservation target refused');require(not path.exists() or path.stat().st_nlink==1,'hard-linked preservation target refused');before=identity(path);backup=private/label;moves.append((path,backup,before));(private/'restore-journal.json').write_text(json.dumps([{'target':str(t),'backup':str(b),'before':v} for t,b,v in moves],indent=2)+'\n')
  if before['exists']:path.rename(backup)
  if install:shutil.copy2(install,path)
 def close_owned_guest():
  if pid and (Path('/proc')/str(pid)).exists():
   try:
    if (Path('/proc')/str(pid)/'exe').resolve()==HOST.resolve():subprocess.run([sys.executable,str(REPO/'Tools/Symbian/eka2l1/close-emulator-window.py'),str(pid)],timeout=6,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
   except (OSError,subprocess.SubprocessError):pass
 def interrupted(signum,frame):raise KeyboardInterrupt('signal '+str(signum))
 for sig in (signal.SIGTERM,signal.SIGINT):signal.signal(sig,interrupted)
 try:
  for name in ('.jsc-run.lock','.browser-run.lock'):
   f=(DATA/name).open('a');fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB);locks.append(f)
  spec=importlib.util.spec_from_file_location('menu_preservation','/workspace/symbian-p1-system-runtime-work/runtime_support.py');support=importlib.util.module_from_spec(spec);spec.loader.exec_module(support)
  saved=support.SavedFiles(DATA,a.run,result.setdefault('restoration',{}));saved.reserve(DATA/'drives/c/Data/WebKit',directory=True)
  for name in ('sys/bin/browser.exe','private/10003a3f/import/apps/browser_reg.rsc','resource/apps/browser_loc.rsc','resource/apps/browser_ui.rsc','resource/icu/icudt70l.dat','resource/fontconfig/fonts.conf'):saved.reserve(DATA/'drives/c'/name)
  cfg=DATA.parent/'config.yml';reserve_file(cfg,'config.private.yml')
  # Keep the original inode private and run against its temporary copy.
  shutil.copy2(private/'config.private.yml',cfg)
  profile=DATA.parent/'bindings/webkit-menu-review.yml';reserve_file(profile,'profile.private.yml',ROOT/'webkit-menu-review.yml')
  clipboard=DATA/'drives/d/System/Data/Clpboard.cbd';ancestor=clipboard.parent
  while not ancestor.exists():absent_clipboard_parents.append(ancestor);ancestor=ancestor.parent
  reserve_file(clipboard,'clipboard.private')
  env=os.environ.copy();env.update(EKA2L1=str(HOST),ICU_DIR='/workspace/symbian-idna-work/icu',SYMBIAN_WEBKIT_WORK=str(a.output),WAIT='500',SCREENSHOT='null:',EKA2L1_KEYBIND_PROFILE='webkit-menu-review',EKA2L1_BROWSER_LOCK_FD=str(locks[1].fileno()),EKA2L1_EXPECT_PASS=('STORED: belle-clipboard '+a.run if a.through_step==9 else 'PASS: trusted Unicode multiline copy and native paste'),EKA2L1_COMPLETION_GRACE='5')
  # Partial calibration is bounded separately and must never claim 9-case PASS.
  if a.through_step<9:env['WAIT']='180';env.pop('EKA2L1_COMPLETION_GRACE',None);env.pop('EKA2L1_EXPECT_PASS',None)
  url='http://localhost:8802/index.html?run='+a.run+'&mode=menu';result['url']=url
  with (a.output/'runner.log').open('w') as log:
   runner=subprocess.Popen(['bash',str(REPO/'Tools/Symbian/eka2l1/run-browser.sh'),str(BROWSER),url],env=env,pass_fds=tuple(f.fileno() for f in locks),stdout=log,stderr=subprocess.STDOUT)
   deadline=time.monotonic()+25
   while pid is None:
    require(runner.poll() is None,'runner exited before input window');require(time.monotonic()<deadline,'no owned host before input deadline')
    matches=subprocess.check_output(['pgrep','-x','eka2l1_qt'],text=True).split() if subprocess.run(['pgrep','-x','eka2l1_qt'],stdout=subprocess.DEVNULL).returncode==0 else []
    if len(matches)==1:
     candidate=int(matches[0]);ancestor=candidate
     while ancestor>1 and ancestor!=runner.pid:ancestor=int((Path('/proc')/str(ancestor)/'stat').read_text().split(') ',1)[1].split()[1])
     if ancestor==runner.pid and (Path('/proc')/str(candidate)/'exe').resolve()==HOST.resolve():pid=candidate
    time.sleep(.1)
   # Qt registers its window just after the host process starts; the fresh START
   # marker proves browser setup before constructing the same-PID XInput helper.
   guest=DATA/'drives/c/Data/WebKit/log.txt'
   def fresh_start():
    if not guest.exists():return False
    for line in guest.read_text(errors='replace').splitlines():
     _,marker,data=line.partition('START: belle-clipboard ')
     if marker:
      try:
       if json.loads(data)=={'run':a.run,'mode':'menu','total':9}:return True
      except ValueError:pass
    return False
   while not fresh_start():
    require(runner.poll() is None and time.monotonic()<deadline,'fresh START missing');time.sleep(.1)
   cmd=[sys.executable,str(ROOT/'review-drive.py'),'--pid',str(pid),'--guest-log',str(guest),'--run-token',a.run,'--mode','menu','--origin','http://localhost:8802','--content-rect','270','77','360','532','--menu-plan',str(ROOT/'menu-candidate.json'),'--reports',str(a.reports),'--output',str(a.output/'input'),'--through-step',str(a.through_step)]
   with (a.output/'input-driver.log').open('w') as inp:
    driver=subprocess.Popen(cmd,env=env,stdout=inp,stderr=subprocess.STDOUT);result['input_exit']=driver.wait()
   if result['input_exit'] or a.through_step<9:close_owned_guest()
   result['launcher_exit']=runner.wait(timeout=510)
  if result['input_exit']==result['launcher_exit']==0 and a.through_step==9:
   runs=list((a.output/'runs').glob('browser-*'));require(len(runs)==1,'run directory ambiguous')
   cmd=[sys.executable,str(REPO/'Tools/Symbian/tests/belle-clipboard/verify.py'),'--report',str(a.reports/(a.run+'-menu.json')),'--guest-log',str(runs[0]/'log.txt'),'--run',a.run,'--mode','menu','--origin','http://localhost:8802']
   v=subprocess.run(cmd,capture_output=True,text=True);(a.output/'validation.log').write_text(v.stdout+v.stderr);result['validator_exit']=v.returncode;require(v.returncode==0,'page/report validator failed');result['status']='PASS_PENDING_RESTORATION'
  elif a.through_step<9 and result['input_exit']==0:result['status']='CALIBRATION_PARTIAL'
 except BaseException as error:result['error']=str(error)
 finally:
  for sig in (signal.SIGTERM,signal.SIGINT):signal.signal(sig,signal.SIG_IGN)
  if driver and driver.poll() is None:driver.terminate();driver.wait(timeout=5)
  if runner and runner.poll() is None:
   close_owned_guest()
   try:result['launcher_exit']=runner.wait(timeout=18)
   except subprocess.TimeoutExpired:result['cleanup_pending']=True
  active=subprocess.run(['pgrep','-x','eka2l1_qt'],stdout=subprocess.DEVNULL).returncode==0
  result['no_guest_running_at_restore']=not active
  if not active:
   restoration=[];restore_errors=[]
   for path,backup,before in reversed(moves):
    try:
     # If setup failed before its rename, the untouched original is still valid.
     if before['exists'] and not backup.exists():
      ok=identity(path)==before
     else:
      path.unlink(missing_ok=True)
      if before['exists']:
       backup.rename(path);os.utime(path,ns=(before['atime_ns'],before['mtime_ns']))
      ok=identity(path)==before
     restoration.append({'target':str(path),'restored':ok})
     if not ok:restore_errors.append('identity mismatch: '+str(path))
    except (OSError,RuntimeError) as error:
     restoration.append({'target':str(path),'restored':False,'error':str(error)});restore_errors.append(str(error))
   for parent in absent_clipboard_parents:
    try:
     if parent.exists():parent.rmdir()
    except OSError as error:restore_errors.append(str(error))
   result['clipboard_parent_absence_restored']=all(not parent.exists() for parent in absent_clipboard_parents)
   result['private_files_restored']=all(x['restored'] for x in restoration) and result['clipboard_parent_absence_restored']
   result['private_restoration']=restoration
   if saved:
    try:saved.restore()
    except (OSError,RuntimeError) as error:restore_errors.append(str(error))
   if restore_errors:result['restoration_errors']=restore_errors;result['status']='FAIL'
   else:
    (private/'restore-journal.json').unlink(missing_ok=True)
    if not list(private.iterdir()):private.rmdir()
  result['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat()
  if result['status']=='PASS_PENDING_RESTORATION' and result.get('private_files_restored') and result.get('restoration',{}).get('restored'):result['status']='PASS'
  (a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n')
  for f in locks:f.close()
 print(json.dumps({k:result.get(k) for k in ('status','run','input_exit','launcher_exit','validator_exit','error','cleanup_pending')},indent=2))
 return 0 if result['status']=='PASS' else 2 if result['status']=='CALIBRATION_PARTIAL' else 1
if __name__=='__main__':raise SystemExit(main())
