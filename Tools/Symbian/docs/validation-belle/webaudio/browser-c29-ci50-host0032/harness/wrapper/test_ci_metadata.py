#!/usr/bin/env python3
"""Synthetic report-consumer guards; no production identity, network or runtime."""
import copy,hashlib,json,tempfile,unittest,zipfile
from pathlib import Path
from unittest.mock import patch
import ci_metadata as m

class Fixture:
 def __init__(self,root):
  self.root=root;self.candidate=root/'candidate29';self.candidate.mkdir();self.payload_dir=root/'synthetic-payloads';self.payload_dir.mkdir();self.sis_dir=root/'synthetic-sis';self.sis_dir.mkdir();self.artifact_id=123456789
  self.blobs={name:('SYNTHETIC CI source '+name+'\n').encode()for name in m.MINIMUM_REQUIRED|{m.OPTIONS}}
  self.blobs[m.OPTIONS]=b'WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_AUDIO PRIVATE ON)\n'
  self.blobs['Source/WebKitLegacy/symbian/WebViewImpl.cpp']=b'#if ENABLE(WEB_AUDIO)\nsettings.setWebAudioEnabled(true);\nsettings.setAudioWorkletEnabled(false);\nsettings.setRequiresUserGestureForAudioPlayback(true);\n#endif\n'
  self.blobs['Source/WebCore/page/Settings.yaml']=b'AudioWorkletEnabled:\n  type: bool\n  condition: ENABLE(WEB_AUDIO)\n  defaultValue:\n    WebCore:\n      OS(SYMBIAN): false\n      default: true\n'
  packs=[];spacks=[];summary=[];self.payloads={}
  for name,(package_uid,targets)in m.PACKAGES.items():
   image=('SYNTHETIC SIS '+name).encode();p=self.sis_dir/name;p.write_bytes(image);entries=[]
   for t in targets:
    basename=t.rsplit('/',1)[-1];value=('SYNTHETIC PAYLOAD '+basename).encode()
    if basename=='browser_ui.rsc':value=b'SYNTHETIC UI'+b'\0'*(242-len(b'SYNTHETIC UI'))
    path=self.payload_dir/basename;path.write_bytes(value);item={'target':t.replace('/','\\'),'bytes':len(value),'sha256':m.digest(path),'file_hash_valid':True,'extracted_path':str(path)}
    if t.endswith('.exe'):item.update(e32_flags='0x12100028',hardware_float_type=1,capabilities=hex(0x2000 if name=='browser.sis'else 0),e32_uid3=hex(0xe1a5c002 if name=='browser.sis'else 0xe1a5c001))
    entries.append(item);self.payloads[(name,t)]=item
   packs.append({'package':name,'uid':hex(package_uid),'version':m.VERSION,'bytes':len(image),'sha256':m.digest(p),'path':str(p),'header_and_section_crc':'PASS','all_payload_hashes_valid':True,'files':copy.deepcopy(entries)})
   spacks.append({'name':name,'uid3':hex(package_uid),'version':m.VERSION,'signature_chains':0,'header_and_section_crc_valid':True,'payloads':copy.deepcopy(entries)})
   summary.append({'name':name,'bytes':len(image),'sha256':m.digest(p)})
  for _,(pack,t,basename)in m.ASSETS.items():(self.candidate/basename).write_bytes(Path(self.payloads[(pack,t)]['extracted_path']).read_bytes())
  self.zip_path=root/'SYNTHETIC-artifact.zip'
  with zipfile.ZipFile(self.zip_path,'w')as z:
   z.writestr('SHA256SUMS','\n'.join(row['sha256']+'  '+row['name']for row in summary)+'\n')
   for row in summary:z.write(self.sis_dir/row['name'],row['name'])
  az={'path':str(self.zip_path),'bytes':self.zip_path.stat().st_size,'sha256':m.digest(self.zip_path),'artifact_id':self.artifact_id};ui=self.payloads[('browser.sis','!:/resource/apps/browser_ui.rsc')];browser=self.payloads[('browser.sis','!:/sys/bin/browser.exe')]
  self.auth_source={'status':'PASS_SOURCE_ONLY_ARTIFACT_PENDING','source_commit':m.SOURCE_COMMIT,'checks':{'password_API_uses_official_SDK_enum':True,'password_resource_maximum_255':True,'no_secret_resource_256':True},'sdk_reference':{'value':255},'expected_runtime_UI_resource':{'bytes':242,'sha256':ui['sha256']}}
  self.auth_source_path=root/'synthetic-auth-source.json';self.auth_source_path.write_text(json.dumps(self.auth_source))
  self.reports={
   'run':{'id':m.RUN_ID,'run_number':m.RUN_NUMBER,'head_sha':m.SOURCE_COMMIT,'head_branch':m.BRANCH,'event':'push','status':'completed','conclusion':'success','repository':{'full_name':m.REPOSITORY}},
   'artifacts':{'artifacts':[{'id':self.artifact_id,'name':'sis','expired':False,'size_in_bytes':az['bytes'],'digest':'sha256:'+az['sha256'],'workflow_run':{'id':m.RUN_ID,'head_sha':m.SOURCE_COMMIT,'head_branch':m.BRANCH}}]},
   'verification':{'status':'PASS','repo':m.REPOSITORY,'run_id':m.RUN_ID,'job_id':m.JOB_ID,'head_sha':m.SOURCE_COMMIT,'package_version':m.VERSION,'payload_count':13,'workflow_status':'success','all_three_sis_sha256_match_artifact_manifest':True,'all_package_headers_sections_crc_valid':True,'all_decompressed_payload_sha1_valid':True,'all_payload_sizes_valid':True,'sis_artifact':{'id':self.artifact_id,'bytes':az['bytes'],'sha256':az['sha256'],'matches_github_digest':True},'packages':summary,'auth_password_source_API_and_RSS_limit':255,'auth_resource':{'bytes':242,'sha256':ui['sha256']},'release_default_WEB_AUDIO':'ON'},
   'archive':{'status':'PASS','artifact_id':self.artifact_id,'bytes':az['bytes'],'sha256':az['sha256'],'matches_github_digest':True,'zip_member_crc_valid':True,'workflow_head_sha':m.SOURCE_COMMIT,'workflow_conclusion':'success','job_id':m.JOB_ID,'job_conclusion':'success','members':['SHA256SUMS',*m.PACKAGES]},
   'package':{'status':'PASS','run_id':m.RUN_ID,'head_sha':m.SOURCE_COMMIT,'branch':m.BRANCH,'packages':packs},
   'sis_payload':{'status':'PASS','run_id':m.RUN_ID,'head_sha':m.SOURCE_COMMIT,'sis_version':m.VERSION,'payload_count':13,'packages':spacks},
   'auth_resource':{'status':'PASS','resource':{'path':ui['extracted_path'],'bytes':242,'sha256':ui['sha256']},'source_contract_report':{'path':str(self.auth_source_path),'sha256':m.digest(self.auth_source_path)}},
   'e32_imports':{'schema':'ci-e32-import-audit-v1','status':'PASS','source_commit':m.SOURCE_COMMIT,'run_id':m.RUN_ID,'artifact_id':self.artifact_id,'method':'parsed-e32-import-table','image':{'path':browser['extracted_path'],'bytes':browser['bytes'],'sha256':browser['sha256']},'canonical_imported_dlls':['euser.dll','mediaclientaudiostream.dll']},
  }
  self.report_paths={role:root/('synthetic-'+role+'.json')for role in self.reports};self.contract={'schema':'classic-webaudio-ci-audit-v1','source_commit':m.SOURCE_COMMIT,'run_id':m.RUN_ID,'run_number':m.RUN_NUMBER,'package_version':m.VERSION,'artifact_zip':az,'reports':{}};self.contract_path=root/'SYNTHETIC-contract.json'
  for role in self.reports:self.refresh(role)
 def refresh(self,role):
  p=self.report_paths[role];p.write_text(json.dumps(self.reports[role]));self.contract['reports'][role]={'path':str(p),'sha256':m.digest(p)};self.save()
 def save(self):self.contract_path.write_text(json.dumps(self.contract))
 def fake_git(self,*args):
  if args[0]=='cat-file':return b'commit\n'
  name=args[1].split(':',1)[1];value=self.blobs[name]
  if args[0]=='show':return value
  if args[0]=='rev-parse':return hashlib.sha1(b'blob '+str(len(value)).encode()+b'\0'+value).hexdigest().encode()+b'\n'
  raise AssertionError('unexpected git command')
 def build(self,materialize=True):return m.build_metadata(self.contract_path,m.digest(self.contract_path),m.SOURCE_COMMIT,m.MINIMUM_REQUIRED,materialize)

