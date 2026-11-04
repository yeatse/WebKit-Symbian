#!/usr/bin/env python3
"""Preserved Root-owned C33 ScreenPlay browser observation. Default prepares only."""
import argparse,fcntl,hashlib,importlib.util,json,os,re,select,shutil,signal,stat,subprocess,uuid
from pathlib import Path
ROOT=Path(__file__).resolve().parent
DATA=Path('/workspace/eka-validation/xdg/data/EKA2L1/data');BASE=DATA.parent
QT=Path('/workspace/eka-validation/xdg/config/EKA2L1')
HOST=Path('/workspace/symbian-screenplay-host0039-work/eka2l1_qt')
HOST_SHA='dc6dd3067fbc155353154db0f0a90246913e4f03edf8f36e5558db32c2f29968'
CANDIDATE=Path('/workspace/symbian-belle-work/candidate33')
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
 canonical(HOST);need(HOST.is_file()and sha(HOST)==HOST_SHA,'HOST_PIN_CHANGED')
 for path,expected in [(PRESERVE,'ff27d4f3e25d2014cc3a21d8133df532966dc9c202bb5aa35eb26a9bcf40d80c'),(CONFIG,'e6679c12d66fadb4ad6c69f46f0deede57470f379d5f64591d1e6c312b8015d0')]:
  canonical(path);need(path.is_file()and sha(path)==expected,'PRESERVATION_HELPER_PIN_CHANGED')
 support=module(ROOT/'src/browser_support.py','browser_public_pins')
 for name,pin in json.loads((ROOT/'candidate-files.json').read_text()).items():support.verify_file(CANDIDATE/name,pin)
 for relative,pin in json.loads((ROOT/'runtime-pins.json').read_text()).items():support.verify_file(ROOT/relative,pin)
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

def configure_disabled_dbus(env,runtime):
 """Use absent task-local endpoints so Qt cannot auto-launch a session bus."""
 try:need(runtime.is_absolute()and runtime==runtime.resolve()and stat.S_ISDIR(runtime.lstat().st_mode),'TASK_DBUS_RUNTIME_REQUIRED')
 except OSError:raise Refusal('TASK_DBUS_RUNTIME_REQUIRED')from None
 for name in ['screenplay-session-bus','screenplay-system-bus']:
  endpoint=runtime/name
  try:endpoint.lstat()
  except FileNotFoundError:continue
  except OSError:raise Refusal('TASK_DBUS_ENDPOINT_CHECK_FAILED')from None
  raise Refusal('TASK_DBUS_ENDPOINT_PRESENT')
 env.update(DBUS_SESSION_BUS_ADDRESS='unix:path=/run/screenplay-session-bus',DBUS_SYSTEM_BUS_ADDRESS='unix:path=/run/screenplay-system-bus')

