#!/usr/bin/env python3
"""Read-only public evidence checks; never starts browser, input, capture or playback."""
from pathlib import Path
import hashlib,importlib.util,json,math,re,sys

ROOT=Path(__file__).resolve().parent
SOURCE='886d227f1780298cc17cf99695a3c78c0b7d9ff6'
IMAGE='30fd5b179a1ca595216c77da1e4bf4561e9d362dcc2dcde9fd4354d4cb1c0429'
METADATA='4869f3ca993e787ab509c5563a15bb618e629d9828ffda026b821d816bac98fb'
RAW_SHA='b7ed4045048998e220d33385ab58e13f31ee48073e8c407c9a3b1627f03093bf'
RAW_BYTES=4567540
LABELS=['click-start','native-running','click-suspend','native-suspended','click-resume','native-resumed','click-close','quiet']
NATIVE={
 'mediaclientaudio_general.dll':(72620,'a07674f2957410a00f0bcdbc997590feca50eb028147a1497934713d8aa65d49'),
 'mediaclientvideo_v100.dll':(52556,'03fd46f1ce54a2b5863fe841e87bdc17b99fbd05ef99459f5a56b7dbd75209c3'),
 'mediaclientaudio.dll.map':(48796,'5fadc737e5182ddd24230885af23717cd7537daf7427501ff09e43a9e40df5d3'),
 'mediaclientvideo.dll.map':(11393,'1d0f3e76bcbc25d65207b0cb5674bcbbc9c19e58bdf0959b2d3ac0f7f9824282'),
 'avkonfep_general.dll':(99488,'fd6092f2ad61d39ccb47475229c367f5494fe68a8bd2bd8d0e04dc1bafcdc7ae'),
 'mediaclientaudiostream_general.dll':(40384,'8559122f1caeea69e9612ee68f641f9ca90501a12183c5fbd3b77368cdbede04'),
 'mediaclientaudiostream.dll.map':(5027,'e92b9ede1b1a11355120e7a661be025f795e86841deec5c41b78fe7b691bc4ce')}
def read(rel):return json.loads((ROOT/rel).read_text())
def digest(path):
 h=hashlib.sha256()
 with Path(path).open('rb')as stream:
  for block in iter(lambda:stream.read(1048576),b''):h.update(block)
 return h.hexdigest()
