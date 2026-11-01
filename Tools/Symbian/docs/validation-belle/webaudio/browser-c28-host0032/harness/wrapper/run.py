#!/usr/bin/env python3
"""Prepare by default. Root owns trusted input and independent PCM verification."""
import argparse,contextlib,datetime,fcntl,hashlib,importlib.util,json,os,re,resource,shutil,signal,subprocess,sys,time,uuid
from pathlib import Path
ROOT=Path(__file__).resolve().parent
FIXTURE=Path('/workspace/symbian-webaudio-classic-fixture-work')
FIXTURE_PINS_SHA='9b60cc00760a5a1590b741741fc6edb473feef069cac533243c9bebc83362db7'
DATA=Path('/workspace/eka-validation/xdg/data/EKA2L1/data');BASE=DATA.parent
QT=Path('/workspace/eka-validation/xdg/config/EKA2L1')
HOST=Path('/workspace/EKA2L1-validation/build/bin/eka2l1_qt')
HOST_SHA='692abe6937ed1b651914ff6ab35a78bbd3693a705ba47853fc6f9a438ae178bd'
TIMEOUT=180
FATAL=re.compile(r'access violation|\bpanicked\b|KERN-EXEC|USER-EXEC|segmentation fault|\bSIGSEGV\b|\bSIGABRT\b|Main loop exited with exception|Unable to launch process|Thread .* (?:forcefully killed|terminated peacefully) with category:.*exit code: -?[1-9][0-9]*',re.I)
STATIC={
 'icu':(Path('/workspace/symbian-idna-work/icu/install/share/icu/icudt70l.dat'),'808fc9bd37589371288b409a8433cf3434ba792ca9d640109c5da41454895d0d'),
 'fontconfig':(Path('/workspace/WebKit-Symbian-graphics/Tools/Symbian/resources/fonts.conf'),'3dbcf8a3c9b39c0d1e15e6c6906789c0b55a4f6c02f5a63dbc05fe4792aa864c'),
 'preservation':(Path('/workspace/symbian-cookie-store-runtime-c21-work/runtime_support.py'),'0deded8b62c096774b94ebbc2bfc86a9df51ef6b95a7dee7e1e23f7d7ba46a9b'),
 'configuration':(Path('/workspace/symbian-cookie-store-runtime-c21-work/config_preservation.py'),'e6679c12d66fadb4ad6c69f46f0deede57470f379d5f64591d1e6c312b8015d0'),
 'close_window':(Path('/workspace/WebKit-Symbian-graphics/Tools/Symbian/eka2l1/close-emulator-window.py'),'d4bf431805790e88c120b58ab8d6eb4bd9cece6c5eaeb61d0fd98d0a326f9d4f'),
 'preserved_runner':(Path('/workspace/symbian-multi-upload-work/runtime-c25-v2-host0032/run.py'),'1b2177f47770b075b3d140c49ff2288d94353e8387e2befccdf3140f07a2c2a7'),
}
RESOURCES={
 'ui':('browser_ui.rsc','697d56af49ce2fbde1e73ad3063ea9b7325abe0ef0a2081f66e915facd6a3b82'),
 'registration':('browser_reg.rsc','b0a97bcbaa79c250f78611f8a0e976c44e085cdb19dbbfe83fa8fbaae78e8056'),
 'localization':('browser_loc.rsc','30724f35615879bf281c61e1a079497b0c938822d3fc25098b180ee275c58a00'),
}
# Seven named assets in exactly two patch directories: original ten copies plus four ostream copies.
# Only the genuine ordinal-override .dll.map is named here; never a GNU .elf.map.
NATIVE={
 'mediaclientaudio_general.dll':(72620,'a07674f2957410a00f0bcdbc997590feca50eb028147a1497934713d8aa65d49'),
 'mediaclientvideo_v100.dll':(52556,'03fd46f1ce54a2b5863fe841e87bdc17b99fbd05ef99459f5a56b7dbd75209c3'),
 'mediaclientaudio.dll.map':(48796,'5fadc737e5182ddd24230885af23717cd7537daf7427501ff09e43a9e40df5d3'),
 'mediaclientvideo.dll.map':(11393,'1d0f3e76bcbc25d65207b0cb5674bcbbc9c19e58bdf0959b2d3ac0f7f9824282'),
 'avkonfep_general.dll':(99488,'fd6092f2ad61d39ccb47475229c367f5494fe68a8bd2bd8d0e04dc1bafcdc7ae'),
 'mediaclientaudiostream_general.dll':(40384,'8559122f1caeea69e9612ee68f641f9ca90501a12183c5fbd3b77368cdbede04'),
 'mediaclientaudiostream.dll.map':(5027,'e92b9ede1b1a11355120e7a661be025f795e86841deec5c41b78fe7b691bc4ce'),
}
PATCH_ROOTS=[BASE/'patch',HOST.parent/'patch']
OSTREAM_SOURCE=Path('/workspace/symbian-ostream-pause-native-build-20261101/artifacts')
CANDIDATE_ROOT=Path('/workspace/symbian-belle-work/candidate28')
C25_SHA='b3ebbb964066eb2c3ca17a686e3a104839a387196a4d875271e84bfa06aa29f3'
BUILD_INPUTS={
 'cache':Path('/workspace/symbian-graphics-work/build/webcore/CMakeCache.txt'),
 'config':Path('/workspace/symbian-graphics-work/build/webcore/cmakeconfig.h'),
 'settings_h':Path('/workspace/symbian-graphics-work/build/webcore/WebCore/DerivedSources/Settings.h'),
 'settings_cpp':Path('/workspace/symbian-graphics-work/build/webcore/WebCore/DerivedSources/Settings.cpp'),
}
REQUIRED_SOURCES={
 'Source/WebCore/platform/audio/symbian/'+name for name in ['AudioDestinationSymbian.cpp','AudioDestinationSymbianNative.cpp','AudioFileReaderSymbian.cpp','FFTFrameSymbian.cpp']
}|{
 'Source/WebCore/Modules/webaudio/'+name for name in ['AudioContext.cpp','AudioNodeOutput.cpp','MediaElementAudioSourceNode.cpp','BaseAudioContext.idl','AudioWorklet.idl','AudioWorkletNode.idl']
}|{'Source/WebCore/page/Settings.yaml','Source/WebKitLegacy/symbian/WebViewImpl.cpp'}
def require(value,message):
 if not value:raise RuntimeError(message)
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def identity(path):
 p=Path(path);return{'path':str(p),'bytes':p.stat().st_size,'sha256':sha(p)}
