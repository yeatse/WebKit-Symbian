from pathlib import Path
import fcntl,hashlib,json,os,re,shutil,signal,subprocess,time
out=Path('/workspace/symbian-memory-pressure-work/runtime-accounting-after-0021/run');out.mkdir()
data=Path(os.environ['XDG_DATA_HOME'])/'EKA2L1/data';locks=[]
for name in ['.jsc-run.lock','.browser-run.lock']:
 f=(data/name).open('a');fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB);locks.append(f)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
image=Path('/workspace/symbian-memory-pressure-work/native-build/e32/memory_pressure_probe.exe');exe=Path(os.environ['EKA2L1']);assert sha(image)=='13eccaba4a7ba1cf242a443b778d06e372194b91b2b34ad2ad40072faf09db62';assert sha(exe)=='046bc41192886a671c144983dd74d1e45de6860d179ae073a335a5db997c1e64'
shutil.copy2(image,data/'drives/c/sys/bin/memory_pressure_probe.exe');guest=data/'drives/c/Data/memory-pressure-probe.log'
if guest.exists():guest.rename(out/'previous-memory-pressure-probe.log')
result={'image_sha256':sha(image),'emulator_sha256':sha(exe),'scope':'native monitor lifecycle and bounded committed-RAM accounting after patch 0021','system_threshold_modified':False,'max_committed_bytes':67108864,'status':'FAIL'}
p=None
try:
 with (out/'emulator.log').open('w') as log:
  p=subprocess.Popen([str(exe),'--device','RM-707','--run','C:\\sys\\bin\\memory_pressure_probe.exe'],cwd=str(exe.parent),stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
  result['emulator_pid']=p.pid;p.wait(timeout=20)
 if p.returncode:raise RuntimeError('nonzero host exit '+str(p.returncode))
 result['host_exit_code']=p.returncode
 text=guest.read_text(errors='replace');host=(out/'emulator.log').read_text(errors='replace');(out/'memory-pressure-probe.log').write_text(text)
 if re.search(r'access violation|KERN-EXEC|USER-EXEC|panicked|SIGSEGV',host,re.I):raise RuntimeError('guest panic or access violation')
 if re.findall(r'^EXIT error=(-?\d+)$',text,re.M)!=['0']:raise RuntimeError('expected unique guest EXIT error=0')
 matches=re.findall(r'^PROBE_RESULT bridge=(PASS|FAIL) pattern=(PASS|FAIL) accounting=(PASS|UNAVAILABLE) memoryNotifications=(\d+)$',text,re.M)
 if len(matches)!=1:raise RuntimeError('expected unique PROBE_RESULT')
 bridge,pattern,accounting,count=matches[0];result.update(bridge=bridge,pattern=pattern,accounting=accounting,memory_notifications=int(count),guest_exit_error=0)
 if bridge!='PASS' or pattern!='PASS':raise RuntimeError('native lifecycle or committed pattern failed')
 if not re.search(r'^CANCEL completed=(\d+) after=\1 late=0 errors=0$',text,re.M):raise RuntimeError('late/error callback after cancellation')
 if accounting!='PASS':raise RuntimeError('patched RAM accounting must be available')
 result['status']='PASS'
 result['note']='No notification is required without a measured threshold crossing; unavailable accounting is not an overall implementation PASS.'
except BaseException as e:result['error']=str(e)
finally:
 if p and p.poll() is None:
  result['forced_cleanup']=True;os.killpg(p.pid,signal.SIGTERM)
  try:p.wait(timeout=3)
  except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait()
 if guest.exists():shutil.copy2(guest,out/'memory-pressure-probe.log')
 result['host_exit_code']=p.returncode if p else None
 (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
raise SystemExit(result['status']=='FAIL')
