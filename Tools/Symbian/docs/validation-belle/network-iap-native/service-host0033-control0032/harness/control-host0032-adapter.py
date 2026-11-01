#!/usr/bin/env python3
"""Finite Root-owned API adapter. Default prepares; execution is explicit."""
import argparse,fcntl,hashlib,importlib.util,json,os,re,select,shutil,signal,stat,subprocess,uuid
from pathlib import Path
ROOT=Path(__file__).resolve().parent
DATA=Path('/workspace/eka-validation/xdg/data/EKA2L1/data');BASE=DATA.parent
QT=Path('/workspace/eka-validation/xdg/config/EKA2L1')
HOST=Path('/workspace/symbian-iap-root-host32-control-20261101/eka2l1_qt')
HOST_SHA='692abe6937ed1b651914ff6ab35a78bbd3693a705ba47853fc6f9a438ae178bd'
CANARY=ROOT/'build/e32/iap_owned_start_canary.exe'
CANARY_SHA='3f52a5ca2f5241bbb719f740cbbdefe68e2865fcc447675bfe900b2d917a5895'
PRESERVE=Path('/workspace/symbian-iap-proxy-preserved-probe-work/run.py')
CONFIG=Path('/workspace/symbian-cookie-store-runtime-c21-work/config_preservation.py')
RESERVE=512*1024*1024
FATAL=re.compile(rb'access violation|\bpanicked\b|KERN-EXEC|USER-EXEC|segmentation fault|\bSIGSEGV\b|\bSIGABRT\b|Main loop exited with exception|Unable to launch process|Thread .* (?:forcefully killed|terminated peacefully) with category:.*exit code: -?[1-9][0-9]*',re.I)
class Refusal(Exception):pass
def need(ok,code):
 if not ok:raise Refusal(code)
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def module(path,name):
 spec=importlib.util.spec_from_file_location(name,path);out=importlib.util.module_from_spec(spec);spec.loader.exec_module(out);return out
def canonical(path,existing=True):
 path=Path(path);need(path.is_absolute()and path==path.resolve(),'UNSAFE_PATH')
 for part in [path,*path.parents]:need(not part.is_symlink(),'SYMLINK_PATH')
 need(not existing or path.exists(),'MISSING_PATH');return path
