from pathlib import Path
import datetime,hashlib,json,subprocess
r=Path(__file__).resolve().parent;repo=Path('/workspace/WebKit-Symbian');head='0885f13ce4252a96b77d4c8cac0579ba2b8ba618';prior='67140bdc320bb9c911bf6f4045a3b38755b7707a'
show=lambda rev,path:subprocess.check_output(['git','show',rev+':'+path],cwd=repo)
paths=['Source/WTF/wtf/posix/FileSystemPOSIX.cpp','Source/WTF/wtf/symbian/FileSystemSymbian.cpp','Source/WTF/wtf/symbian/SymbianFilePath.h','Source/cmake/OptionsSymbian.cmake']
records=[]
for path in paths:
 data=show(head,path);assert data==show(prior,path);out=r/'utf16-source'/path;out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(data)
 blob=subprocess.check_output(['git','rev-parse',head+':'+path],cwd=repo,text=True).strip();assert hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()==blob
 records.append({'path':path,'git_blob':blob,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'identical_to_reviewed_CI48_source':True})
changed=subprocess.check_output(['git','diff-tree','--no-commit-id','--name-only','-z','-r',head],cwd=repo,text=True).rstrip('\0').split('\0');assert len(changed)==38 and not any(p.startswith('Source/') for p in changed)
assert 'WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_AUDIO PRIVATE OFF)' in show(head,paths[-1]).decode()
manifest=json.loads(Path('/workspace/symbian-ci48-validation/native-filesystem-fix/patch-manifest.json').read_text());tests=[]
for row in manifest['files']:
 data=show(head,row['path']);assert hashlib.sha256(data).hexdigest()==row['after_sha256'];out=r/'host-test-source'/row['path'];out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(data);tests.append(dict(row,bytes=len(data)))
assert len(tests)==6
priorReview=json.loads(show(head,'Tools/Symbian/docs/validation-release-0.3.48/ci/independent-source-review.json'));assert priorReview['source_commit']==prior and len(priorReview['checks'])==16 and all(priorReview['checks'].values()) and not priorReview['findings']
failed=json.loads(show(head,'Tools/Symbian/docs/validation-release-0.3.48/ci/failure-report.json'));assert failed['status']=='FAIL' and failed['run_id']==41292379739 and not failed['SIS_artifact_produced']
helper=show(head,paths[2]).decode();run=show(head,'Tools/Symbian/tests/native-filesystem/run.sh').decode();fixture=show(head,'Tools/Symbian/tests/native-filesystem/filesystem.cpp').decode()
assert 'static_assert(sizeof(wchar_t) == 2 && alignof(wchar_t) == 2);' in helper
assert '-fshort-wchar' in run and '-I"$root/Source/WTF"' in run
assert '<wtf/symbian/SymbianFilePath.h>' in fixture and 'extern "C" int wstat' in fixture and 'extern "C" int waccess' in fixture
assert not subprocess.check_output(['git','ls-tree','-r','--name-only',head,'--','Tools/Symbian/tests/native-filesystem/include/wtf/symbian'],cwd=repo)
checks={'current_commit_has_zero_production_source_edits':True,'three_UTF16_files_identical_to_independently_reviewed_CI48':True,'prior_16_source_contract_checks_passed':True,'current_Options_default_Audio_OFF_unchanged':True,'host_adapter_six_files_match_reviewed_test_only_patch':True,'real_production_helper_included_not_copied':True,'unchanged_2_byte_wchar_production_assert':True,'host_short_wchar_explicit_and_wide_boundary_adapters_present':True,'CI48_failure_and_zero_SIS_evidence_retained':True}
report={'status':'PASS_SOURCE_SCOPE_ONLY','utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source_commit':head,'prior_reviewed_source_commit':prior,'source_files':records,'current_changed_file_count':38,'current_production_source_edits':0,'host_test_source_files':tests,'checks':checks,'prior_independent_UTF16_checks':priorReview['checks'],'findings':[],'artifact_verification_pending':True,'CI49_image_execution_claimed':False,'phone_acceptance_claimed':False,'guest_or_server_started':False,'shared_source_modified':False}
(r/'independent-source-review.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'status':report['status'],'checks':len(checks),'prior_UTF16_checks':16,'current_production_edits':0}))