def safe_file(path):
 p=Path(path);require(p.is_absolute()and p.is_file()and not p.is_symlink()and p==p.resolve(),'regular canonical absolute input required: '+str(p))
 for ancestor in p.parents:require(not ancestor.is_symlink(),'input symlink ancestor refused: '+str(p))
 return p
def module(path,name):
 spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
def sources():
 path=safe_file(FIXTURE/'fixture-pins.json');require(sha(path)==FIXTURE_PINS_SHA,'fixture pin list changed')
 return{'fixture_pins':(path,FIXTURE_PINS_SHA),**{'fixture_'+name:(FIXTURE/name,expected)for name,expected in json.loads(path.read_text()).items()}}
def candidate_pins(args):
 require(bool(args.browser)==bool(args.browser_sha256),'provide --browser and --browser-sha256 together')
 if not args.browser:return{}
 require(re.fullmatch('[0-9a-f]{64}',args.browser_sha256)is not None,'browser SHA-256 must be 64 lowercase hex digits')
 browser=safe_file(args.browser);require(not browser.is_relative_to(BASE.parent),'browser must be an external reviewed candidate')
 require(args.browser_sha256!=C25_SHA and not browser.is_relative_to(Path('/workspace/symbian-belle-work/candidate25')),'C25 browser cannot stand in for the C28 candidate')
 directory=args.resource_dir or browser.parent;require(directory.is_absolute(),'absolute resource directory required')
 return{'browser':(browser,args.browser_sha256),**{name:(directory/filename,expected)for name,(filename,expected)in RESOURCES.items()}}