def no_host():need(subprocess.run(['pgrep','-x','eka2l1_qt'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=3).returncode==1,'HOST_PRESENT_OR_CHECK_FAILED')
def pins():
 for path,expected in [(HOST,HOST_SHA),(CANARY,CANARY_SHA),(PRESERVE,'ff27d4f3e25d2014cc3a21d8133df532966dc9c202bb5aa35eb26a9bcf40d80c'),(CONFIG,'e6679c12d66fadb4ad6c69f46f0deede57470f379d5f64591d1e6c312b8015d0')]:
  canonical(path);need(path.is_file()and sha(path)==expected,'INPUT_PIN_CHANGED')
def safe_output(path):
 canonical(path,False);need(not path.exists(),'FRESH_OUTPUT_REQUIRED')
 for protected in ['/workspace/eka-validation','/workspace/EKA2L1-validation','/workspace/symbian-sdk','/workspace/WebKit-Symbian','/workspace/WebKit-Symbian-graphics','/workspace/symbian-media-work/runtime']:
  need(not path.is_relative_to(Path(protected)),'EXTERNAL_OUTPUT_REQUIRED')
def case_path(parent,parts,directory=False):
 current=parent
 for index,part in enumerate(parts):
  need(current.is_dir()and not current.is_symlink(),'UNSAFE_CLONE_COMPONENT')
  exact=current/part
  if exact.exists()or exact.is_symlink():
   current=exact
  else:
   matches=[entry for entry in current.iterdir()if entry.name.casefold()==part.casefold()]
   need(len(matches)<=1,'AMBIGUOUS_CLONE_COMPONENT');current=matches[0]if matches else exact
  if index<len(parts)-1 or directory:
   if not current.exists():current.mkdir(mode=0o700)
  need(not current.is_symlink(),'UNSAFE_CLONE_COMPONENT')
 return current
class LockKeeper:
 """Original lock FDs remain outside sandbox; driver death retains them."""
 def __init__(self,lock_fds,private):
  reader,writer=os.pipe();ready_r,ready_w=os.pipe();self.writer=writer;self.pid=os.fork()
  if self.pid==0:
   os.setsid()
   os.close(writer);os.close(ready_r);keep={reader,ready_w,*lock_fds}
   for name in os.listdir('/proc/self/fd'):
    fd=int(name)
    if fd>2 and fd not in keep:
     try:os.close(fd)
     except OSError:pass
   null=os.open('/dev/null',os.O_RDWR)
   for fd in range(3):os.dup2(null,fd)
   if null>2:os.close(null)
   os.write(ready_w,b'K');os.close(ready_w)
   try:message=os.read(reader,1)
   except OSError:message=b''
   if message==b'R':os._exit(0)
   while True:signal.pause()
  os.close(reader);os.close(ready_w)
  try:
   need(bool(select.select([ready_r],[],[],3)[0])and os.read(ready_r,1)==b'K','LOCKKEEPER_NOT_READY')
   (private/'lockkeeper.json').write_text(json.dumps({'pid':self.pid,'retains_on_driver_death':True})+'\n')
  except BaseException:
   try:os.kill(self.pid,signal.SIGKILL)
   except ProcessLookupError:pass
   os.waitpid(self.pid,0);os.close(writer);raise
  finally:os.close(ready_r)
 def release(self):
  os.write(self.writer,b'R');os.close(self.writer);os.waitpid(self.pid,0)

def execute(args,token):
 import os_sandbox
 result={'status':'FAIL','run':token,'native_guest_launch_attempted':False,'native_guest_started':False,'commdb_scope_known':False,'routing_verified':False,'drives_restored':False,'configuration_restored':False,'host_stopped':False,'forced_cleanup':False,'originals_retained':False,'lockkeeper_retained':False}
 need(Path(os.environ.get('XDG_DATA_HOME','/invalid')).resolve()==BASE.parent,'SOURCE_RUNTIME_ENVIRONMENTS')
 need(Path(os.environ.get('XDG_CONFIG_HOME','/invalid')).resolve()==QT.parent,'SOURCE_RUNTIME_ENVIRONMENTS')
 display=os.environ.get('DISPLAY','');need(re.fullmatch(r':[0-9]+(?:\.[0-9]+)?',display)is not None,'FIXED_LOCAL_DISPLAY_REQUIRED')
 need(os.environ.get('PULSE_SERVER')=='unix:/workspace/symbian-media-work/runtime/native','FIXED_PULSE_REQUIRED')
 private=canonical(args.private_root,False);safe_output(private)
 need(not private.is_relative_to(args.output)and not args.output.is_relative_to(private),'PRIVATE_OUTPUT_OVERLAP')
 need(private.parent.stat().st_dev==DATA.stat().st_dev,'PRIVATE_ROOT_CROSS_FILESYSTEM')
 preserve=module(PRESERVE,'owned_start_preservation');config=module(CONFIG,'owned_start_configuration')
 locks=[];keeper=drives=configs=None;config_report={};sandbox=None;restoration_error=False;private_created=False
 fatal=False;output_total=0;output_tail=b''
 def output(data):
  nonlocal fatal,output_total,output_tail
  output_total+=len(data);pending=output_tail+data;fatal|=FATAL.search(pending)is not None;output_tail=pending[-65536:]
 try:
  for name in ['.jsc-run.lock','.browser-run.lock']:
   path=DATA/name;need(not path.is_symlink(),'UNSAFE_LOCK')
   if not path.exists():
    fd=os.open(path,os.O_CREAT|os.O_EXCL|os.O_WRONLY|os.O_NOFOLLOW,0o600);os.close(fd)
   fd=os.open(path,os.O_RDONLY|os.O_NOFOLLOW);fcntl.flock(fd,fcntl.LOCK_EX|fcntl.LOCK_NB);locks.append(fd)
  no_host();pins();catalog,roots=preserve.selected_scope();inventories=[];needed=0
  for path in roots:
   if path.exists()or path.is_symlink():rows,size=preserve.scan_tree(path,DATA.stat().st_dev)
   else:canonical(path,False);rows,size=[],0
   inventories.append(rows);needed+=size
  need(bool(inventories[0]),'ORIGINAL_C_REQUIRED')
  need(shutil.disk_usage(DATA).free>=needed+64*1024*1024+RESERVE,'SPACE_BEFORE_CLONE')
  private.mkdir(mode=0o700);private_created=True;keeper=LockKeeper(locks,private)
  configs=config.PrivateConfigurations(private/'config-originals',[(BASE/'config.yml',False),(BASE/'bindings',True),(QT,True),(DATA/'devices.yml',False)],config_report)
  drives=preserve.WholeDrives(roots,private,private/'restore-journal.json')
  configs.reserve_all();drives.isolate(inventories)
  view=private/'config-view';view.mkdir(mode=0o700);(view/'data').mkdir(mode=0o700);(view/'patch').mkdir(mode=0o700)
  if (BASE/'bindings').is_dir():shutil.copytree(BASE/'bindings',view/'bindings',copy_function=shutil.copy2)
  else:(view/'bindings').mkdir(mode=0o700)
  (view/'config.yml').write_text(json.dumps({'data-storage':str(DATA),'device':0,'log-filter':'*:Error Kernel:Trace','enable-gdb-stub':False})+'\n')
  devices=private/'devices.yml';devices.write_text(json.dumps(catalog)+'\n');qt=private/'qt-view'
  if QT.exists():shutil.copytree(QT,qt,copy_function=shutil.copy2)
  else:qt.mkdir(mode=0o700)
  cwd=private/'logs';cwd.mkdir(mode=0o700);(cwd/'patch').mkdir(mode=0o700)
  temporary=private/'tmp';temporary.mkdir(mode=0o700)
  for name in ['cache','runtime','pulse','x11','shm','home']:(temporary/name).mkdir(mode=0o700)
  (temporary/'pulse/cookie').write_bytes(os.urandom(256))
  (temporary/'pulse/client.conf').write_text('autospawn = no\nenable-shm = no\nenable-memfd = no\n')
  profile=case_path(roots[0],['Data','WebKit'],True);shutil.rmtree(profile);profile.mkdir(mode=0o700)
  target=case_path(roots[0],['sys','bin','iap_owned_start_canary.exe']);shutil.copy2(CANARY,target);need(sha(target)==CANARY_SHA,'CANARY_DEPLOYMENT_CHANGED')
  guest_log=profile/'iap-owned-start.log'
  firmware=next(iter(catalog));rom_source=canonical(DATA/'roms'/firmware.lower()/'SYM.ROM')
  need(rom_source.is_file()and not rom_source.is_symlink(),'ROM_SOURCE_NOT_REGULAR')
  rom_digest=sha(rom_source);rom_working=private/'SYM.ROM';shutil.copyfile(rom_source,rom_working);rom_working.chmod(0o600)
  need(sha(rom_working)==rom_digest and rom_working.stat().st_ino!=rom_source.stat().st_ino,'ROM_WORKING_COPY_MISMATCH')
  (private/'rom-preservation-private.json').write_text(json.dumps({'source':str(rom_source),'sha256':rom_digest})+'\n')
  need(shutil.disk_usage(DATA).free>=RESERVE,'SPACE_AFTER_CLONE')
  x11=Path('/tmp/.X11-unix')/('X'+display[1:].split('.')[0]);pulse=Path('/workspace/symbian-media-work/runtime/native')
  for path in [x11,pulse]:need(stat.S_ISSOCK(path.stat().st_mode),'FIXED_SOCKET_ABSENT')
  env={'PATH':'/usr/bin:/bin','LANG':'C.UTF-8','DISPLAY':display,'PULSE_SERVER':'unix:'+str(pulse),'PULSE_CLIENTCONFIG':str(temporary/'pulse/client.conf'),'XDG_DATA_HOME':str(BASE.parent),'XDG_CONFIG_HOME':str(QT.parent),'XDG_CACHE_HOME':str(temporary/'cache'),'XDG_RUNTIME_DIR':str(temporary/'runtime'),'TMPDIR':str(temporary),'QT_X11_NO_MITSHM':'1','QT_XCB_NO_MITSHM':'1','LIBGL_DRI3_DISABLE':'1','LIBGL_ALWAYS_SOFTWARE':'1','LD_LIBRARY_PATH':'/workspace/host-sysroot/usr/lib/x86_64-linux-gnu','QT_PLUGIN_PATH':'/workspace/host-sysroot/usr/lib/x86_64-linux-gnu/qt6/plugins'}
  env.update(HOME=str(temporary/'home'),PULSE_COOKIE=str(temporary/'pulse/cookie'))
  rw=[{'source':str(view),'destination':str(BASE)},{'source':str(qt),'destination':str(QT)},{'source':str(devices),'destination':str(DATA/'devices.yml')},{'source':str(cwd),'destination':str(cwd)},{'source':str(temporary),'destination':str(temporary)},{'source':str(temporary),'destination':'/tmp'}]
  rw.extend({'source':str(path),'destination':str(path)}for path in roots)
  rw.append({'source':str(rom_working),'destination':str(rom_source)})
  asset_ro=[]
  data_view=view/'data'
  j2me_original_pins={}
  def readonly_children(original,working):
   for child in original.iterdir():
    canonical(child);dest=working/child.name
    if child.parent==DATA/'j2me'and child.name in ['applist.db','applist.db-journal','applist.db-wal','applist.db-shm']:
     need(child.is_file()and child.stat().st_size<=64*1024*1024,'J2ME_REGISTRY_COPY_BOUND')
     expected=sha(child);shutil.copy2(child,dest)
     need(sha(dest)==expected and dest.stat().st_ino!=child.stat().st_ino,'J2ME_REGISTRY_COPY_MISMATCH')
     j2me_original_pins[str(child)]=expected;continue
    if child.is_dir():
     dest.mkdir(mode=0o700)
     if child in roots:continue
     if any(root.is_relative_to(child)for root in roots):readonly_children(child,dest);continue
    elif child.is_file():dest.touch(mode=0o600)
    else:need(False,'UNSUPPORTED_DATA_CHILD_NODE')
    asset_ro.append({'source':str(child),'destination':str(child)})
  for entry in DATA.iterdir():
   canonical(entry);target_view=data_view/entry.name
   if entry.is_dir():
    target_view.mkdir(mode=0o700)
    if entry.name.casefold()!='cache':readonly_children(entry,target_view)
   elif entry.is_file():shutil.copy2(entry,target_view)
   else:need(False,'UNSUPPORTED_DATA_ROOT_NODE')
  rw.append({'source':str(data_view),'destination':str(DATA)})
  font_cache=temporary/'fontconfig-system';font_cache.mkdir(mode=0o700)
  need(Path('/var/cache/fontconfig').is_dir(),'SYSTEM_FONT_CACHE_POINT_MISSING')
  rw.append({'source':str(font_cache),'destination':'/var/cache/fontconfig'})
  for name in ['resources','compat','scripts']:
   asset=canonical(Path('/workspace/EKA2L1-validation/build/bin')/name);need(asset.is_dir(),'PUBLIC_RUNTIME_ASSET_MISSING');(view/name).mkdir(mode=0o700);asset_ro.append({'source':str(asset),'destination':str(BASE/name)})
  rw.append({'source':str(temporary/'runtime'),'destination':'/run'})
  protected=[str(private/'config-originals')]+[entry['backup']for entry in drives.entries if entry['moved']]
  spec={'rw_binds':rw,'ro_binds':asset_ro+[{'source':str(x11),'destination':str(x11)},{'source':str(pulse),'destination':str(pulse)}],'protected_paths':protected,'cwd':str(cwd),'env':env,'allowed_unix_sockets':[str(x11),str(pulse)]}
  no_host();pins();result['native_guest_launch_attempted']=True
  sandbox=os_sandbox.run_sandbox(spec,[str(HOST),'--device','RM-707','--run',r'C:\sys\bin\iap_owned_start_canary.exe',' --run '+token],timeout=60,output_handler=output)
  if sandbox.get('stopped')is True and guest_log.is_file()and not guest_log.is_symlink()and guest_log.stat().st_size<=16384:
   (private/'native-report-before-gates.txt').write_bytes(guest_log.read_bytes());result['native_record_present']=True
  result['sandbox']={key:sandbox.get(key)for key in ['exit_code','stopped','forced_cleanup','child_pid','guard_ok','guard_error_code','audit_ok','denied_mutations','denied_call_counts','denied_role_counts','unsupported_async','output_fatal','error_code']}
  result['forced_cleanup']=sandbox.get('forced_cleanup')is True
  need(sandbox.get('stopped')is True,'OWNED_SANDBOX_LIVE')
  need(sandbox.get('exit_code')==0 and sandbox.get('guard_ok')is True and sandbox.get('audit_ok')is True,'SANDBOX_OR_AUDIT_FAILED')
  need(sandbox.get('denied_mutations')==0 and sandbox.get('unsupported_async')==0 and not sandbox.get('output_fatal'),'SANDBOX_WRITE_OR_ASYNC_VIOLATION')
  need(not result['forced_cleanup']and not fatal and output_total<=16*1024*1024,'TARGET_HEALTH_FAILED')
  owned_log=view/'EKA2L1.log';need(owned_log.is_file()and not owned_log.is_symlink()and owned_log.stat().st_size<=16*1024*1024,'OWNED_HOST_LOG_BOUND')
  with owned_log.open('rb')as stream:
   for line in stream:need(not FATAL.search(line),'TARGET_FATAL_OUTCOME')
  need(guest_log.is_file()and not guest_log.is_symlink()and guest_log.stat().st_size<=16384,'FRESH_NATIVE_RECORD_REQUIRED')
  native_report=guest_log.read_text();(private/'native-public-record-private.txt').write_text(native_report);validator=module(ROOT/'validate.py','owned_start_validator');result['snapshot']=validator.validate(native_report,token);(args.output/'native-public-record.txt').write_text(native_report);result['native_guest_started']=True
  need(result['snapshot']['timed_out']is False and result['snapshot']['owned_handle_closed']is True,'NATIVE_TIMEOUT_OR_CLOSE_FAILED')
  result['normal_completion_evidence']='source_control_flow_plus_fresh_public_result_and_frontend_target_exit'
 except Refusal as error:result['error_code']=str(error)
 except BaseException:result['error_code']='UNEXPECTED_PRIVATE_FAILURE'
 finally:
  stopped=not result['native_guest_launch_attempted']or(sandbox is not None and sandbox.get('stopped')is True)
  if stopped:
   try:no_host()
   except BaseException:stopped=False
  result['host_stopped']=stopped
  if stopped:
   if drives:
    try:result['drives_restored']=drives.restore(True)
    except BaseException:restoration_error=True
   else:result['drives_restored']=True
   if configs:
    try:configs.restore();result['configuration_restored']=config_report.get('restored')is True
    except BaseException:restoration_error=True
   else:result['configuration_restored']=True
  else:restoration_error=True
  if 'j2me_original_pins'in locals():
   try:result['original_j2me_registry_bytes_unchanged']=all(sha(Path(name))==expected for name,expected in j2me_original_pins.items());need(result['original_j2me_registry_bytes_unchanged'],'ORIGINAL_J2ME_REGISTRY_CHANGED')
   except BaseException:restoration_error=True
  if 'rom_source'in locals():
   try:result['original_ROM_bytes_unchanged']=sha(rom_source)==rom_digest;need(result['original_ROM_bytes_unchanged'],'ORIGINAL_ROM_CHANGED')
   except BaseException:restoration_error=True
  try:pins();result['inputs_unchanged']=True
  except BaseException:result['inputs_unchanged']=False;restoration_error=True
  retained=restoration_error or not result['drives_restored']or not result['configuration_restored'];result['originals_retained']=retained
  if keeper:
   if not retained:
    try:keeper.release()
    except BaseException:restoration_error=True;result['error_code']='LOCKKEEPER_RELEASE_FAILED'
   else:result['lockkeeper_retained']=True
  for fd in locks:os.close(fd)
  if not result.get('error_code')and not retained and result.get('inputs_unchanged')is True:result['status']='NORMAL_API_SNAPSHOT_COMPLETE'
  if result['status']!='NORMAL_API_SNAPSHOT_COMPLETE':result.setdefault('error_code','RESTORE_OR_INPUT_GATE_FAILED')
  if private_created:
   try:
    (private/'adapter-status.json').write_text(json.dumps({'status':result['status'],'originals_retained':retained,'lockkeeper_retained':result['lockkeeper_retained']})+'\n')
    if result['status']=='NORMAL_API_SNAPSHOT_COMPLETE':shutil.rmtree(private)
   except BaseException:result.update(status='FAIL',error_code='PRIVATE_JOURNAL_OR_CLEANUP_FAILED')
 return result

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',required=True,type=Path);parser.add_argument('--private-root',type=Path);parser.add_argument('--execute',action='store_true');args=parser.parse_args()
 old_umask=os.umask(0o077);created=False;result={'status':'FAIL','native_guest_started':False,'commdb_scope_known':False,'routing_verified':False}
 try:
  safe_output(args.output);pins();args.output.mkdir(mode=0o700);created=True
  if args.execute:
   need(args.private_root is not None,'EXPLICIT_PRIVATE_ROOT_REQUIRED');result=execute(args,'owned-'+uuid.uuid4().hex)
  else:result.update(status='PREPARED_NOT_EXECUTED',execution_allowed_in_this_task=False,canary_sha256=CANARY_SHA,host_sha256=HOST_SHA)
 except Refusal as error:result.update(status='FAIL',error_code=str(error))
 except BaseException:result.update(status='FAIL',error_code='UNEXPECTED_PRIVATE_FAILURE')
 finally:os.umask(old_umask)
 if created:(args.output/'result.json').write_text(json.dumps(result,indent=2)+'\n')
 print(json.dumps(result,indent=2));return 0 if result['status']in ['PREPARED_NOT_EXECUTED','NORMAL_API_SNAPSHOT_COMPLETE']else 1
if __name__=='__main__':raise SystemExit(main())
