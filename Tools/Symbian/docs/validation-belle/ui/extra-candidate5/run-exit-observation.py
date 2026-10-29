import datetime,fcntl,hashlib,json,os,re,shutil,signal,subprocess,time
from pathlib import Path
out=Path('/workspace/symbian-belle-work/ui-extra-candidate5/exit');out.mkdir()
data=Path(os.environ['EKA2L1_DATA']);c=data/'drives/c';locks=[]
for name in ['.jsc-run.lock','.browser-run.lock']:
 f=(data/name).open('a');fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB);locks.append(f)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
expected='139e140d0802e9de9b202e6bb3d292ad7507415d68fbe86979f5af5f6756a7ff'
for drive in ['c','e']:assert sha(data/f'drives/{drive}/sys/bin/browser.exe')==expected
helper=Path('/workspace/symbian-ui-work/app-exit-build/e32/browser_exit_request.exe')
assert sha(helper)=='e668e662f15b563b71e5237b910db422f86fc265de7f6a88925b310776e7489d'
shutil.copy2(helper,c/'sys/bin/browser_exit_request.exe')
for name in ['log.txt','log.old.txt','app-exit-request.log']:
 p=c/'Data/WebKit'/name
 if p.exists():p.rename(out/('previous-'+name))
url='http://localhost:8782/diagnostics.html?case=exit&run=extra5-exit-1929'
command=[os.environ['EKA2L1'],'--device','RM-707','--run','C:\\sys\\bin\\browser.exe',' '+url]
result={'kind':'expected guest application exit while picker open','url':url,'browser_sha256':expected,'emulator_sha256':sha(Path(command[0])),'helper_sha256':sha(helper),'wm_close_sent':False,'forced_cleanup':False,'status':'RUNNING','started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()}
error=None
with (out/'emulator.log').open('w') as log:
 p=subprocess.Popen(command,cwd=str(Path(command[0]).parent),stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
 result['emulator_pid']=p.pid;(out/'launch.json').write_text(json.dumps(result,indent=2)+'\n');print(p.pid,flush=True)
 try:
  deadline=time.monotonic()+180
  while p.poll() is None and time.monotonic()<deadline:
   text=(out/'emulator.log').read_text(errors='replace')
   if re.search(r'access violation|panicked|KERN-EXEC|USER-EXEC',text,re.I):raise RuntimeError('guest panic/AV')
   time.sleep(.2)
  if p.poll() is None:raise RuntimeError('application failed to exit within observation deadline')
  if p.returncode:raise RuntimeError('host exited '+str(p.returncode))
 except BaseException as exc:error=str(exc)
 finally:
  if p.poll() is None:
   result['forced_cleanup']=True
   os.killpg(p.pid,signal.SIGTERM)
   try:p.wait(timeout=3)
   except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait()
  result['host_exit_code']=p.returncode
  for name in ['log.txt','app-exit-request.log']:
   src=c/'Data/WebKit'/name
   if src.exists():shutil.copy2(src,out/name)
result['status']='FAIL' if error else 'OBSERVATION_COMPLETE_PENDING_VALIDATION';result['error']=error;result['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat()
(out/'observation.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2));raise SystemExit(bool(error))