def candidate_metadata_pins(args):
 require(bool(args.candidate_metadata)==bool(args.candidate_metadata_sha256),'provide --candidate-metadata and its --candidate-metadata-sha256 together')
 if not args.candidate_metadata:return{}
 require(args.browser is not None,'candidate metadata requires the explicit C28 browser pin')
 require(re.fullmatch('[0-9a-f]{64}',args.candidate_metadata_sha256)is not None,'metadata SHA-256 must be 64 lowercase hex digits')
 path=safe_file(args.candidate_metadata);require(sha(path)==args.candidate_metadata_sha256,'candidate metadata SHA-256 differs');value=json.loads(path.read_text())
 require(set(value)=={'schema','candidate_id','browser','enable_web_audio','runtime_audio_worklet_enabled','build_inputs','build_identities','source_identities','source_files'},'candidate metadata fields differ')
 require(value['schema']=='classic-webaudio-c28-v1'and value['candidate_id']=='C28'and value['enable_web_audio']is True and value['runtime_audio_worklet_enabled']is False,'C28/classic WebAudio runtime metadata differs')
 require(value['browser']=={'path':str(args.browser),'sha256':args.browser_sha256},'metadata does not identify the actual supplied C28 image')
 pins={'candidate_metadata':(path,args.candidate_metadata_sha256)}
 rows=value['build_inputs'];require(type(rows)is list and len(rows)==4 and {row.get('role')for row in rows if isinstance(row,dict)}==set(BUILD_INPUTS),'actual four generated build inputs required')
 for row in rows:
  require(set(row)=={'role','path','sha256'}and row['path']==str(BUILD_INPUTS[row['role']])and re.fullmatch('[0-9a-f]{64}',row['sha256']),'build input identity differs');pins['candidate_build_'+row['role']]=(Path(row['path']),row['sha256'])
 cache=BUILD_INPUTS['cache'].read_text();config=BUILD_INPUTS['config'].read_text();settings=BUILD_INPUTS['settings_cpp'].read_text()
 require(re.findall(r'^ENABLE_WEB_AUDIO:[A-Z]+=(\S+)$',cache,re.M)==['ON']and re.search(r'^CMAKE_HOME_DIRECTORY:INTERNAL=/workspace/WebKit-Symbian-graphics$',cache,re.M),'actual C28 cache must enable WebAudio for the correct source tree')
 require(re.search(r'^#define ENABLE_WEB_AUDIO 1$',config,re.M),'actual generated config must enable WebAudio')
 require(re.search(r'#if OS\(SYMBIAN\)\s+#define SETTING_DEFAULT_VALUE_FOR_audioWorkletEnabled false',settings),'actual generated Symbian worklet default must be false')
 for field,filename in [('build_identities','build-identities.json'),('source_identities','source-identities.json')]:
  row=value[field];require(type(row)is dict and set(row)=={'path','sha256'}and row['path']==str(CANDIDATE_ROOT/filename)and re.fullmatch('[0-9a-f]{64}',row['sha256']),'actual C28 '+field+' artifact required');pins['candidate_'+field]=(Path(row['path']),row['sha256'])
 rows=value['source_files'];require(type(rows)is list and len(rows)<=256,'bounded C28 source snapshot required');seen=set()
 for row in rows:
  require(type(row)is dict and set(row)=={'relative','path','sha256'}and type(row['relative'])is str and type(row['path'])is str,'source snapshot fields differ')
  relative=Path(row['relative']);p=Path(row['path']);require(not relative.is_absolute()and '..'not in relative.parts and row['relative']not in seen and p.is_relative_to(CANDIDATE_ROOT/'source')and re.fullmatch('[0-9a-f]{64}',row['sha256']),'source snapshot identity differs');seen.add(row['relative']);pins['candidate_source_'+row['relative']]=(p,row['sha256'])
 require(REQUIRED_SOURCES<=seen,'C28 snapshot missing backend/worklet/media-source inputs')
 return pins
