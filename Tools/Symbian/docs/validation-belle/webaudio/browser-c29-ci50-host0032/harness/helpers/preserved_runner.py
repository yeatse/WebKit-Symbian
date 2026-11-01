#!/usr/bin/env python3
"""Prepare by default; root owns execution of one preserved C23 manual native-file UI run."""
import argparse,contextlib,datetime,fcntl,hashlib,importlib.util,json,os,re,resource,shutil,signal,subprocess,sys,time,uuid
from pathlib import Path
ROOT=Path(__file__).resolve().parent
FIXTURE=Path('/workspace/symbian-multi-upload-fixture-v2-work')
DATA=Path('/workspace/eka-validation/xdg/data/EKA2L1/data');BASE=DATA.parent
QT=Path('/workspace/eka-validation/xdg/config/EKA2L1')
HOST=Path('/workspace/EKA2L1-validation/build/bin/eka2l1_qt')
HOST_SHA='692abe6937ed1b651914ff6ab35a78bbd3693a705ba47853fc6f9a438ae178bd'
CANDIDATE=Path('/workspace/symbian-belle-work/candidate25')
TIMEOUT=180
FATAL=re.compile(r'access violation|\bpanicked\b|KERN-EXEC|USER-EXEC|segmentation fault|\bSIGSEGV\b|\bSIGABRT\b|Main loop exited with exception|Unable to launch process|Thread .* (?:forcefully killed|terminated peacefully) with category:.*exit code: -?[1-9][0-9]*',re.I)
INPUTS={
 'browser':(CANDIDATE/'browser.exe','b3ebbb964066eb2c3ca17a686e3a104839a387196a4d875271e84bfa06aa29f3'),
 'ui':(CANDIDATE/'browser_ui.rsc','697d56af49ce2fbde1e73ad3063ea9b7325abe0ef0a2081f66e915facd6a3b82'),
 'registration':(CANDIDATE/'browser_reg.rsc','b0a97bcbaa79c250f78611f8a0e976c44e085cdb19dbbfe83fa8fbaae78e8056'),
 'localization':(CANDIDATE/'browser_loc.rsc','30724f35615879bf281c61e1a079497b0c938822d3fc25098b180ee275c58a00'),
 'icu':(Path('/workspace/symbian-idna-work/icu/install/share/icu/icudt70l.dat'),'808fc9bd37589371288b409a8433cf3434ba792ca9d640109c5da41454895d0d'),
 'fontconfig':(Path('/workspace/WebKit-Symbian-graphics/Tools/Symbian/resources/fonts.conf'),'3dbcf8a3c9b39c0d1e15e6c6906789c0b55a4f6c02f5a63dbc05fe4792aa864c'),
 'preservation':(Path('/workspace/symbian-cookie-store-runtime-c21-work/runtime_support.py'),'0deded8b62c096774b94ebbc2bfc86a9df51ef6b95a7dee7e1e23f7d7ba46a9b'),
 'configuration':(Path('/workspace/symbian-cookie-store-runtime-c21-work/config_preservation.py'),'e6679c12d66fadb4ad6c69f46f0deede57470f379d5f64591d1e6c312b8015d0'),
 'close_window':(Path('/workspace/WebKit-Symbian-graphics/Tools/Symbian/eka2l1/close-emulator-window.py'),'d4bf431805790e88c120b58ab8d6eb4bd9cece6c5eaeb61d0fd98d0a326f9d4f'),
}
def require(value,message):
 if not value:raise RuntimeError(message)
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def identity(path):
 p=Path(path);return {'path':str(p),'bytes':p.stat().st_size,'sha256':sha(p)}
def module(path,name):
 spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
def sources():
 p=ROOT/'fixture-pins.json';require(p.is_file(),'fixture delivery not frozen yet');j=json.loads(p.read_text());return {n:(FIXTURE/n,s) for n,s in j.items()}
def checked_inputs(require_host=True):
 pins=dict(INPUTS);pins.update({'fixture_'+n:v for n,v in sources().items()})
 deployment=json.loads(Path('/workspace/symbian-belle-work/fep-secret-deployment-20261101/deployment.json').read_text())
 for index,row in enumerate(deployment['deployed_assets']):pins['native_asset_'+str(index)]=(Path(row['path']),row['sha256'])
 for row in deployment['assets']:
  p=Path('/workspace/EKA2L1-validation/build/bin/patch')/row['name'];pins['build_asset_'+row['name']]=(p,row['sha256'])
 result={}
 for name,(p,expected) in pins.items():
  require(p.is_file() and not p.is_symlink(),'pinned input missing/symlink: '+name);v=identity(p);require(v['sha256']==expected,'pinned input changed: '+name);result[name]=v
 require(HOST.is_file() and not HOST.is_symlink(),'host absent/symlink');actual=identity(HOST)
 if require_host:require(actual['sha256']==HOST_SHA,'deploy pinned host0030 before --execute')
 result['host']=actual;return result
