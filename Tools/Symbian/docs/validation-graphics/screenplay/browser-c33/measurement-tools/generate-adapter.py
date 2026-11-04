#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parent
s=Path('/workspace/symbian-screenplay-root-private-nodbus-host37-harness-work/adapter.py').read_text()
def replace(old,new):
 global s
 assert s.count(old)==1,(old[:100],s.count(old))
 s=s.replace(old,new)
replace('"""Finite Root-owned ScreenPlay v3 private-client API adapter. Default prepares; execution is explicit."""','"""Preserved Root-owned C33 ScreenPlay browser observation. Default prepares only."""')
a=s.index("HOST=Path(");b=s.index('PRESERVE=',a)
s=s[:a]+'''HOST=Path('/workspace/symbian-screenplay-host0039-work/eka2l1_qt')
HOST_SHA='dc6dd3067fbc155353154db0f0a90246913e4f03edf8f36e5558db32c2f29968'
CANDIDATE=Path('/workspace/symbian-belle-work/candidate33')
'''+s[b:]
a=s.index('def pins():');b=s.index('def safe_output',a)
s=s[:a]+'''def pins():
 canonical(HOST);need(HOST.is_file()and sha(HOST)==HOST_SHA,'HOST_PIN_CHANGED')
 for path,expected in [(PRESERVE,'ff27d4f3e25d2014cc3a21d8133df532966dc9c202bb5aa35eb26a9bcf40d80c'),(CONFIG,'e6679c12d66fadb4ad6c69f46f0deede57470f379d5f64591d1e6c312b8015d0')]:
  canonical(path);need(path.is_file()and sha(path)==expected,'PRESERVATION_HELPER_PIN_CHANGED')
 support=module(ROOT/'src/browser_support.py','browser_public_pins')
 for name,pin in json.loads((ROOT/'candidate-files.json').read_text()).items():support.verify_file(CANDIDATE/name,pin)
 for relative,pin in json.loads((ROOT/'runtime-pins.json').read_text()).items():support.verify_file(ROOT/relative,pin)
'''+s[b:]
a=s.index('def validate_native_record');b=s.index('def configure_disabled_dbus',a)
s=s[:a]+s[b:]
replace(" import os_sandbox\n result=", " import os_sandbox\n support=module(ROOT/'src/browser_support.py','owned_browser_support')\n result=")
replace("result={'status':'FAIL','run':token,'record_version':3,'private_update_client':True,'native_suite':'screenplay-private-capability','native_guest_launch_attempted':False,'native_guest_started':False,'screenplay_capability_known':False,'screenplay_api_verified':False,'capability_control_complete':False,'display_verified':False,'drives_restored':False,'configuration_restored':False,'host_stopped':False,'forced_cleanup':False,'originals_retained':False,'lockkeeper_retained':False}","result={'status':'FAIL','run':token,'record_version':1,'suite':'screenplay-browser-c33','browser_launch_attempted':False,'browser_started':False,'browser_control_complete':False,'screenplay_native_path_verified':False,'display_verified':False,'trusted_input_verified':False,'phone_verified':False,'drives_restored':False,'configuration_restored':False,'host_stopped':False,'forced_cleanup':False,'originals_retained':False,'lockkeeper_retained':False}")
replace("  (view/'config.yml').write_text(json.dumps({'data-storage':str(DATA),'device':0,'log-filter':'*:Error Kernel:Trace','enable-gdb-stub':False})+'\\n')", "  (view/'config.yml').write_text(json.dumps({'data-storage':str(DATA),'device':0,'log-filter':'*:Error Kernel:Trace','enable-gdb-stub':False,'screen-scale-ratio':100,'background-image':'','enable-nearest-neighbor-filter':True})+'\\n')")
replace("  if QT.exists():shutil.copytree(QT,qt,copy_function=shutil.copy2)\n  else:qt.mkdir(mode=0o700)","  qt.mkdir(mode=0o700)\n  (qt/'EKA2L1.conf').write_text('[General]\\nstretchDisplay=false\\n')")
replace("  target=case_path(roots[0],['sys','bin','screenplay_private_capability.exe']);shutil.copy2(CANARY,target);need(sha(target)==CANARY_SHA,'CANARY_DEPLOYMENT_CHANGED')\n  guest_log=profile/'screenplay-private-capability.log'",'''  result['candidate_assets']=support.stage_candidate_in_isolated_clone(roots[0],CANDIDATE,isolation_verified=True)
  page_log=support.FreshPageLog(profile/'log.txt',token)
  controller=support.BrowserController(page_log,display,HOST,ROOT/'bin/owned-wm-close',sha(ROOT/'bin/owned-wm-close'),ROOT/'bin/owned-color-rois',sha(ROOT/'bin/owned-color-rois'),page_wait_seconds=30,geometry_helper=ROOT/'bin/owned-native-leaf',geometry_sha=sha(ROOT/'bin/owned-native-leaf'))
  url=page_log.check.url_for(token)''')