class ContractTests(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory(prefix='ci29-synthetic-',dir=Path(__file__).resolve().parent);self.root=Path(self.tmp.name);self.f=Fixture(self.root);self.p1=patch.object(m,'CANDIDATE',self.f.candidate);self.p2=patch.object(m,'_git',self.f.fake_git);self.p1.start();self.p2.start()
 def tearDown(self):self.p2.stop();self.p1.stop();self.tmp.cleanup()
 def bad(self):
  with self.assertRaises(RuntimeError):self.f.build()
 def test_positive_rebuild_and_exact_six_assets(self):
  value,pins=self.f.build();self.assertTrue(set(m.ASSETS)<=set(pins));self.assertEqual(len(value['source_files']),13);self.assertFalse(value['claims']['sis_installer_tested']);p=self.root/'SYNTHETIC-metadata.json';p.write_text(json.dumps(value));checked,allpins=m.verify_metadata(p,m.digest(p),m.SOURCE_COMMIT,m.MINIMUM_REQUIRED);self.assertEqual(checked,value);self.assertIn('candidate_metadata',allpins)
 def test_missing_source_materialization_rejected(self):
  with self.assertRaises(RuntimeError):self.f.build(False)
 def test_source_default_must_be_on(self):self.f.blobs[m.OPTIONS]=b'WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_AUDIO PRIVATE OFF)\n';self.bad();self.assertFalse((self.f.candidate/'source').exists())
 def test_source_commit_must_be_exact(self):
  with self.assertRaises(RuntimeError):m.source_snapshots('0'*40,m.MINIMUM_REQUIRED,True)
 def test_no_required_source_dropped(self):
  with self.assertRaises(RuntimeError):m.source_snapshots(m.SOURCE_COMMIT,list(m.MINIMUM_REQUIRED)[:-1],True)
 def test_materialized_source_mismatch_not_overwritten(self):
  self.f.build();p=self.f.candidate/'source'/m.OPTIONS;p.write_text('wrong');self.bad();self.assertEqual(p.read_text(),'wrong')
 def test_missing_report(self):del self.f.contract['reports']['e32_imports'];self.f.save();self.bad()
 def test_stale_run(self):self.f.reports['run']['id']-=1;self.f.refresh('run');self.bad()
 def test_failed_run(self):self.f.reports['run']['conclusion']='failure';self.f.refresh('run');self.bad()
 def test_other_head(self):self.f.reports['run']['head_sha']='0'*40;self.f.refresh('run');self.bad()
 def test_other_repository(self):self.f.reports['run']['repository']['full_name']='other/repo';self.f.refresh('run');self.bad()
 def test_artifact_from_other_workflow(self):self.f.reports['artifacts']['artifacts'][0]['workflow_run']['head_sha']='0'*40;self.f.refresh('artifacts');self.bad()
 def test_expired_artifact(self):self.f.reports['artifacts']['artifacts'][0]['expired']=True;self.f.refresh('artifacts');self.bad()
 def test_wrong_zip_digest(self):self.f.contract['artifact_zip']['sha256']='0'*64;self.f.save();self.bad()
 def test_missing_actual_sis(self):Path(self.f.reports['package']['packages'][0]['path']).unlink();self.bad()
 def test_incomplete_payload_audit(self):self.f.reports['sis_payload']['payload_count']=12;self.f.refresh('sis_payload');self.bad()
 def test_wrong_package_version(self):self.f.reports['package']['packages'][0]['version']='0.3.49';self.f.refresh('package');self.bad()
 def test_failed_package_crc(self):self.f.reports['package']['packages'][0]['header_and_section_crc']='FAIL';self.f.refresh('package');self.bad()
 def test_failed_payload_hash(self):self.f.reports['sis_payload']['packages'][0]['payloads'][0]['file_hash_valid']=False;self.f.refresh('sis_payload');self.bad()
 def test_missing_materialized_payload(self):Path(self.f.reports['package']['packages'][0]['files'][0]['extracted_path']).unlink();self.bad()
 def test_payload_target_duplicates(self):items=self.f.reports['package']['packages'][2]['files'];items[1]['target']=items[0]['target'];self.f.refresh('package');self.bad()
 def test_candidate_ui_383_is_not_accepted(self):(self.f.candidate/'browser_ui.rsc').write_bytes(b'x'*383);self.bad()
 def test_candidate_registration_must_be_package_bytes(self):(self.f.candidate/'browser_reg.rsc').write_bytes(b'local reg');self.bad()
 def test_candidate_icu_must_be_package_bytes(self):(self.f.candidate/'icudt70l.dat').write_bytes(b'local ICU');self.bad()
 def test_auth255_source_required(self):self.f.auth_source['sdk_reference']['value']=256;self.f.auth_source_path.write_text(json.dumps(self.f.auth_source));self.f.reports['auth_resource']['source_contract_report']['sha256']=m.digest(self.f.auth_source_path);self.f.refresh('auth_resource');self.bad()
 def test_failed_auth_source_even_with_checks_true(self):self.f.auth_source['status']='FAIL';self.f.auth_source_path.write_text(json.dumps(self.f.auth_source));self.f.reports['auth_resource']['source_contract_report']['sha256']=m.digest(self.f.auth_source_path);self.f.refresh('auth_resource');self.bad()
 def test_audit_default_off_is_not_on(self):self.f.reports['verification']['release_default_WEB_AUDIO']='OFF';self.f.refresh('verification');self.bad()
 def test_e32_strings_are_not_import_parser(self):self.f.reports['e32_imports']['method']='binary-strings';self.f.refresh('e32_imports');self.bad()
 def test_e32_actual_ostream_import_required(self):self.f.reports['e32_imports']['canonical_imported_dlls']=['euser.dll'];self.f.refresh('e32_imports');self.bad()
 def test_e32_for_another_image(self):self.f.reports['e32_imports']['image']['sha256']='0'*64;self.f.refresh('e32_imports');self.bad()
 def test_no_local_build_evidence_contract(self):self.f.contract['build_inputs']=[{'path':'/local/CMakeCache.txt'}];self.f.save();self.bad()
 def test_no_local_generated_metadata(self):
  value,_=self.f.build();value['build_inputs']=[];p=self.root/'SYNTHETIC-local-metadata.json';p.write_text(json.dumps(value))
  with self.assertRaises(RuntimeError):m.verify_metadata(p,m.digest(p),m.SOURCE_COMMIT,m.MINIMUM_REQUIRED)
 def test_unknown_metadata_field(self):
  value,_=self.f.build();value['runtime_pass']=True;p=self.root/'SYNTHETIC-extra-metadata.json';p.write_text(json.dumps(value))
  with self.assertRaises(RuntimeError):m.verify_metadata(p,m.digest(p),m.SOURCE_COMMIT,m.MINIMUM_REQUIRED)
 def test_report_bytes_tampered(self):self.f.report_paths['package'].write_text('{}');self.bad()
 def test_report_duplicate_json_rejected(self):
  p=self.f.report_paths['run'];p.write_text('{"id":1,"id":2}');self.f.contract['reports']['run']['sha256']=m.digest(p);self.f.save();self.bad()
 def test_nonfinite_json_rejected(self):
  with self.assertRaises(RuntimeError):m.strict_json('{"x":NaN}')
 def test_private_guest_path_is_not_ci_evidence(self):
  with self.assertRaises(RuntimeError):m.file('/workspace/eka-validation/xdg/data/EKA2L1/data/devices.yml')

if __name__=='__main__':
 suite=unittest.defaultTestLoader.loadTestsFromTestCase(ContractTests);result=unittest.TextTestRunner(verbosity=1).run(suite);print(json.dumps({'status':'GUARDS_PASS_NOT_RUNTIME'if result.wasSuccessful()else'GUARDS_FAIL','checks':result.testsRun,'synthetic_only':True,'CI_download_or_production_materialization':False,'server_guest_display_audio_started':False}));raise SystemExit(not result.wasSuccessful())
