from pathlib import Path
import fcntl,hashlib,json,os,re,shutil,signal,subprocess,time
out=Path('/workspace/symbian-memory-pressure-work/runtime-threshold-after-0021/run');out.mkdir()
data=Path(os.environ['XDG_DATA_HOME'])/'EKA2L1/data';locks=[]
for name in ['.jsc-run.lock','.browser-run.lock']:
 f=(data/name).open('a');fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB);locks.append(f)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
image=Path('/workspace/symbian-memory-pressure-work/native-threshold-final/build/e32/memory_pressure_probe.exe');exe=Path(os.environ['EKA2L1']);assert sha(image)=='d13a09da3554a27dd172ebae77ec9818831fd2f425abccf1412a08a6c878c689';assert sha(exe)=='046bc41192886a671c144983dd74d1e45de6860d179ae073a335a5db997c1e64'
shutil.copy2(image,data/'drives/c/sys/bin/memory_pressure_probe.exe');guest=data/'drives/c/Data/memory-pressure-threshold.log'
if guest.exists():guest.rename(out/'previous-memory-pressure-threshold.log')
result={'image_sha256':sha(image),'emulator_sha256':sha(exe),'scope':'native real low-memory threshold, recovery, coalescing and live-page preservation after patch 0021','system_threshold_modified':False,'max_committed_bytes':260046848,'status':'FAIL'}
p=None
try:
 with (out/'emulator.log').open('w') as log:
  p=subprocess.Popen([str(exe),'--device','RM-707','--run','C:\\sys\\bin\\memory_pressure_probe.exe'],cwd=str(exe.parent),stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
  result['emulator_pid']=p.pid;p.wait(timeout=20)
 if p.returncode:raise RuntimeError('nonzero host exit '+str(p.returncode))
 result['host_exit_code']=p.returncode
 text=guest.read_text(errors='replace');host=(out/'emulator.log').read_text(errors='replace');(out/'memory-pressure-threshold.log').write_text(text)
 if re.search(r'access violation|KERN-EXEC|USER-EXEC|panicked|SIGSEGV',host,re.I):raise RuntimeError('guest panic or access violation')
 if re.findall(r'^EXIT error=(-?\d+)$',text,re.M)!=['0']:raise RuntimeError('expected unique guest EXIT error=0')
 matches=re.findall(r'^PROBE_RESULT accounting=(PASS|FAIL) low=(PASS|FAIL) recovery=(PASS|FAIL) coalescing=(PASS|FAIL) live80=(PASS|FAIL) cancellation=(PASS|FAIL)$',text,re.M)
 if len(matches)!=1:raise RuntimeError('expected unique six-check PROBE_RESULT')
 labels=['accounting','low','recovery','coalescing','live80','cancellation'];result['checks']=dict(zip(labels,matches[0]));result['guest_exit_error']=0
 if any(value!='PASS' for value in matches[0]):raise RuntimeError('one or more native threshold checks failed')
 low=re.findall(r'^LOW reached=1 free=(\d+) live=(\d+) pattern=1$',text,re.M)
 recovery=re.findall(r'^RECOVERY free=(\d+) live=(\d+) recovery=1 coalesced=0x([0-9a-fA-F]+) pattern=1$',text,re.M)
 if len(low)!=1 or int(low[0][0])>=8*1024*1024 or int(low[0][1])<80*1024*1024:raise RuntimeError('missing measured low threshold with live pages')
 if len(recovery)!=1 or int(recovery[0][0])<24*1024*1024 or int(recovery[0][1])<80*1024*1024 or int(recovery[0][2],16)&0xA0!=0xA0:raise RuntimeError('missing measured recovery/coalesced events')
 if not re.search(r'^CANCEL before=(\d+) after=\1 late=0 errors=0 recommitError=0$',text,re.M):raise RuntimeError('late/error callback after cancellation')
 result['status']='PASS';result['measured_low']={'free_bytes':int(low[0][0]),'live_bytes':int(low[0][1])};result['measured_recovery']={'free_bytes':int(recovery[0][0]),'live_bytes':int(recovery[0][1])}

except BaseException as e:result['error']=str(e)
finally:
 if p and p.poll() is None:
  result['forced_cleanup']=True;os.killpg(p.pid,signal.SIGTERM)
  try:p.wait(timeout=3)
  except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait()
 if guest.exists():shutil.copy2(guest,out/'memory-pressure-threshold.log')
 result['host_exit_code']=p.returncode if p else None
 (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
raise SystemExit(result['status']=='FAIL')