DIAGNOSTIC_CODES=frozenset(('AMBIGUOUS_CLONE_COMPONENT', 'ATTRIBUTE_ERROR', 'BROWSER_LOG_BOUND', 'BROWSER_LOG_LINE_BOUND', 'BROWSER_LOG_OPEN_CHANGED', 'BROWSER_LOG_REPLACED', 'BROWSER_SCREENPLAY_DISPLAY_VERIFIED', 'BUILD_CURATED_MMF_PATCH_VIEW', 'BUILD_PRIVATE_COMPAT_VIEW', 'BUILD_PUBLIC_RESOURCE_VIEWS', 'BUILD_READONLY_DATA_VIEW', 'BUILD_TASK_ENVIRONMENT', 'C33', 'CASE_ALIAS_NON_ASCII', 'CLONE_PARENT_UNSAFE', 'CLONE_ROOT_UNSAFE', 'CLONE_SYMLINK', 'CONFIGURE_DISABLED_DBUS', 'CONSTRUCT_BROWSER_CONTROLLER', 'CONSTRUCT_FRESH_PAGE_LOG', 'CONSTRUCT_PUBLIC_FIXTURE_URL', 'CONTROLLER_DEADLINE_BOUND', 'CONTROLLER_HOOK_FAILED', 'COPY_BINDINGS_VIEW', 'CREATE_CONFIG_VIEW', 'CREATE_EMPTY_BROWSER_PROFILE', 'CREATE_PRESERVATION_OBJECTS', 'CREATE_PRIVATE_ROOT', 'CREATE_TASK_RUNTIME', 'DISPLAY', 'DISPLAY_AREA_BOUND', 'DISPLAY_AREA_FIELDS', 'DISPLAY_SCALE_BOUND', 'EMPTY_NEW_PROFILE_REQUIRED', 'EXPLICIT_PRIVATE_ROOT_REQUIRED', 'EXTERNAL_OUTPUT_REQUIRED', 'FAIL', 'FILE_EXISTS', 'FILE_NOT_FOUND', 'FILE_TYPE_OR_BOUND', 'FIXED_LOCAL_DISPLAY_REQUIRED', 'FIXED_PATCH_ALLOWLIST_REQUIRED', 'FIXED_PULSE_REQUIRED', 'FIXED_SOCKET_ABSENT', 'FIXTURE_PIN_CHANGED', 'FRESH_BROWSER_LOG_REQUIRED', 'FRESH_OUTPUT_REQUIRED', 'FRESH_PAGE_REQUESTS_REQUIRED', 'FRESH_PATCH_VIEW_REQUIRED', 'FRESH_PROFILE_SCOPE_REJECTED', 'FRESH_PUBLIC_PAGE_RECORD_REJECTED', 'GEOMETRY_HELPER_PIN_CHANGED', 'GEOMETRY_TYPES', 'HELPER_OUTPUT_REJECTED', 'HELPER_RECORD_REJECTED', 'HOST_PIN_CHANGED', 'HOST_PRESENT_OR_CHECK_FAILED', 'HOST_SHA_REQUIRED', 'IMPORT_ERROR', 'INDEPENDENT_MAPPING_EVIDENCE_REQUIRED', 'INVALID_COMPONENT', 'ISOLATE_DRIVES', 'IS_A_DIRECTORY', 'J2ME_REGISTRY_COPY_BOUND', 'J2ME_REGISTRY_COPY_MISMATCH', 'KEY_ERROR', 'LANG', 'LAUNCH_OWNED_BROWSER', 'LD_LIBRARY_PATH', 'LIBGL_ALWAYS_SOFTWARE', 'LIBGL_DRI3_DISABLE', 'LOCAL_DISPLAY_REQUIRED', 'LOCKKEEPER_NOT_READY', 'LOCKKEEPER_RELEASE_FAILED', 'LOCKS', 'MAPPING_FIELDS', 'MAPPING_ORIGIN_BOUND', 'MAPPING_SCALE_BOUND', 'MAPPING_TYPES', 'MISSING_PATH', 'NATIVE_CONTENT_BOUND', 'NATIVE_CONTENT_FIELDS', 'NATIVE_PUBLIC_CONTENT_GEOMETRY_REQUIRED', 'NATIVE_RENDER_WIDGET_IDENTITY_REQUIRED', 'NEW_PROFILE_REQUIRED', 'NORMAL_CLOSE_REJECTED', 'NORMAL_OWNED_CLOSE_AND_EXIT_REQUIRED', 'NOT_A_DIRECTORY', 'ORIGINAL_C_REQUIRED', 'ORIGINAL_J2ME_REGISTRY_CHANGED', 'ORIGINAL_ROM_CHANGED', 'OS_ERROR', 'OTHER_EXCEPTION', 'OWNED_GEOMETRY_FIELDS', 'OWNED_GEOMETRY_HELPER_REQUIRED', 'OWNED_GEOMETRY_IDENTITY', 'OWNED_HELPER_PIN_CHANGED', 'OWNED_HOST_LOG_BOUND', 'OWNED_NATIVE_RENDER_AREA', 'OWNED_SANDBOX_LIVE', 'OWNED_WINDOW_NORMAL_CLOSE_UNPROVEN', 'PAGE_RECORD_AFTER_DONE_OR_BOUND', 'PAGE_REQUESTS_PASS', 'PAGE_TOKEN_CHANGED', 'PAGE_UNPROVEN', 'PAGE_WRAPPER_INVALID', 'PATCH_NAME_REJECTED', 'PATH', 'PERMISSION_ERROR', 'PIN_READONLY_ROM', 'PREPARED_NOT_EXECUTED', 'PRESERVATION_HELPER_PIN_CHANGED', 'PRESERVED_CLONE_REQUIRED', 'PRIVATE_JOURNAL_OR_CLEANUP_FAILED', 'PRIVATE_OUTPUT_OVERLAP', 'PRIVATE_ROOT_CROSS_FILESYSTEM', 'PUBLIC_COLOR_ROIS_DIFFER', 'PUBLIC_COLOR_ROIS_DIFFER_OR_UNPROVEN', 'PUBLIC_COLOR_ROIS_MATCH', 'PUBLIC_COMPAT_ASSET_MISSING', 'PUBLIC_DESTINATION_REJECTED', 'PUBLIC_INPUT_PIN_CHANGED', 'PUBLIC_PAYLOAD_SET_CHANGED', 'PUBLIC_RENDER_OBSERVATION_FAILED', 'PUBLIC_ROI_OBSERVATION_REJECTED', 'PUBLIC_RUNTIME_ASSET_MISSING', 'PUBLIC_TARGET_BRANCH_CAP', 'PUBLIC_TARGET_LINK_COUNT', 'PUBLIC_TARGET_NOT_REGULAR', 'PUBLIC_TARGET_READONLY', 'PULSE_CLIENTCONFIG', 'PULSE_SERVER', 'QT_PLUGIN_PATH', 'QT_SCALE_FACTOR', 'QT_X11_NO_MITSHM', 'QT_XCB_NO_MITSHM', 'REFUSAL', 'RESERVE_CONFIGURATIONS', 'RESTORE_OR_INPUT_GATE_FAILED', 'ROI_AGGREGATE_COUNTS', 'ROI_COUNTS', 'ROI_EXIT_CODE', 'ROI_EXIT_STATUS', 'ROI_FIELDS', 'ROI_IDENTITY', 'ROI_PATCH_COUNT', 'ROI_PATCH_COUNTS', 'ROI_PATCH_FIELDS', 'ROI_PATCH_TYPES', 'ROI_STATUS', 'ROM_SOURCE_NOT_REGULAR', 'RUNTIME_ERROR', 'SANDBOX_OR_AUDIT_FAILED', 'SANDBOX_WRITE_OR_ASYNC_VIOLATION', 'SCAN_SELECTED_DRIVES', 'SCREENPLAY_FIXED_TAG_REJECTED', 'SCREENPLAY_NATIVE_SUBMIT_REQUIRED', 'SELECT_SCOPE', 'SOURCE_RUNTIME_ENVIRONMENTS', 'SPACE_AFTER_CLONE', 'SPACE_BEFORE_CLONE', 'STAGE_CANDIDATE_AND_FIXTURE', 'STAGE_DIAGNOSTIC_REJECTED', 'STAGE_PUBLIC_ALIAS_PLAN', 'STAGE_PUBLIC_COPY', 'STAGE_PUBLIC_DIRECTORIES', 'STAGE_PUBLIC_FIXTURE', 'STAGE_PUBLIC_INPUTS', 'STAGE_PUBLIC_PROFILE', 'SUPPORT_REFUSAL', 'SYMLINK_FILE', 'SYMLINK_PATH', 'SYSTEM_FONT_CACHE_POINT_MISSING', 'TARGET_FATAL_OUTCOME', 'TARGET_HEALTH_FAILED', 'TASK_DBUS_ENDPOINT_CHECK_FAILED', 'TASK_DBUS_ENDPOINT_PRESENT', 'TASK_DBUS_RUNTIME_REQUIRED', 'TMPDIR', 'TYPE_ERROR', 'UNCLASSIFIED_PHASE', 'UNCLASSIFIED_REFUSAL', 'UNEXPECTED_PRIVATE_FAILURE', 'UNSAFE_CLONE_COMPONENT', 'UNSAFE_LOCK', 'UNSAFE_PATH', 'UNSUPPORTED_DATA_CHILD_NODE', 'UNSUPPORTED_DATA_ROOT_NODE', 'UNWRAPPED_OR_STALE_PAGE_RECORD', 'VALIDATED_PAGE_RECORDS_REQUIRED', 'VALUE_ERROR', 'VERIFY_BROWSER_OBSERVATION', 'WRITE_TASK_CONFIG', 'WRITE_TASK_DEVICES_AND_QT', 'XDG_CACHE_HOME', 'XDG_CONFIG_HOME', 'XDG_DATA_HOME', 'XDG_RUNTIME_DIR'))
DIAGNOSTIC_PHASES=frozenset(('BUILD_CURATED_MMF_PATCH_VIEW', 'BUILD_PRIVATE_COMPAT_VIEW', 'BUILD_PUBLIC_RESOURCE_VIEWS', 'BUILD_READONLY_DATA_VIEW', 'BUILD_TASK_ENVIRONMENT', 'CONFIGURE_DISABLED_DBUS', 'CONSTRUCT_BROWSER_CONTROLLER', 'CONSTRUCT_FRESH_PAGE_LOG', 'CONSTRUCT_PUBLIC_FIXTURE_URL', 'COPY_BINDINGS_VIEW', 'CREATE_CONFIG_VIEW', 'CREATE_EMPTY_BROWSER_PROFILE', 'CREATE_PRESERVATION_OBJECTS', 'CREATE_PRIVATE_ROOT', 'CREATE_TASK_RUNTIME', 'ISOLATE_DRIVES', 'LAUNCH_OWNED_BROWSER', 'LOCKS', 'PIN_READONLY_ROM', 'RESERVE_CONFIGURATIONS', 'SCAN_SELECTED_DRIVES', 'SELECT_SCOPE', 'STAGE_CANDIDATE_AND_FIXTURE', 'STAGE_PUBLIC_ALIAS_PLAN', 'STAGE_PUBLIC_COPY', 'STAGE_PUBLIC_DIRECTORIES', 'STAGE_PUBLIC_FIXTURE', 'STAGE_PUBLIC_INPUTS', 'STAGE_PUBLIC_PROFILE', 'VERIFY_BROWSER_OBSERVATION', 'WRITE_TASK_CONFIG', 'WRITE_TASK_DEVICES_AND_QT'))
def diagnostic_refusal(error):
 value=error.args[0]if len(error.args)==1 else None
 return value if type(value)is str and value in DIAGNOSTIC_CODES else "UNCLASSIFIED_REFUSAL"