def no_host():require(subprocess.run(['pgrep','-x','eka2l1_qt'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=3).returncode==1,'EKA host active/process check failed')
def read_log(path,limit):
 if not path.exists():return b''
 require(path.is_file() and not path.is_symlink() and path.stat().st_size<=limit,'owned log exceeds bound or unsafe');data=path.read_bytes();require(len(data)<=limit,'owned log grew beyond bound');return data
def no_fatal(*logs):require(not any(FATAL.search(b.decode('utf-8',errors='replace')) for b in logs),'fresh host/target fatal or nonzero guest exit; host0 is insufficient')
def result_record(log,token):
 found=[]
 for line in log.decode('utf-8',errors='replace').splitlines():
  if 'console:' in line and 'MULTI-UPLOAD-RESULT ' in line:
   try:j=json.loads(line.split('MULTI-UPLOAD-RESULT ',1)[1])
   except ValueError:continue
   if isinstance(j,dict) and j.get('run')==token and j.get('kind')=='result':found.append(j)
 require(len(found)<=1,'duplicate fresh browser result');return found[0] if found else None
def completion_gate(result,host,guest,validation):
 require(result.get('calibration_only')is False,'calibration-only must never PASS')
 no_fatal(host,guest)
 require(type(result.get('host_exit'))is int and result['host_exit']==0,'normal host exit0 required')
 require(result.get('wm_close_sent')is True and result.get('wm_close_after_functional')is True,'ordinary own-PID close after complete functional result required')
 require(result.get('forced_cleanup')is False and result.get('deadline_expired')is False,'deadline/failure cleanup cannot PASS')
 require(result['restoration']['guest'].get('restored')is True and result['restoration']['configuration'].get('restored')is True,'original guest/config/bindings/Qt not restored')
 require(result.get('inputs_unchanged')is True,'pinned inputs changed')
 require(validation.get('status')=='PASS','fresh FileList/multipart validator did not pass')
 return {'status':'PASS','scope':'controlled localhost native multi-file fixture; ordinary own-PID Qt close, not official app EndTask','validation':validation}
def execute(output,token,inputs,mode,calibrate_only=False):
 require(Path(os.environ.get('XDG_DATA_HOME','/invalid')).resolve()==BASE.parent,'source eka-validation/env.sh');require(Path(os.environ.get('XDG_CONFIG_HOME','/invalid')).resolve()==QT.parent,'isolated XDG_CONFIG_HOME required');require(os.environ.get('DISPLAY') and os.environ.get('PULSE_SERVER')=='unix:/workspace/symbian-media-work/runtime/native','source both runtime environments')
 output.mkdir(mode=0o700);result={'status':'FAIL','run':token,'mode':mode,'calibration_only':calibrate_only,'operator_input_required':True,'inputs':inputs,'wrapper':identity(Path(__file__)),'started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'deadline_seconds':TIMEOUT,'deadline_expired':False,'forced_cleanup':False,'wm_close_sent':False,'wm_close_after_functional':False,'restoration':{'guest':{},'configuration':{}},'wrapper_screenshots_taken':False,'private_data_archived':False,'tls_verification_disabled':False}
 support=module(INPUTS['preservation'][0],'network_preservation');config=module(INPUTS['configuration'][0],'network_configuration');sys.path.insert(0,str(FIXTURE));validator=module(FIXTURE/'validate.py','file_picker_validator')
 host=server=saved=configurations=host_stream=server_stream=None;guest_path=None;fresh=False;errors=[];locks=[];hostlog=output/'emulator.log';guestlog=output/'guest.log';old_handlers={}
 def close_owned(functional):
  if host and host.poll()is None:
   c=subprocess.run([sys.executable,str(INPUTS['close_window'][0]),str(host.pid)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=6);require(c.returncode==0,'own-PID WM close failed');result.update(wm_close_sent=True,wm_close_after_functional=functional)
 def interrupted(signum,_):raise RuntimeError('wrapper signal '+str(signum))
 for sig in [signal.SIGINT,signal.SIGTERM]:old_handlers[sig]=signal.signal(sig,interrupted)
 try:
  with contextlib.ExitStack() as stack:
   for n in ['.jsc-run.lock','.browser-run.lock']:
    p=DATA/n;require(not p.is_symlink(),'symlink lock refused');f=stack.enter_context(p.open('a'));fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB);locks.append(f)
   no_host();require(checked_inputs()==inputs,'input changed before isolation')
   saved=support.SavedFiles(DATA,token,result['restoration']['guest']);configurations=config.PrivateConfigurations(BASE/('.network-config-originals-'+token),[(BASE/'config.yml',False),(BASE/'bindings',True),(QT,True)],result['restoration']['configuration'])
   try:
    saved.reserve(support.resolve_guest(DATA,r'C:\Data\WebKit'),directory=True);fresh=True
    resources={'browser':r'C:\sys\bin\browser.exe','ui':r'C:\resource\apps\browser_ui.rsc','registration':r'C:\private\10003a3f\import\apps\browser_reg.rsc','localization':r'C:\resource\apps\browser_loc.rsc','icu':r'C:\resource\icu\icudt70l.dat','fontconfig':r'C:\resource\fontconfig\fonts.conf'}
    for name,dest in resources.items():p=saved.reserve(support.resolve_guest(DATA,dest));shutil.copy2(INPUTS[name][0],p);require(sha(p)==inputs[name]['sha256'],'resource deployment mismatch')
    configurations.reserve_all();guest_path=support.resolve_guest(DATA,r'C:\Data\WebKit\log.txt');require(not guest_path.exists(),'fresh browser log already present')
    env=os.environ.copy();env.pop('DEBUG_CURL',None);env.pop('CURL_VERBOSE',None)
    server_env=env.copy()
    server_stream=(output/'fixture.log').open('xb');server=subprocess.Popen([sys.executable,str(FIXTURE/'fixture.py'),'--serve','--run',token,'--output',str(output/'server'),'--scenario',mode,'--port','8858'],stdout=server_stream,stderr=subprocess.STDOUT,env=server_env,start_new_session=True)
    ready=output/'server/ready.json';until=time.monotonic()+10
    while not ready.exists():require(server.poll()is None and time.monotonic()<until,'fixture startup failed/timed out');time.sleep(.1)
    ready=json.loads(ready.read_text());require(ready['run']==token,'fixture token mismatch');url=ready['url'];require(url.startswith('http://localhost:8858/')and token in url,'unrecognized fixed fixture URL');result['url']=url
    controlled=json.loads((output/'server/files.json').read_text());require(controlled['run']==token,'controlled files token differs')
    guest_directory=support.resolve_guest(DATA,'C:\\Data\\WebKit\\multi-upload-'+token)
    guest_directory.mkdir(parents=True,exist_ok=False)
    for row in controlled['files']:
     name=row['name'];require(name in validator.EXPECTED and row==validator.EXPECTED[name],'unknown controlled filename/metadata')
     source=output/'server/files'/name;target=guest_directory/name;require(sha(source)==row['sha256'],'controlled source bytes changed');shutil.copyfile(source,target);require(sha(target)==row['sha256'],'controlled deployment differs')
    result['controlled_guest_directory']='C:\\Data\\WebKit\\multi-upload-'+token+'\\';result['controlled_files']=controlled['files']
    (output/'host-pid.public').write_text('starting\n')
    host_stream=hostlog.open('xb');limit=resource.getrlimit(resource.RLIMIT_CORE)
    try:
     resource.setrlimit(resource.RLIMIT_CORE,(0,limit[1]));host=subprocess.Popen([str(HOST),'--device','RM-707','--run',r'C:\sys\bin\browser.exe',' '+url],cwd=HOST.parent,env=env,stdout=host_stream,stderr=subprocess.STDOUT,start_new_session=True,pass_fds=tuple(f.fileno() for f in locks))
    finally:resource.setrlimit(resource.RLIMIT_CORE,limit)
    result['host_pid']=host.pid;(output/'host-pid.public').write_text(str(host.pid)+'\n');deadline=time.monotonic()+TIMEOUT;functional=None
    if calibrate_only:
     while host.poll()is None and time.monotonic()<deadline:
      require(server.poll()is None,'owned fixture stopped during calibration');no_fatal(read_log(hostlog,16*1024*1024),read_log(guest_path,1024*1024));time.sleep(.1)
     if host.poll()is None:close_owned(False)
     result['host_exit']=host.wait(timeout=15);result['calibration_observation_completed']=True
    else:
     while time.monotonic()<deadline:
      require(host.poll()is None,'host exited before functional completion');require(server.poll()is None,'fixture exited before completion');hl=read_log(hostlog,16*1024*1024);gl=read_log(guest_path,1024*1024);no_fatal(hl,gl);record=result_record(gl,token)
      if record:
       require(record.get('status')=='PASS' and record.get('scenario')==mode,'browser reported wrong/failed fixture result');functional=validator.validate(json.loads((output/'server/state.json').read_text()),token,mode)
       if functional.get('status')=='PASS':break
      time.sleep(.1)
     else:result['deadline_expired']=True;raise RuntimeError('180s manual UI/FileList/multipart deadline expired')
     time.sleep(2);require(host.poll()is None and server.poll()is None,'host/server exited during quiet observation');no_fatal(read_log(hostlog,16*1024*1024),read_log(guest_path,1024*1024));functional=validator.validate(json.loads((output/'server/state.json').read_text()),token,mode);require(functional.get('status')=='PASS','functional result changed during quiet observation');result['functional_validation']=functional;close_owned(True);result['host_exit']=host.wait(timeout=15)
   finally:
    for sig in old_handlers:signal.signal(sig,signal.SIG_IGN)
    if host and host.poll()is None:
     result['forced_cleanup']=True
     try:close_owned(False);host.wait(timeout=10)
     except (OSError,RuntimeError,subprocess.SubprocessError)as e:errors.append(str(e))
    if host:result['host_exit']=host.poll()
    if host_stream:host_stream.close()
    if server and server.poll()is None:
     server.terminate()
     try:server.wait(timeout=5)
     except subprocess.TimeoutExpired:errors.append('owned fixture did not stop')
    if server_stream:server_stream.close()
    stopped=not host or host.poll()is not None;result['owned_host_stopped_before_restore']=stopped
    if stopped:
     try:no_host()
     except (OSError,RuntimeError,subprocess.SubprocessError)as e:stopped=False;errors.append(str(e))
    if fresh and guest_path and guest_path.is_file():guestlog.write_bytes(read_log(guest_path,1024*1024))
    if stopped:
     for t in [configurations,saved]:
      if t:
       try:t.restore()
       except (OSError,RuntimeError)as e:errors.append(str(e))
    else:result['private_originals_retained']=True;errors.append('live host: keep private originals/journals, no restore')
    try:result['inputs_unchanged']=checked_inputs()==inputs
    except (OSError,RuntimeError)as e:result['inputs_unchanged']=False;errors.append(str(e))
   require(not errors,'; '.join(errors));result['functional_validation']=validator.validate(json.loads((output/'server/state.json').read_text()),token,mode)
   if calibrate_only:
    no_fatal(read_log(hostlog,16*1024*1024),read_log(guestlog,1024*1024));require(result.get('host_exit')==0 and result.get('calibration_observation_completed')is True and result.get('forced_cleanup')is False,'calibration did not close normally');require(result['restoration']['guest'].get('restored')is True and result['restoration']['configuration'].get('restored')is True and result.get('inputs_unchanged')is True,'calibration restore/pins failed');result['status']='CALIBRATION_ONLY'
   else:
    result['acceptance']=completion_gate(result,read_log(hostlog,16*1024*1024),read_log(guestlog,1024*1024),result.get('functional_validation',{}));result['status']='PASS'
 except (Exception,KeyboardInterrupt)as e:result['error']=str(e)or'interrupted'
 finally:
  for sig,h in old_handlers.items():signal.signal(sig,h)
  result['restoration_errors']=errors;result['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();(output/'result.json').write_text(json.dumps(result,indent=2)+'\n')
 return result
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--execute',action='store_true');p.add_argument('--output',required=True,type=Path);p.add_argument('--mode',choices=['multi','single','cancel-folder','cancel-list','navigate'],default='multi');p.add_argument('--calibrate-only',action='store_true');a=p.parse_args()
 try:
  for d in [a.output,*a.output.parents]:require(not d.is_symlink(),'output symlink ancestor refused')
  require(a.output.is_absolute()and not a.output.exists()and not a.output.is_relative_to(BASE.parent.parent),'fresh absolute output outside emulator required');inputs=checked_inputs(a.execute);token='mu-'+uuid.uuid4().hex
  if not a.execute:print(json.dumps({'status':'PREPARED_NOT_EXECUTED','future_run':token,'mode':a.mode,'host0030_deployment_needed':inputs['host']['sha256']!=HOST_SHA,'inputs':inputs,'deadline_seconds':TIMEOUT},indent=2));return 0
  result=execute(a.output,token,inputs,a.mode,a.calibrate_only);print(json.dumps({k:result.get(k)for k in ['status','run','mode','host_exit','error','forced_cleanup','restoration_errors']},indent=2));return 0 if result['status']in('PASS','CALIBRATION_ONLY')else 1
 except (OSError,ValueError,RuntimeError,subprocess.SubprocessError)as e:print('FAIL: '+str(e),file=sys.stderr);return 1
if __name__=='__main__':raise SystemExit(main())