replace("  for name in ['resources','compat','scripts']:\n   asset=canonical(Path('/workspace/EKA2L1-validation/build/bin')/name);need(asset.is_dir(),'PUBLIC_RUNTIME_ASSET_MISSING');(view/name).mkdir(mode=0o700);asset_ro.append({'source':str(asset),'destination':str(BASE/name)})",'''  for name in ['resources','scripts']:
   asset=canonical(Path('/workspace/EKA2L1-validation/build/bin')/name);need(asset.is_dir(),'PUBLIC_RUNTIME_ASSET_MISSING');(view/name).mkdir(mode=0o700);asset_ro.append({'source':str(asset),'destination':str(BASE/name)})
  compat_source=canonical(Path('/workspace/EKA2L1-validation/build/bin/compat'));need(compat_source.is_dir(),'PUBLIC_COMPAT_ASSET_MISSING')
  compat_view=private/'compat-view';shutil.copytree(compat_source,compat_view,copy_function=shutil.copy2)
  override=case_path(compat_view,['E1A5C002.yml'])
  override.write_text(json.dumps({'fps':60,'screen-rotation':0,'screen-mode':-1,'screen-upscale':-1,'screen-upscale-method':0,'filter-shader-path':'','should-child-inherit-setting':True})+'\\n')
  (view/'compat').mkdir(mode=0o700);rw.append({'source':str(compat_view),'destination':str(BASE/'compat')})
  patch_view=private/'mmf-patch-view';result['curated_patch_view']=support.create_curated_patch_view(patch_view,Path('/workspace/EKA2L1-validation/build/bin/patch'))
  asset_ro.append({'source':str(patch_view),'destination':str(BASE/'patch')})''')
replace("  env.update(HOME=str(temporary/'home'),PULSE_COOKIE=str(temporary/'pulse/cookie'))", "  env.update(HOME=str(temporary/'home'),PULSE_COOKIE=str(temporary/'pulse/cookie'),QT_SCALE_FACTOR='1',QT_FONT_DPI='96',QT_ENABLE_HIGHDPI_SCALING='0')")
a=s.index("  no_host();pins();result['native_guest_launch_attempted']=True");b=s.index("  result['sandbox']=",a)
s=s[:a]+'''  no_host();pins();result['browser_launch_attempted']=True
  sandbox=os_sandbox.run_sandbox(spec,[str(HOST),'--device','RM-707','--run',r'C:\\sys\\bin\\browser.exe',' '+url],timeout=60,output_handler=output,controller_hook=controller.tick)
  result['browser_observation']=controller.finish(sandbox)
'''+s[b:]
replace("'unsupported_async','output_fatal','error_code']}","'unsupported_async','output_fatal','error_code','controller_error_count']}")
a=s.index("  need(guest_log.is_file()");b=s.index(' except Refusal as error:',a)
s=s[:a]+'''  observation=result['browser_observation']
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
'''+s[b:]
s=s.replace("result['native_guest_launch_attempted']","result['browser_launch_attempted']")
s=s.replace("'NORMAL_API_SNAPSHOT_COMPLETE'","'BROWSER_SCREENPLAY_DISPLAY_VERIFIED'")
a=s.index(" result['capability_control_complete']=");b=s.index('\ndef main():',a)
s=s[:a]+''' result['browser_control_complete']=result['status']=='BROWSER_SCREENPLAY_DISPLAY_VERIFIED'
 result['screenplay_native_path_verified']=result['browser_control_complete']
 result['display_verified']=result['browser_control_complete']
 return result
'''+s[b:]
a=s.index('def main():')
s=s[:a]+'''def main():
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
 if created:(args.output/'result.json').write_text(json.dumps(result,indent=2)+'\\n')
 print(json.dumps(result,indent=2));return 0 if result['status']in ['PREPARED_NOT_EXECUTED','BROWSER_SCREENPLAY_DISPLAY_VERIFIED']else 1
if __name__=='__main__':raise SystemExit(main())
'''
(root/'adapter.py').write_text(s)