def diagnostic_phase(phase):
 return phase if phase in DIAGNOSTIC_PHASES else "UNCLASSIFIED_PHASE"
def diagnostic_exception(error):
 allowed={PermissionError:"PERMISSION_ERROR",FileNotFoundError:"FILE_NOT_FOUND",FileExistsError:"FILE_EXISTS",NotADirectoryError:"NOT_A_DIRECTORY",IsADirectoryError:"IS_A_DIRECTORY",ValueError:"VALUE_ERROR",TypeError:"TYPE_ERROR",AttributeError:"ATTRIBUTE_ERROR",KeyError:"KEY_ERROR",OSError:"OS_ERROR",RuntimeError:"RUNTIME_ERROR",ImportError:"IMPORT_ERROR"}
 return allowed.get(type(error),"OTHER_EXCEPTION")

def execute(args,token):
 import os_sandbox
 support=module(ROOT/'src/browser_support.py','owned_browser_support')
 result={'status':'FAIL','run':token,'record_version':1,'suite':'screenplay-browser-c33','browser_launch_attempted':False,'browser_started':False,'browser_control_complete':False,'screenplay_native_path_verified':False,'display_verified':False,'trusted_input_verified':False,'phone_verified':False,'drives_restored':False,'configuration_restored':False,'host_stopped':False,'forced_cleanup':False,'originals_retained':False,'lockkeeper_retained':False}
 need(Path(os.environ.get('XDG_DATA_HOME','/invalid')).resolve()==BASE.parent,'SOURCE_RUNTIME_ENVIRONMENTS')
 need(Path(os.environ.get('XDG_CONFIG_HOME','/invalid')).resolve()==QT.parent,'SOURCE_RUNTIME_ENVIRONMENTS')
 display=os.environ.get('DISPLAY','');need(re.fullmatch(r':[0-9]+(?:\.[0-9]+)?',display)is not None,'FIXED_LOCAL_DISPLAY_REQUIRED')
 need(os.environ.get('PULSE_SERVER')=='unix:/workspace/symbian-media-work/runtime/native','FIXED_PULSE_REQUIRED')
 private=canonical(args.private_root,False);safe_output(private)
 need(not private.is_relative_to(args.output)and not args.output.is_relative_to(private),'PRIVATE_OUTPUT_OVERLAP')
 need(private.parent.stat().st_dev==DATA.stat().st_dev,'PRIVATE_ROOT_CROSS_FILESYSTEM')
 preserve=module(PRESERVE,'owned_start_preservation');config=module(CONFIG,'owned_start_configuration')
 locks=[];keeper=drives=configs=None;config_report={};sandbox=None;restoration_error=False;private_created=False
 phase='LOCKS';failure_asset=None
 def stage_diagnostic(stage,asset):
  nonlocal phase,failure_asset
  need(stage in DIAGNOSTIC_PHASES,'STAGE_DIAGNOSTIC_REJECTED')
  need(asset is None or asset in support.PUBLIC_ASSET_DESTINATIONS,'STAGE_DIAGNOSTIC_REJECTED')
  phase=stage;failure_asset=asset
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
  phase='SELECT_SCOPE'
  no_host();pins();catalog,roots=preserve.selected_scope();inventories=[];needed=0
  phase='SCAN_SELECTED_DRIVES'
  for path in roots:
   if path.exists()or path.is_symlink():rows,size=preserve.scan_tree(path,DATA.stat().st_dev)
   else:canonical(path,False);rows,size=[],0
   inventories.append(rows);needed+=size
  need(bool(inventories[0]),'ORIGINAL_C_REQUIRED')
  need(shutil.disk_usage(DATA).free>=needed+64*1024*1024+RESERVE,'SPACE_BEFORE_CLONE')
  phase='CREATE_PRIVATE_ROOT'
  private.mkdir(mode=0o700);private_created=True;keeper=LockKeeper(locks,private)
  phase='CREATE_PRESERVATION_OBJECTS'
  configs=config.PrivateConfigurations(private/'config-originals',[(BASE/'config.yml',False),(BASE/'bindings',True),(QT,True),(DATA/'devices.yml',False)],config_report)
  drives=preserve.WholeDrives(roots,private,private/'restore-journal.json')
  phase='RESERVE_CONFIGURATIONS'
  configs.reserve_all()
  phase='ISOLATE_DRIVES'
  drives.isolate(inventories)
  phase='CREATE_CONFIG_VIEW'
  view=private/'config-view';view.mkdir(mode=0o700);(view/'data').mkdir(mode=0o700);(view/'patch').mkdir(mode=0o700)
  phase='COPY_BINDINGS_VIEW'
  if (BASE/'bindings').is_dir():shutil.copytree(BASE/'bindings',view/'bindings',copy_function=shutil.copy2)
  else:(view/'bindings').mkdir(mode=0o700)
  phase='WRITE_TASK_CONFIG'
  (view/'config.yml').write_text(json.dumps({'data-storage':str(DATA),'device':0,'log-filter':'*:Error Kernel:Trace','enable-gdb-stub':False,'screen-scale-ratio':100,'background-image':'','enable-nearest-neighbor-filter':True})+'\n')
  phase='WRITE_TASK_DEVICES_AND_QT'
  devices=private/'devices.yml';devices.write_text(json.dumps(catalog)+'\n');qt=private/'qt-view'
  qt.mkdir(mode=0o700)
  (qt/'EKA2L1.conf').write_text('[General]\nstretchDisplay=false\n')
  phase='CREATE_TASK_RUNTIME'
  cwd=private/'logs';cwd.mkdir(mode=0o700);(cwd/'patch').mkdir(mode=0o700)
  temporary=private/'tmp';temporary.mkdir(mode=0o700)
  for name in ['cache','runtime','pulse','x11','shm','home']:(temporary/name).mkdir(mode=0o700)
  (temporary/'pulse/cookie').write_bytes(os.urandom(256))
  (temporary/'pulse/client.conf').write_text('autospawn = no\nenable-shm = no\nenable-memfd = no\n')
  phase='CREATE_EMPTY_BROWSER_PROFILE'
  profile=case_path(roots[0],['Data','WebKit'],True);shutil.rmtree(profile);profile.mkdir(mode=0o700)
  phase='STAGE_CANDIDATE_AND_FIXTURE'
  result['candidate_assets']=support.stage_candidate_in_isolated_clone(roots[0],CANDIDATE,isolation_verified=True,fresh_profile=profile,diagnostic=stage_diagnostic)
  phase='CONSTRUCT_FRESH_PAGE_LOG'
  page_log=support.FreshPageLog(profile/'log.txt',token)
  phase='CONSTRUCT_BROWSER_CONTROLLER'
  controller=support.BrowserController(page_log,display,HOST,ROOT/'bin/owned-wm-close',sha(ROOT/'bin/owned-wm-close'),ROOT/'bin/owned-color-rois',sha(ROOT/'bin/owned-color-rois'),page_wait_seconds=30,geometry_helper=ROOT/'bin/owned-native-leaf',geometry_sha=sha(ROOT/'bin/owned-native-leaf'))
  phase='CONSTRUCT_PUBLIC_FIXTURE_URL'
  url=page_log.check.url_for(token)
  phase='PIN_READONLY_ROM'
  firmware=next(iter(catalog));rom_source=canonical(DATA/'roms'/firmware.lower()/'SYM.ROM')
  need(rom_source.is_file()and not rom_source.is_symlink(),'ROM_SOURCE_NOT_REGULAR')
  rom_digest=sha(rom_source)
  (private/'rom-preservation-private.json').write_text(json.dumps({'source':str(rom_source),'sha256':rom_digest})+'\n')
  need(shutil.disk_usage(DATA).free>=RESERVE,'SPACE_AFTER_CLONE')
  phase='BUILD_TASK_ENVIRONMENT'
  x11=Path('/tmp/.X11-unix')/('X'+display[1:].split('.')[0]);pulse=Path('/workspace/symbian-media-work/runtime/native')
  for path in [x11,pulse]:need(stat.S_ISSOCK(path.stat().st_mode),'FIXED_SOCKET_ABSENT')
  env={'PATH':'/usr/bin:/bin','LANG':'C.UTF-8','DISPLAY':display,'PULSE_SERVER':'unix:'+str(pulse),'PULSE_CLIENTCONFIG':str(temporary/'pulse/client.conf'),'XDG_DATA_HOME':str(BASE.parent),'XDG_CONFIG_HOME':str(QT.parent),'XDG_CACHE_HOME':str(temporary/'cache'),'XDG_RUNTIME_DIR':str(temporary/'runtime'),'TMPDIR':str(temporary),'QT_X11_NO_MITSHM':'1','QT_XCB_NO_MITSHM':'1','LIBGL_DRI3_DISABLE':'1','LIBGL_ALWAYS_SOFTWARE':'1','LD_LIBRARY_PATH':'/workspace/host-sysroot/usr/lib/x86_64-linux-gnu','QT_PLUGIN_PATH':'/workspace/host-sysroot/usr/lib/x86_64-linux-gnu/qt6/plugins'}
  env.update(HOME=str(temporary/'home'),PULSE_COOKIE=str(temporary/'pulse/cookie'),QT_SCALE_FACTOR='1',QT_FONT_DPI='96',QT_ENABLE_HIGHDPI_SCALING='0')
  rw=[{'source':str(view),'destination':str(BASE)},{'source':str(qt),'destination':str(QT)},{'source':str(devices),'destination':str(DATA/'devices.yml')},{'source':str(cwd),'destination':str(cwd)},{'source':str(temporary),'destination':str(temporary)},{'source':str(temporary),'destination':'/tmp'}]
  rw.extend({'source':str(path),'destination':str(path)}for path in roots)
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
  phase='BUILD_READONLY_DATA_VIEW'
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
  phase='BUILD_PUBLIC_RESOURCE_VIEWS'
  for name in ['resources','scripts']:
   asset=canonical(Path('/workspace/EKA2L1-validation/build/bin')/name);need(asset.is_dir(),'PUBLIC_RUNTIME_ASSET_MISSING');(view/name).mkdir(mode=0o700);asset_ro.append({'source':str(asset),'destination':str(BASE/name)})
  phase='BUILD_PRIVATE_COMPAT_VIEW'
  compat_source=canonical(Path('/workspace/EKA2L1-validation/build/bin/compat'));need(compat_source.is_dir(),'PUBLIC_COMPAT_ASSET_MISSING')
  compat_view=private/'compat-view';shutil.copytree(compat_source,compat_view,copy_function=shutil.copy2)
  override=case_path(compat_view,['E1A5C002.yml'])
  override.write_text(json.dumps({'fps':60,'screen-rotation':0,'screen-mode':-1,'screen-upscale':-1,'screen-upscale-method':0,'filter-shader-path':'','should-child-inherit-setting':True})+'\n')
  (view/'compat').mkdir(mode=0o700);rw.append({'source':str(compat_view),'destination':str(BASE/'compat')})
  phase='BUILD_CURATED_MMF_PATCH_VIEW'
  patch_view=private/'mmf-patch-view';result['curated_patch_view']=support.create_curated_patch_view(patch_view,Path('/workspace/EKA2L1-validation/build/bin/patch'))
  asset_ro.append({'source':str(patch_view),'destination':str(BASE/'patch')})
  rw.append({'source':str(temporary/'runtime'),'destination':'/run'})
  protected=[str(private/'config-originals')]+[entry['backup']for entry in drives.entries if entry['moved']]
  asset_ro.append({'source':str(rom_source),'destination':str(rom_source)})
  spec={'rw_binds':rw,'ro_binds':asset_ro+[{'source':str(x11),'destination':str(x11)},{'source':str(pulse),'destination':str(pulse)}],'protected_paths':protected,'cwd':str(cwd),'env':env,'allowed_unix_sockets':[str(x11),str(pulse)]}
  phase='CONFIGURE_DISABLED_DBUS'
  configure_disabled_dbus(env,temporary/'runtime')
  phase='LAUNCH_OWNED_BROWSER'
  no_host();pins();result['browser_launch_attempted']=True
  sandbox=os_sandbox.run_sandbox(spec,[str(HOST),'--device','RM-707','--run',r'C:\sys\bin\browser.exe',' '+url],timeout=60,output_handler=output,controller_hook=controller.tick)
  phase='VERIFY_BROWSER_OBSERVATION'
  result['browser_observation']=controller.finish(sandbox)
  if page_log.report is not None and page_log.error is None:
   result['public_page_report']=page_log.report
   (args.output/'public-page-records.txt').write_text(page_log.public_text())
   result['strict_page_records_exported']=True
  result['sandbox']={key:sandbox.get(key)for key in ['exit_code','stopped','forced_cleanup','child_pid','child_pid_associated','guard_ok','guard_error_code','guard_count','audit_ok','audit_capture_ok','audit_error_count','collectors_live','unexpected_peers','unexpected_peer_role_counts','connect_parse_errors','unpaired_connects','denied_mutations','denied_call_counts','denied_role_counts','unsupported_async','output_fatal','error_code','controller_error_count','exec_candidate_count','exec_candidate_parse_errors']}
  result['forced_cleanup']=sandbox.get('forced_cleanup')is True
  need(sandbox.get('stopped')is True,'OWNED_SANDBOX_LIVE')
  need(sandbox.get('exit_code')==0 and sandbox.get('guard_ok')is True and sandbox.get('audit_ok')is True,'SANDBOX_OR_AUDIT_FAILED')
  need(sandbox.get('denied_mutations')==0 and sandbox.get('unsupported_async')==0 and not sandbox.get('output_fatal'),'SANDBOX_WRITE_OR_ASYNC_VIOLATION')
  result['original_ROM_namespace_readonly_verified']=True
  need(not result['forced_cleanup']and not fatal and output_total<=16*1024*1024,'TARGET_HEALTH_FAILED')
  owned_log=view/'EKA2L1.log';need(owned_log.is_file()and not owned_log.is_symlink()and owned_log.stat().st_size<=16*1024*1024,'OWNED_HOST_LOG_BOUND')
  with owned_log.open('rb')as stream:
   for line in stream:need(not FATAL.search(line),'TARGET_FATAL_OUTCOME')
  observation=result['browser_observation']
  need(sandbox.get('controller_error_count')==0,'CONTROLLER_HOOK_FAILED')
  need(observation['normal_close_sent']and observation['owned_host_exit_independently_observed'],'NORMAL_OWNED_CLOSE_AND_EXIT_REQUIRED')
  need(observation['page_status']=='PAGE_REQUESTS_PASS'and not observation['page_error_code'],'FRESH_PAGE_REQUESTS_REQUIRED')
  (args.output/'public-page-records.txt').write_text(page_log.public_text())
  result['browser_started']=True
  need(observation['controller_error_code']is None,'PUBLIC_RENDER_OBSERVATION_FAILED')
  need(observation['screenplay_submit_tags']>0 and observation['screenplay_fallback_tags']==0,'SCREENPLAY_NATIVE_SUBMIT_REQUIRED')
  pixels=observation['public_roi_report'];need(pixels is not None and pixels['status']=='PUBLIC_COLOR_ROIS_MATCH'and pixels['mismatch_count']==0,'PUBLIC_COLOR_ROIS_DIFFER_OR_UNPROVEN')
  geometry=observation['owned_geometry_report'];need(geometry is not None and geometry['render_widget_identity_verified']is True,'NATIVE_RENDER_WIDGET_IDENTITY_REQUIRED')
  result['normal_completion_evidence']='fresh public page plus owned render ROIs plus XRes normal close and independent host exit'
 except (Refusal,support.Refusal) as error:
  result.update(error_code=diagnostic_refusal(error),failure_phase=diagnostic_phase(phase),failure_exception_class='SUPPORT_REFUSAL'if type(error)is support.Refusal else'REFUSAL',failure_asset_name=failure_asset)
 except BaseException as error:
  result.update(error_code='UNEXPECTED_PRIVATE_FAILURE',failure_phase=diagnostic_phase(phase),failure_exception_class=diagnostic_exception(error),failure_asset_name=failure_asset)
 finally:
  stopped=not result['browser_launch_attempted']or(sandbox is not None and sandbox.get('stopped')is True)
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
  if not result.get('error_code')and not retained and result.get('inputs_unchanged')is True:
   result['status']='BROWSER_SCREENPLAY_DISPLAY_VERIFIED'
  if result['status']!='BROWSER_SCREENPLAY_DISPLAY_VERIFIED':result.setdefault('error_code','RESTORE_OR_INPUT_GATE_FAILED')
  if private_created:
   try:
    (private/'adapter-status.json').write_text(json.dumps({'status':result['status'],'originals_retained':retained,'lockkeeper_retained':result['lockkeeper_retained']})+'\n')
    if result['status']=='BROWSER_SCREENPLAY_DISPLAY_VERIFIED':
     need(stopped and result['host_stopped'] is True and not result['forced_cleanup'] and
          result['drives_restored'] is True and result['configuration_restored'] is True and
          not retained and result.get('inputs_unchanged') is True,'PRIVATE_PATCH_CLEANUP_GATE')
     no_host()
     support.prepare_private_patch_cleanup(private,patch_view)
     result['private_patch_view_cleanup_verified']=True
     shutil.rmtree(private)
   except BaseException:result.update(status='FAIL',error_code='PRIVATE_JOURNAL_OR_CLEANUP_FAILED')
 result['browser_control_complete']=result['status']=='BROWSER_SCREENPLAY_DISPLAY_VERIFIED'
 result['screenplay_native_path_verified']=result['browser_control_complete']
 result['display_verified']=result['browser_control_complete']
 return result

