#!/usr/bin/env python3
"""Prepare one fresh C21 localhost auth case; execution requires public UI calibration."""
import argparse,datetime,fcntl,json,os,re,shutil,signal,subprocess,sys,time
from pathlib import Path
from support import ROOT,CASES,SCHEMES,sha,load_module,checked_inputs,check_calibration,require
from observer_contract import target_thread_failures,driver_contract,close_contract
from public_review import validate_public_evidence
DATA=Path('/workspace/eka-validation/xdg/data/EKA2L1/data')
def file_identity(p):
 if not p.exists():return {'exists':False}
 s=p.stat();digest=sha(p);os.utime(p,ns=(s.st_atime_ns,s.st_mtime_ns));return {'exists':True,'sha256':digest,'bytes':s.st_size,'inode':s.st_ino,'mode':s.st_mode&0o777,'mtime_ns':s.st_mtime_ns,'atime_ns':s.st_atime_ns}
def create_output(path):
 path.parent.mkdir(mode=0o700,parents=True,exist_ok=True)
 path.mkdir(mode=0o700)
def parse():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--execute',action='store_true');p.add_argument('--calibrate-only',action='store_true');p.add_argument('--calibration-seconds',type=float,default=120);p.add_argument('--scheme',choices=SCHEMES,default='basic');p.add_argument('--case',choices=CASES,default='success');p.add_argument('--run',required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--calibration',type=Path,default=ROOT/'calibration.example.json');p.add_argument('--port',type=int,default=8794);p.add_argument('--timeout',type=float,default=300);p.add_argument('--review',type=Path,help='Optional root observations; absent flags remain false/INCOMPLETE');return p.parse_args()
def main():
 a=parse();require(re.fullmatch('[A-Za-z0-9_-]{8,80}',a.run),'invalid fresh run');require(a.output.is_absolute() and not a.output.exists(),'fresh absolute output required');require(not a.output.is_relative_to(DATA.parent),'output must be outside emulator state');require(1024<=a.port<=65535 and 60<=a.timeout<=420,'invalid bounded port/timeout')
 inputs=checked_inputs();ui,start,needs=check_calibration(a.calibration,a.scheme,a.case)
 result={'status':'PREPARED_NEEDS_CALIBRATION' if needs else 'PREPARED_NOT_EXECUTED','run':a.run,'scheme':a.scheme,'case':a.case,'needs':needs,'inputs':inputs,'calibration_sha256':sha(a.calibration),'screenshots_taken':False,'credentials_opened_during_preparation':False,'calibration_only':a.calibrate_only,'tool_identities':{name:sha(ROOT/name) for name in ('run.py','drive.py','support.py','finalize.py','observer_contract.py','mask_review.py','revised_validator.py','approve-mask.py','heading_review.py','approve-heading.py','public_review.py','parent_probe.py','fixture.py')},'exit_semantics':'official EndTask helper only' if a.case.startswith('exit') else 'ordinary Qt close after functional+quiet observation'}
 if not a.execute:print(json.dumps(result,indent=2));return 0
 require(a.calibrate_only or not needs,'measured public UI calibration incomplete');require(20<=a.calibration_seconds<=180,'invalid public calibration time');require(os.environ.get('DISPLAY'),'root DISPLAY missing');require(subprocess.run(['pgrep','-x','eka2l1_qt'],stdout=subprocess.DEVNULL).returncode!=0,'emulator already running')
 create_output(a.output);private=a.output/'private';private.mkdir(mode=0o700);moves=[];locks=[];saved=None;fresh_profile=False;fixture=host=driver=None;pid=None;hostlog=None;result.update(status='FAIL',started_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),wm_close_sent=False,forced_cleanup=False)
 def reserve_file(path,label,copy_source=None):
  require(path.is_relative_to(DATA.parent),'unscoped private preservation path');ancestor=path
  while ancestor!=DATA.parent:require(not ancestor.is_symlink(),'symlink refused');ancestor=ancestor.parent
  require(not path.exists() or (path.is_file() and path.stat().st_nlink==1),'unsafe private preservation file');before=file_identity(path);backup=private/label;moves.append((path,backup,before));(private/'restore-journal.json').write_text(json.dumps([{'target':str(p),'backup':str(b),'before':v} for p,b,v in moves],indent=2)+'\n')
  if before['exists']:path.rename(backup)
  if copy_source:shutil.copy2(copy_source,path)
 def close_own_host():
  if host and host.poll() is None:
   complete=False
   if result.get('input_exit')==0 and (a.output/'interaction.json').is_file():
    try:
     interaction=json.loads((a.output/'interaction.json').read_text())
     summary=validate_public_evidence(interaction,a.output)
     complete=driver_contract(result,interaction,summary)['status']=='PENDING_STRICT_HTTP_AND_UI_VALIDATOR'
    except (OSError,ValueError,RuntimeError,TypeError,KeyError):pass
   try:
    subprocess.run([sys.executable,inputs['close_window']['path'],str(host.pid)],check=True,timeout=6,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    result['wm_close_sent']=True
    result['wm_close_record']={'pid':host.pid,'after_driver_complete':complete,'sent':True}
   except (OSError,subprocess.SubprocessError):pass
 def interrupted(signum,frame):raise KeyboardInterrupt('signal '+str(signum))
 for sig in (signal.SIGTERM,signal.SIGINT):signal.signal(sig,interrupted)
 try:
  for name in ('.jsc-run.lock','.browser-run.lock'):
   f=(DATA/name).open('a');fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB);locks.append(f)
  preservation=load_module('auth_preservation',inputs['preservation']['path']);saved=preservation.SavedFiles(DATA,a.run,result.setdefault('restoration',{}));saved.reserve(DATA/'drives/c/Data/WebKit',directory=True);fresh_profile=True
  resources={'browser':'sys/bin/browser.exe','registration':'private/10003a3f/import/apps/browser_reg.rsc','localization':'resource/apps/browser_loc.rsc','ui':'resource/apps/browser_ui.rsc','icu':'resource/icu/icudt70l.dat','fontconfig':'resource/fontconfig/fonts.conf'}
  if a.case.startswith('exit'):resources['helper']='sys/bin/browser_exit_request.exe'
  for name,dest in resources.items():target=DATA/'drives/c'/dest;saved.reserve(target);shutil.copy2(inputs[name]['path'],target)
  cfg=DATA.parent/'config.yml';reserve_file(cfg,'config.private.yml');shutil.copy2(private/'config.private.yml',cfg)
  reserve_file(DATA.parent/'bindings/webkit-editor.yml','profile.private.yml',inputs['profile']['path'])
  # C profile was renamed out in full. Its old curl marker/log/account state is
  # private and untouched. The new profile must contain no trace marker.
  require(not (DATA/'drives/c/Data/WebKit/curl-verbose').exists(),'curl tracing marker present in isolated profile')
  result['curl_trace_disabled']=True
  env=os.environ.copy();env.pop('DEBUG_CURL',None);env.pop('CURL_VERBOSE',None);env['EKA2L1_KEYBIND_PROFILE']='webkit-editor'
  with (a.output/'fixture.log').open('w') as log:
   fixture=subprocess.Popen([sys.executable,str(ROOT/'fixture.py'),'--port',str(a.port),'--run-token',a.run,'--scheme',a.scheme,'--case',a.case,'--output',str(a.output/'fixture')],env=env,stdout=log,stderr=subprocess.STDOUT)
  ready_path=a.output/'fixture/ready.json';deadline=time.monotonic()+8
  while not ready_path.exists():require(fixture.poll() is None and time.monotonic()<deadline,'fresh local fixture did not become ready');time.sleep(.1)
  ready=json.loads(ready_path.read_text());require((ready['run'],ready['scheme'],ready['case'])==(a.run,a.scheme,a.case),'fixture identity changed');result['url']=ready['url']
  command=[inputs['host']['path'],'--keybindprofile','webkit-editor','--device','RM-707','--run','C:\\sys\\bin\\browser.exe',' '+ready['url']]
  # Observer never captures a screen or invokes the standard screenshot runner.
  # Driver may capture only a cleared/public-probe FEP before private input.
  # Failed credential phases cannot trigger a screenshot.
  hostlog=(a.output/'emulator.log').open('w');host=subprocess.Popen(command,cwd=str(Path(inputs['host']['path']).parent),env=env,stdout=hostlog,stderr=subprocess.STDOUT,start_new_session=True);pid=host.pid;result['host_pid']=pid
  guest=DATA/'drives/c/Data/WebKit/log.txt';deadline=time.monotonic()+30
  while not guest.exists() or a.run not in guest.read_text(errors='replace'):require(host.poll() is None and time.monotonic()<deadline,'fresh browser log missing before input');time.sleep(.1)
  (a.output/'host-pid.public').write_text(str(pid)+'\n')
  if a.calibrate_only:
   # Root may inspect only the untouched page/empty username UI. No credential
   # file is opened by an input driver or validator in this calibration mode.
   deadline=time.monotonic()+a.calibration_seconds
   while host.poll() is None and time.monotonic()<deadline:time.sleep(.2)
   if host.poll() is None:close_own_host()
   result['host_exit_code']=host.wait(timeout=15)
  else:
   with (a.output/'input-driver.log').open('w') as log:
    cmd=[sys.executable,str(ROOT/'drive.py'),'--execute','--scheme',a.scheme,'--case',a.case,'--calibration',str(a.calibration),'--pid',str(pid),'--fixture',str(a.output/'fixture'),'--guest-log',str(guest),'--output',str(a.output/'interaction.json'),'--run-token',a.run]
    driver=subprocess.Popen(cmd,env=env,stdout=log,stderr=subprocess.STDOUT)
    try:result['input_exit']=driver.wait(timeout=a.timeout)
    except subprocess.TimeoutExpired:driver.terminate();result['input_exit']=driver.wait(timeout=5);raise RuntimeError('bounded native input deadline')
   if a.case.startswith('exit'):
    # Success may only follow the guest helper's official application shutdown.
    # A WM close used by failure cleanup is explicitly recorded and rejected.
    try:result['host_exit_code']=host.wait(timeout=20)
    except subprocess.TimeoutExpired:raise RuntimeError('official EndTask did not finish host within observer deadline')
   else:
    close_own_host();result['host_exit_code']=host.wait(timeout=15)
 except BaseException as error:result['error']=str(error)
 finally:
  for sig in (signal.SIGTERM,signal.SIGINT):signal.signal(sig,signal.SIG_IGN)
  if driver and driver.poll() is None:driver.terminate();driver.wait(timeout=5)
  if host and host.poll() is None:
   close_own_host()
   try:result['host_exit_code']=host.wait(timeout=15)
   except subprocess.TimeoutExpired:
    result['forced_cleanup']=True
    try:os.killpg(host.pid,signal.SIGTERM);host.wait(timeout=3)
    except (ProcessLookupError,subprocess.TimeoutExpired):
     try:os.killpg(host.pid,signal.SIGKILL)
     except ProcessLookupError:pass
     host.wait(timeout=3)
    result['host_exit_code']=host.returncode
  if hostlog:hostlog.close()
  if fixture and fixture.poll() is None:fixture.terminate();fixture.wait(timeout=5)
  active=subprocess.run(['pgrep','-x','eka2l1_qt'],stdout=subprocess.DEVNULL).returncode==0;result['no_guest_running_at_restore']=not active
  # Copy only this fresh redacted guest/helper log, never old profile contents.
  for name,dest in [('log.txt','guest.log'),('app-exit-request.log','app-exit-request.log')]:
   source=DATA/'drives/c/Data/WebKit'/name
   if fresh_profile and source.is_file():shutil.copy2(source,a.output/dest)
  trace=DATA/'drives/c/Data/WebKit/curl.txt'
  if fresh_profile and trace.exists():result['curl_trace_disabled']=False;result['error']='unexpected curl trace file in fresh auth profile'
  errors=[]
  if not active:
   for target,backup,before in reversed(moves):
    try:
     if before['exists'] and not backup.exists():ok=file_identity(target)==before
     else:
      target.unlink(missing_ok=True)
      if before['exists']:backup.rename(target);os.utime(target,ns=(before['atime_ns'],before['mtime_ns']))
      ok=file_identity(target)==before
     if not ok:errors.append('private file restore identity mismatch: '+str(target))
    except (OSError,RuntimeError) as error:errors.append(str(error))
   result['private_config_profile_restored']=not errors
   if saved:
    try:saved.restore()
    except (OSError,RuntimeError) as error:errors.append(str(error))
   if not errors:
    (private/'restore-journal.json').unlink(missing_ok=True)
    if not list(private.iterdir()):private.rmdir()
  else:errors.append('live guest: preserve originals and restore journal')
  if errors:result['restoration_errors']=errors
  result['tools_unchanged']=all((ROOT/name).is_file() and sha(ROOT/name)==expected for name,expected in result['tool_identities'].items())
  result['public_inputs_unchanged']=all(Path(v['path']).is_file() and sha(v['path'])==v['sha256'] for v in inputs.values())
  result['inputs_unchanged']=result['public_inputs_unchanged'] and result['tools_unchanged']
  host_path=a.output/'emulator.log'
  if host_path.is_file():
   with host_path.open(errors='replace') as stream:result['target_thread_failures']=target_thread_failures(stream)
  else:result['target_thread_failures']=[]
  # A host exit code of zero does not establish a normal target-guest exit.
  result.update(close_contract(result.get('host_exit_code'),result['forced_cleanup'],result['target_thread_failures']))
  interaction=None
  if not a.calibrate_only and (a.output/'interaction.json').is_file():
   try:interaction=json.loads((a.output/'interaction.json').read_text())
   except (ValueError,OSError):pass
  result['public_probe_validation']=validate_public_evidence(interaction,a.output) if isinstance(interaction,dict) else None
  if isinstance(interaction,dict):result['screenshots_taken']=interaction.get('screenshots_taken')
  result['driver_contract']=driver_contract(result,interaction,result['public_probe_validation'])
  if not a.calibrate_only and not a.case.startswith('exit'):
   wm=result.get('wm_close_record',{})
   owned=(isinstance(wm,dict) and set(wm)=={'pid','after_driver_complete','sent'} and wm.get('pid')==result.get('host_pid') and wm.get('after_driver_complete')is True and wm.get('sent')is True)
   result['normal_close']=result['normal_close'] and owned
  result['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();result['status']='OBSERVED_PENDING_STRICT_VALIDATOR'
  if result.get('error') or errors or result['driver_contract']['status']=='FAIL' or not result['normal_close'] or not result['inputs_unchanged']:result['status']='FAIL'
  elif a.calibrate_only:result['status']='CALIBRATION_ONLY'
  (a.output/'observer-result.json').write_text(json.dumps(result,indent=2)+'\n')
  for f in locks:f.close()
 if a.calibrate_only:
  print(json.dumps({k:result.get(k) for k in ('status','run','host_exit_code','normal_host_close','normal_close','target_guest_failed','target_thread_failures','private_config_profile_restored','error')},indent=2));return 0 if result['status']=='CALIBRATION_ONLY' else 1
 # No UI observation boolean is inferred from successful input or source code.
 finalize=load_module('auth_finalize',ROOT/'finalize.py');validation=finalize.finish(a.output,a.review,'initial')
 print(json.dumps({k:validation.get(k) for k in ('status','run','scheme','case','functional_checks','incomplete','failures')},indent=2));return {'PASS':0,'FAIL':1,'INCOMPLETE':2}[validation['status']]
if __name__=='__main__':raise SystemExit(main())