def finite(value):return type(value)in(int,float)and math.isfinite(value)
def close(a,b,tol=1e-8):return finite(a)and finite(b)and abs(a-b)<=tol
def verify():
 checks={}
 def check(name,value):
  checks[name]=bool(value)
  if not value:raise ValueError(name)
 files=[p for p in ROOT.rglob('*')if p.is_file()];check('no_symlinks',not any(p.is_symlink()for p in ROOT.rglob('*')))
 prohibited_names={'result.json','config.yml','devices.yml','restore-journal.json','items.json','inventory.json'}
 prohibited_suffixes={'.raw','.exe','.elf','.dll','.sis','.zip','.log','.pyc'}
 check('no_private_or_binary_payloads',not any(p.name in prohibited_names or p.suffix.lower()in prohibited_suffixes or '__pycache__'in p.parts or any(part.lower()in {'profile','profiles','backups','private','inventory'}for part in p.relative_to(ROOT).parts)for p in files))
 if(ROOT/'SHA256SUMS').exists():
  listed={}
  for line in(ROOT/'SHA256SUMS').read_text().splitlines():
   expected,rel=line.split('  ',1);check('manifest_path:'+rel,bool(re.fullmatch('[0-9a-f]{64}',expected))and not Path(rel).is_absolute()and '..'not in Path(rel).parts and rel not in listed);listed[rel]=expected;check('sha256:'+rel,digest(ROOT/rel)==expected)
  check('manifest_complete_file_set',set(listed)=={str(p.relative_to(ROOT))for p in files if p.name!='SHA256SUMS'})
 provenance=read('copy-provenance.json')
 for rel,pin in provenance.items():
  p=ROOT/rel;check('copied:'+rel,p.stat().st_size==pin['bytes']and digest(p)==pin['sha256'])
 state=read('public/state.json');run=state['run'];accept=read('acceptance.json')
 check('actual_CI50_scope',accept['status']=='PASS'and accept['run']==run and accept['source_commit']==SOURCE and accept['ci_run']==41299027063 and accept['package_version']=='0.3.50'and accept['browser']['bytes']==59863884 and accept['browser']['sha256']==IMAGE and accept['metadata_sha256']==METADATA)
 check('acceptance_boundaries',accept['timing_alignment_verified']is False and accept['SIS_installer_tested']is False and accept['phone_tested']is False and set(accept['not_verified'])=={'genuine native resource-loss notification','seamless pause','official EndTask'})
 check('combined_acceptance_gates',accept['real_trusted_input_actions']==4 and accept['host_exit']==0 and accept['forced_cleanup']is False and accept['deadline_expired']is False and accept['guest_profile_and_resources_restored']is True and accept['configuration_bindings_Qt_devices_restored']is True and accept['pins_unchanged']is True and accept['native_expected_pinned_copies']==14)
 pinlist=ROOT/'harness/fixture/fixture-pins.json';check('unchanged_strict_fixture',digest(pinlist)=='9b60cc00760a5a1590b741741fc6edb473feef069cac533243c9bebc83362db7')
 for name,expected in read('harness/fixture/fixture-pins.json').items():check('fixture:'+name,digest(ROOT/'harness/fixture'/name)==expected)
 # Import only the pinned, inert public report validator; no server/wrapper/input code.
 protocol_previous=sys.modules.pop('protocol',None);sys.path.insert(0,str(ROOT/'harness/fixture'))
 try:
  spec=importlib.util.spec_from_file_location('c29_public_fixture_validator',ROOT/'harness/fixture/validate.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);validation=module.validate(state,run,'complete')
 finally:
  sys.path.pop(0);sys.modules.pop('protocol',None)
  if protocol_previous is not None:sys.modules['protocol']=protocol_previous
 check('strict_API_complete',validation['status']=='PUBLIC_API_COMPLETE'and validation['native_audio_verified']is False and validation['failures']==[]and len(state['events'])==15 and accept['public_api']==validation)
 summary=read('run-summary.json');g=summary['booleans'];check('summary_whitelist',set(summary)<={'schema','status','run','scope','booleans'}and summary['run']==run and summary['status']=='CI_BROWSER_PAYLOAD_RUN_COMPLETE'and all(type(v)is bool for v in g.values()))
 for key in ['host_exit_zero','wm_close_sent','wm_close_after_functional','owned_host_stopped_before_restore','inputs_unchanged','guest_profile_and_resources_restored','configuration_bindings_Qt_devices_restored','restoration_errors_empty']:check('outer_true:'+key,g[key]is True)
 for key in ['forced_cleanup','deadline_expired','private_data_archived','native_audio_verified','wrapper_screenshots_taken','tls_verification_disabled']:check('outer_false:'+key,g[key]is False)
 check('outer_PCM_and_input_required',g['root_pcm_capture_required']is True and g['root_trusted_input_required']is True)
 meta=read('identity/runtime-metadata.json');check('actual_CI_metadata_identity',digest(ROOT/'identity/runtime-metadata.json')==METADATA and meta['schema']=='classic-webaudio-c29-ci-v1'and meta['candidate_id']=='C29'and meta['ci']['source_commit']==SOURCE and meta['ci']['run_id']==41299027063 and meta['assets']['browser']['sha256']==IMAGE and meta['assets']['ui']['bytes']==242 and len(meta['source_files'])==13 and meta['release_default_web_audio']=='ON'and meta['runtime_audio_worklet_enabled']is False)
 check('CI_metadata_not_runtime_claim',meta['claims']['sis_installer_tested']is False and meta['claims']['physical_phone_tested']is False and meta['claims']['runtime_verified_by_metadata']is False and 'build_inputs'not in meta)
 native=read('identity/public-asset-pins.json');native_pins=native['pins'];check('native_fourteen_explicit_pins',all('native_copy_'+str(i)in native_pins for i in range(14))and sum(key.startswith('native_copy_')for key in native_pins)==14 and native['run_inputs_unchanged']is True)
 for i in range(14):
  pin=native_pins['native_copy_'+str(i)];name=Path(pin['path']).name;check('native_known_identity:'+str(i),name in NATIVE and(pin['bytes'],pin['sha256'])==NATIVE[name])
 for role in ['browser','ui','registration','localization','icu','fontconfig']:
  pin=native_pins[role];check('CI_asset_pin:'+role,(pin['bytes'],pin['sha256'])==(meta['assets'][role]['bytes'],meta['assets'][role]['sha256']))
 actions=[json.loads(line)for line in(ROOT/'public/trusted-input.jsonl').read_text().splitlines()];cal=read('public/calibration.json');mark_document=read('pcm/marks.json');rows=mark_document['marks'];marks={m['label']:m['observed_monotonic']for m in rows}
 check('four_real_owned_actions',len(actions)==4 and[x['step']for x in actions]==['start','suspend','resume','close'])
 check('public_calibration',cal['visually_reviewed']is True and cal['run']==run and cal['css_viewport']==[360,532]and cal['content_rectangle']==[270,77,360,532]and cal['window_geometry']==[1,25,900,700]and cal['device_pixel_ratio']==1 and cal['image']=='native-ready.png')
 check('host_markers',mark_document['run']==run and[r['label']for r in rows]==LABELS and all(finite(r['observed_monotonic'])for r in rows)and all(a['observed_monotonic']<b['observed_monotonic']for a,b in zip(rows,rows[1:])))
 for action,event in zip(actions,[state['events'][i]for i in[9,11,12,13]]):
  button=action['layout']['button'];point=[round(cal['content_rectangle'][0]+button['x']+button['width']/2),round(cal['content_rectangle'][1]+button['y']+button['height']/2)]
  check('real_input:'+action['step'],action['run']==run and action['pid']==actions[0]['pid']and action['window']==actions[0]['window']and action['geometry']==cal['window_geometry']and action['point']==point==[450,223]and action['method']=='real XTest pointer press/release'and action['before_monotonic']<action['after_monotonic']==marks['click-'+action['step']]and event['trusted']is True)
 check('single_reviewed_public_screenshot',len(list(ROOT.rglob('*.png')))==1 and(ROOT/'public/native-ready.png').is_file())
 analysis=read('pcm/analysis.json');capture=read('pcm/capture.json');ready=read('pcm/ready.json');raw=read('pcm/raw-reference.json')
 check('PCM_metadata_scope',analysis['status']=='PCM_OBSERVATIONS_COMPLETE'and analysis['whole_run_verified']is False and analysis['timing_alignment_verified']is False and analysis['run']==run and capture['whole_run_verified']is False and mark_document['provenance']=='root-host-observations-not-independent-input-proof')
 check('capture_normal_bounded_stop',capture['status']=='CAPTURE_STOPPED_REQUIRES_ANALYSIS'and capture['execute_requested']is True and capture['stop_reason']=='root_signal'and capture['forced_cleanup']is False and capture['deadline_expired']is False and capture['pacat_returncode']==0 and capture['tail_bytes_after_stop']==0 and capture['frame_aligned']is True)
 check('PCM_format_identity',analysis['pcm']==accept['pcm']and analysis['pcm']=={k:capture[k]for k in['sha256','bytes','duration_seconds','format','sample_rate','channels']}and capture['format']=='s16le'and capture['sample_rate']==48000 and capture['channels']==2 and capture['bytes']==RAW_BYTES and capture['sha256']==RAW_SHA and capture['bytes']%4==0 and close(capture['duration_seconds'],RAW_BYTES/192000))
 check('PCM_ready',ready['status']=='PCM_BYTES_RECEIVED'and ready['bytes']==capture['first_pcm']['bytes']and ready['observed_monotonic']==capture['first_pcm']['observed_monotonic'])
 check('PCM_analyzer_capture_source_pins',digest(ROOT/'harness/pcm/analyze.py')==analysis['analyzer_sha256']and digest(ROOT/'harness/pcm/capture.py')==capture['capture_script_sha256'])
 check('original_thresholds',analysis['thresholds']=={'window_after_marker_seconds':[.3,.8],'minimum_marker_to_next_click_seconds':1.2,'maximum_pcm_origin_spread_seconds':.15,'running_minimum_rms_s16':100,'expected_hz':997,'peak_frequency_tolerance_hz':3,'band_half_width_hz':10,'minimum_expected_band_power_fraction':.95,'maximum_peak_exclusive_s16':32760,'maximum_exact_zero_gap_seconds':.001,'silence_maximum_rms_s16':2})
 origin=analysis['estimated_origin_range_monotonic'];check('delivery_origin_limits',origin==capture['estimated_origin_range_monotonic']and len(origin)==2 and all(finite(v)for v in origin)and 0<=origin[1]-origin[0]<=.15 and close(capture['origin_spread_seconds'],origin[1]-origin[0]))
 for index,label in[(1,'native-running'),(3,'native-suspended'),(5,'native-resumed')]:
  next_action=actions[(index+1)//2];check('physical_steady_hold:'+label,next_action['before_monotonic']-marks[label]>=1.2)
 check('physical_final_quiet_hold',capture['stopped_monotonic']-marks['quiet']>=1.2)
 observations=analysis['observations'];check('PCM_four_observations',len(observations)==4 and[o['label']for o in observations]==['native-running','native-suspended','native-resumed','quiet']and observations==accept['pcm_observations'])
 for observation in observations:
  label=observation['label'];lo,hi=observation['pcm_window_seconds'];check('PCM_window:'+label,observation['marker_monotonic']==marks[label]and observation['observation_passed']is True and 0<=lo<hi<=capture['duration_seconds']and observation['frames']==round((hi-lo)*48000)and observation['frames']>=24000)
  check('PCM_delivery_window:'+label,close(lo,math.floor((marks[label]+.3-origin[1])*48000)/48000)and close(hi,math.ceil((marks[label]+.8-origin[0])*48000)/48000))
  if label in ['native-running','native-resumed']:
   check('PCM_running:'+label,min(observation['rms_s16'])>100 and max(observation['peak_s16'])<32760 and all(finite(v)and abs(v-997)<=3 for v in observation['dominant_hz'])and min(observation['expected_band_power_fraction'])>=.95 and observation['maximum_exact_zero_gap_seconds']<.001 and observation['exact_zero']is False)
  else:check('PCM_quiet:'+label,observation['exact_zero']is True and observation['rms_s16']==[0.0,0.0]and observation['peak_s16']==[0.0,0.0]and observation['dominant_hz']==[None,None])
 check('raw_reference_not_archived',raw['copied']is False and raw['bytes']==RAW_BYTES and raw['sha256']==RAW_SHA and not list(ROOT.rglob('*.raw')))
 raw_path=Path(raw['path']);external_checked=raw_path.is_file()
 if external_checked:check('external_raw_identity',not raw_path.resolve().is_relative_to(ROOT.resolve())and raw_path.stat().st_size==RAW_BYTES and digest(raw_path)==RAW_SHA)
 return{'status':'PASS_SCOPED_PUBLIC_ARCHIVE_REVIEW','scope':'Host-only public identities/API/input/calibration and recorded PCM-condition checks; no new runtime, capture, playback, installer/phone test or exact timing claim.','checks':checks,'strict_validation':validation,'external_raw_identity_checked':external_checked,'timing_alignment_verified':False,'SIS_installer_verified':False,'physical_phone_verified':False}
if __name__=='__main__':
 try:print(json.dumps(verify(),indent=2))
 except(Exception)as error:print(json.dumps({'status':'FAIL_SCOPED_PUBLIC_ARCHIVE_REVIEW','error':str(error)}));raise SystemExit(1)