def main():
 global HOST,HOST_SHA
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',required=True,type=Path);parser.add_argument('--private-root',type=Path);parser.add_argument('--execute',action='store_true');parser.add_argument('--host',type=Path,default=HOST);parser.add_argument('--host-sha',default=HOST_SHA);args=parser.parse_args()
 old_umask=os.umask(0o077);created=False;result={'status':'FAIL','record_version':1,'suite':'screenplay-browser-c33','browser_started':False,'display_verified':False,'phone_verified':False}
 try:
  need(re.fullmatch(r'[0-9a-f]{64}',args.host_sha)is not None,'HOST_SHA_REQUIRED');HOST=canonical(args.host);HOST_SHA=args.host_sha
  safe_output(args.output);pins();args.output.mkdir(mode=0o700);created=True
  if args.execute:
   need(args.private_root is not None,'EXPLICIT_PRIVATE_ROOT_REQUIRED');result=execute(args,uuid.uuid4().hex)
  else:result.update(status='PREPARED_NOT_EXECUTED',guest_executed=False,original_private_inputs_read=False,drives_cloned=False,host_sha256=HOST_SHA,candidate='C33')
 except Refusal as error:result.update(status='FAIL',error_code=str(error))
 except BaseException:result.update(status='FAIL',error_code='UNEXPECTED_PRIVATE_FAILURE')
 finally:os.umask(old_umask)
 if created:(args.output/'result.json').write_text(json.dumps(result,indent=2)+'\n')
 print(json.dumps(result,indent=2));return 0 if result['status']in ['PREPARED_NOT_EXECUTED','BROWSER_SCREENPLAY_DISPLAY_VERIFIED']else 1
if __name__=='__main__':raise SystemExit(main())