def native_rows():
 result=[]
 for root in PATCH_ROOTS:
  for name,(size,expected)in NATIVE.items():
   path=root/name;actual=identity(path)if path.is_file()and not path.is_symlink()else None
   result.append({'path':str(path),'expected_bytes':size,'expected_sha256':expected,'actual':actual,'matches':actual is not None and actual['bytes']==size and actual['sha256']==expected})
 return result
def checked_inputs(args,strict_native=False):
 pins=dict(STATIC);pins.update(sources());pins.update(candidate_pins(args));pins.update(candidate_metadata_pins(args));pins['host']=(HOST,HOST_SHA)
 for name in ['mediaclientaudiostream_general.dll','mediaclientaudiostream.dll.map']:pins['ostream_source_'+name]=(OSTREAM_SOURCE/name,NATIVE[name][1])
 result={}
 for name,(path,expected)in pins.items():
  p=safe_file(path);actual=identity(p);require(actual['sha256']==expected,'pinned input changed: '+name);result[name]=actual
 rows=native_rows();require(len(rows)==14,'fourteen native copies required')
 for index,row in enumerate(rows):
  if strict_native:require(row['matches'],'native deployment not pinned: '+row['path']);safe_file(Path(row['path']))
  if row['actual']:result['native_copy_'+str(index)]=row['actual']
 return result,rows
def no_host():require(subprocess.run(['pgrep','-x','eka2l1_qt'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=3).returncode==1,'EKA host active/process check failed')
def read_log(path,limit):
 if not path.exists():return b''
 require(path.is_file()and not path.is_symlink()and path.stat().st_size<=limit,'owned log exceeds bound or unsafe');data=path.read_bytes();require(len(data)<=limit,'owned log grew beyond bound');return data
def no_fatal(*logs):require(not any(FATAL.search(data.decode('utf-8',errors='replace'))for data in logs),'fresh host/target fatal or nonzero guest exit; host0 is insufficient')
def result_record(log,token):
 found=[]
 for line in log.decode('utf-8',errors='replace').splitlines():
  if'console:'in line and'WEBAUDIO-RESULT 'in line:
   try:value=json.loads(line.split('WEBAUDIO-RESULT ',1)[1])
   except ValueError:continue
   if isinstance(value,dict)and value.get('run')==token and value.get('kind')=='result':found.append(value)
 require(len(found)<=1,'duplicate fresh browser result');return found[0]if found else None
def completion_gate(result,host,guest,validation):
 no_fatal(host,guest)
 require(type(result.get('host_exit'))is int and result['host_exit']==0,'normal host exit0 required')
 require(result.get('wm_close_sent')is True and result.get('wm_close_after_functional')is True,'ordinary own-PID close after complete functional result required')
 require(result.get('forced_cleanup')is False and result.get('deadline_expired')is False,'deadline/failure cleanup cannot complete')
 require(result['restoration']['guest'].get('restored')is True and result['restoration']['configuration'].get('restored')is True,'original guest/config/bindings/Qt not restored')
 require(result.get('owned_host_stopped_before_restore')is True and result.get('inputs_unchanged')is True,'host ownership/unchanged inputs gate failed')
 require(validation.get('status')=='PUBLIC_API_COMPLETE'and validation.get('native_audio_verified')is False,'fresh full public API validator did not complete')
 return{'status':'PUBLIC_API_RUN_COMPLETE','native_audio_verified':False,'scope':'public APIs plus normal owned-parent close and full restoration only; Root independently verifies PCM and trusted host input','validation':validation}
def execute(args,token,inputs):
 output=args.output
 require(Path(os.environ.get('XDG_DATA_HOME','/invalid')).resolve()==BASE.parent,'source eka-validation/env.sh');require(Path(os.environ.get('XDG_CONFIG_HOME','/invalid')).resolve()==QT.parent,'isolated XDG_CONFIG_HOME required');require(os.environ.get('DISPLAY')and os.environ.get('PULSE_SERVER')=='unix:/workspace/symbian-media-work/runtime/native','source both runtime environments')
 output.mkdir(mode=0o700);result={'status':'FAIL','run':token,'native_audio_verified':False,'root_pcm_capture_required':True,'root_trusted_input_required':True,'inputs':inputs,'wrapper':identity(Path(__file__)),'started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'deadline_seconds':TIMEOUT,'deadline_expired':False,'forced_cleanup':False,'wm_close_sent':False,'wm_close_after_functional':False,'restoration':{'guest':{},'configuration':{}},'wrapper_screenshots_taken':False,'private_data_archived':False,'tls_verification_disabled':False}
 support=module(STATIC['preservation'][0],'webaudio_preservation');config=module(STATIC['configuration'][0],'webaudio_configuration');sys.path.insert(0,str(FIXTURE));validator=module(FIXTURE/'validate.py','webaudio_validator')
 host=server=saved=configurations=host_stream=server_stream=None;guest_path=None;fresh=False;errors=[];locks=[];hostlog=output/'emulator.log';guestlog=output/'guest.log';old_handlers={}
 def save_result():
  temporary=output/'result.json.tmp';temporary.write_text(json.dumps(result,indent=2)+'\n');temporary.replace(output/'result.json')
 def close_owned(functional):
  if host and host.poll()is None:
   closed=subprocess.run([sys.executable,str(STATIC['close_window'][0]),str(host.pid)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=6);require(closed.returncode==0,'own-PID WM close failed');result.update(wm_close_sent=True,wm_close_after_functional=functional,close_requested_monotonic=time.monotonic())
 def interrupted(signum,_):raise RuntimeError('wrapper signal '+str(signum))
 for sig in [signal.SIGINT,signal.SIGTERM]:old_handlers[sig]=signal.signal(sig,interrupted)
 try:
  with contextlib.ExitStack()as stack:
   for name in ['.jsc-run.lock','.browser-run.lock']:
    path=DATA/name;require(not path.is_symlink(),'symlink lock refused');stream=stack.enter_context(path.open('a'));fcntl.flock(stream,fcntl.LOCK_EX|fcntl.LOCK_NB);locks.append(stream)
   no_host();require(checked_inputs(args,True)[0]==inputs,'input changed before isolation')
   saved=support.SavedFiles(DATA,token,result['restoration']['guest']);configurations=config.PrivateConfigurations(BASE/('.webaudio-config-originals-'+token),[(BASE/'config.yml',False),(BASE/'bindings',True),(QT,True),(DATA/'devices.yml',False)],result['restoration']['configuration'])
   try:
    saved.reserve(support.resolve_guest(DATA,r'C:\Data\WebKit'),directory=True);fresh=True
    resources={'browser':r'C:\sys\bin\browser.exe','ui':r'C:\resource\apps\browser_ui.rsc','registration':r'C:\private\10003a3f\import\apps\browser_reg.rsc','localization':r'C:\resource\apps\browser_loc.rsc','icu':r'C:\resource\icu\icudt70l.dat','fontconfig':r'C:\resource\fontconfig\fonts.conf'}
    for name,dest in resources.items():path=saved.reserve(support.resolve_guest(DATA,dest));shutil.copy2(inputs[name]['path'],path);require(sha(path)==inputs[name]['sha256'],'resource deployment mismatch')
    configurations.reserve_all();guest_path=support.resolve_guest(DATA,r'C:\Data\WebKit\log.txt');require(not guest_path.exists(),'fresh browser log already present')
    env=os.environ.copy();env.pop('DEBUG_CURL',None);env.pop('CURL_VERBOSE',None)
    server_stream=(output/'fixture.log').open('xb');server=subprocess.Popen([sys.executable,'-B',str(FIXTURE/'fixture.py'),'--serve','--run',token,'--output',str(output/'server'),'--port','8864','--deadline',str(TIMEOUT+20)],stdout=server_stream,stderr=subprocess.STDOUT,env=env,start_new_session=True)
    ready_path=output/'server/ready.json';until=time.monotonic()+10
    while not ready_path.exists():require(server.poll()is None and time.monotonic()<until,'fixture startup failed/timed out');time.sleep(.1)
    ready=json.loads(ready_path.read_text());url='http://localhost:8864/?run='+token;require(ready.get('suite')=='classic-webaudio'and ready.get('run')==token and ready.get('url')==url and ready.get('port')==8864,'fixture identity/URL differs');result['url']=url
    (output/'host-pid.public').write_text('starting\n');host_stream=hostlog.open('xb');limit=resource.getrlimit(resource.RLIMIT_CORE)
    try:
     resource.setrlimit(resource.RLIMIT_CORE,(0,limit[1]));host=subprocess.Popen([str(HOST),'--device','RM-707','--run',r'C:\sys\bin\browser.exe',' '+url],cwd=HOST.parent,env=env,stdout=host_stream,stderr=subprocess.STDOUT,start_new_session=True,pass_fds=tuple(stream.fileno()for stream in locks))
    finally:resource.setrlimit(resource.RLIMIT_CORE,limit)
    result.update(host_pid=host.pid,launched_monotonic=time.monotonic());(output/'host-pid.public').write_text(str(host.pid)+'\n');save_result();deadline=time.monotonic()+TIMEOUT;functional=None
    while time.monotonic()<deadline:
     require(host.poll()is None,'host exited before public API completion');require(server.poll()is None,'owned fixture exited before completion');hl=read_log(hostlog,16*1024*1024);gl=read_log(guest_path,1024*1024);no_fatal(hl,gl);record=result_record(gl,token)
     if record:
      require(set(record)=={'run','kind','status'}and record['status']=='PUBLIC_API_COMPLETE','browser reported wrong/failed fixture result');state=json.loads(read_log(output/'server/state.json',256*1024));functional=validator.validate(state,token,'complete')
      if functional.get('status')=='PUBLIC_API_COMPLETE':break
     time.sleep(.1)
    else:result['deadline_expired']=True;raise RuntimeError('180s public API/trusted manual UI deadline expired')
    result['public_api_completed_monotonic']=time.monotonic();time.sleep(2);require(host.poll()is None and server.poll()is None,'host/server exited during quiet observation');no_fatal(read_log(hostlog,16*1024*1024),read_log(guest_path,1024*1024));state=json.loads(read_log(output/'server/state.json',256*1024));functional=validator.validate(state,token,'complete');require(functional.get('status')=='PUBLIC_API_COMPLETE','public API result changed during quiet observation');result['functional_validation']=functional;result['public_event_times']=[{'kind':event['kind'],'sequence':event['sequence'],'server_received_monotonic':stamp}for event,stamp in zip(state['events'],state['received_monotonic'])];save_result();close_owned(True);result['host_exit']=host.wait(timeout=15)
   finally:
    for sig in old_handlers:signal.signal(sig,signal.SIG_IGN)
    if host and host.poll()is None:
     result['forced_cleanup']=True
     try:close_owned(False);host.wait(timeout=10)
     except(OSError,RuntimeError,subprocess.SubprocessError)as error:errors.append(str(error))
    if host:result['host_exit']=host.poll()
    if host_stream:host_stream.close()
    if server and server.poll()is None:
     try:server.terminate();server.wait(timeout=5)
     except(OSError,subprocess.SubprocessError):errors.append('owned fixture did not stop normally')
    if server_stream:server_stream.close()
    stopped=not host or host.poll()is not None;result['owned_host_stopped_before_restore']=stopped
    if stopped:
     try:no_host()
     except(OSError,RuntimeError,subprocess.SubprocessError)as error:stopped=False;result['owned_host_stopped_before_restore']=False;errors.append(str(error))
    if fresh and guest_path and guest_path.is_file():
     try:guestlog.write_bytes(read_log(guest_path,1024*1024))
     except(OSError,RuntimeError)as error:errors.append(str(error))
    if stopped:
     for transaction in [configurations,saved]:
      if transaction:
       try:transaction.restore()
       except(OSError,RuntimeError)as error:errors.append(str(error))
    else:result['private_originals_retained']=True;errors.append('live host: keep private originals/journals, no restore')
    try:result['inputs_unchanged']=checked_inputs(args,True)[0]==inputs
    except(OSError,RuntimeError)as error:result['inputs_unchanged']=False;errors.append(str(error))
   require(not errors,'; '.join(errors));result['functional_validation']=validator.validate(json.loads(read_log(output/'server/state.json',256*1024)),token,'complete');result['acceptance']=completion_gate(result,read_log(hostlog,16*1024*1024),read_log(guestlog,1024*1024),result['functional_validation']);result['status']='PUBLIC_API_RUN_COMPLETE'
 except(Exception,KeyboardInterrupt)as error:result['error']=str(error)or'interrupted'
 finally:
  for sig,handler in old_handlers.items():signal.signal(sig,handler)
  result['restoration_errors']=errors;result['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();save_result()
 return result
def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--execute',action='store_true');parser.add_argument('--output',required=True,type=Path);parser.add_argument('--browser',type=Path);parser.add_argument('--browser-sha256');parser.add_argument('--resource-dir',type=Path);parser.add_argument('--candidate-metadata',type=Path);parser.add_argument('--candidate-metadata-sha256');args=parser.parse_args()
 try:
  for path in [args.output,*args.output.parents]:require(not path.is_symlink(),'output symlink ancestor refused')
  require(args.output.is_absolute()and args.output==args.output.resolve()and not args.output.exists()and not args.output.is_relative_to(BASE.parent.parent),'fresh canonical absolute output outside emulator required');require(not args.resource_dir or args.browser,'--resource-dir requires an explicit browser pin');inputs,rows=checked_inputs(args,args.execute);token='wa-'+uuid.uuid4().hex
  if not args.execute:
   blockers=(['C28_BROWSER_PATH_AND_SHA256_REQUIRED']if'browser'not in inputs else[])+(['C28_BUILD_AND_SOURCE_METADATA_REQUIRED']if'candidate_metadata'not in inputs else[])+['NATIVE_COPY_NOT_MATCHING: '+row['path']for row in rows if not row['matches']]
   print(json.dumps({'status':'PREPARED_NOT_EXECUTED','future_run':token,'output_will_be':str(args.output),'ready_to_execute':not blockers,'blockers':blockers,'inputs':inputs,'native_expected_copies':14,'native_copies':rows,'deadline_seconds':TIMEOUT,'port':8864,'root_pcm_capture_required':True,'root_trusted_input_required':True,'native_audio_verified':False,'runtime_started':False},indent=2));return 0
  require('browser'in inputs and'candidate_metadata'in inputs,'--execute requires explicit C28 browser and build/source metadata pins');result=execute(args,token,inputs);print(json.dumps({key:result.get(key)for key in ['status','run','host_exit','error','forced_cleanup','restoration_errors','native_audio_verified']},indent=2));return 0 if result['status']=='PUBLIC_API_RUN_COMPLETE'else 1
 except(OSError,ValueError,TypeError,RuntimeError,subprocess.SubprocessError)as error:print('FAIL: '+str(error),file=sys.stderr);return 1
if __name__=='__main__':raise SystemExit(main())
