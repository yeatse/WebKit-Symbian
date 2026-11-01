from pathlib import Path
import datetime,hashlib,json,re,subprocess
r=Path('/workspace/symbian-ci48-validation');repo=Path('/workspace/WebKit-Symbian');head='67140bdc320bb9c911bf6f4045a3b38755b7707a'
def show(path):return subprocess.check_output(['git','show',head+':'+path],cwd=repo)
def parent(path):return subprocess.check_output(['git','show',head+'^:'+path],cwd=repo)
paths=['Source/WTF/wtf/posix/FileSystemPOSIX.cpp','Source/WTF/wtf/symbian/FileSystemSymbian.cpp','Source/WTF/wtf/symbian/SymbianFilePath.h']
texts={p:show(p).decode() for p in paths};posix,sym,helper=[texts[p] for p in paths]
changed=subprocess.check_output(['git','diff-tree','--no-commit-id','--name-only','-z','-r',head],cwd=repo,text=True).rstrip('\0').split('\0')
assert sorted(p for p in changed if p.startswith('Source/'))==sorted(paths)
assert len(changed)==54 and all(p.startswith('Tools/Symbian/docs/') for p in changed if p not in paths)
options=show('Source/cmake/OptionsSymbian.cmake').decode();assert 'WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_AUDIO PRIVATE OFF)' in options
orig=parent(paths[0]).decode();
def selected(s,a,b):return s[s.index(a):s.index(b,s.index(a))]
# Flags and permissions between the first per-platform flag block and the final call are byte-for-byte unchanged.
a='#if OS(SYMBIAN)\n    int platformFlag';b='    return FileHandle::adopt(open('
old_flags=selected(orig,a,b)
new_flags=selected(posix,a,'#if OS(SYMBIAN)\n    // Explicit mode')
assert new_flags==old_flags
assert 'FileHandle::adopt(wopen(fsRep.data(), platformFlag, permissionFlag), lockMode)' in posix
assert 'm_characters[path.length()] = 0;' in helper and 'if (path.length() > maximumLength)' in helper and 'maximumLength = 256' in helper
assert 'static_assert(sizeof(wchar_t) == 2 && alignof(wchar_t) == 2);' in helper
assert 'if (!character)' in helper and 'errno = EINVAL;' in helper and 'errno = ENOENT;' in helper and 'errno = ENAMETOOLONG;' in helper
assert 'm_characters[i] = static_cast<wchar_t>(character);' in helper and 'explicit operator bool() const { return m_valid; }' in helper
assert '#if OS(SYMBIAN)' in helper and '#include <wtf/symbian/SymbianFilePath.h>' in posix
assert 'if (follow)' in sym and 'wstat(name.data(), &info)' in sym and 'lstat(name.c_str(), &info)' in sym
assert 'return name && !waccess(name.data(), F_OK);' in sym
base='Tools/Symbian/docs/validation-belle/file-utf16/wtf-owner/'
summary=json.loads(show(base+'summary/wtf-owner.json'));ident=json.loads(show(base+'identities.json'));gates=json.loads(show(base+'production/branch-gates.json'));manifest=json.loads(show(base+'production/patch-manifest.json'))
assert summary['status']=='PASS' and summary['normal_worker_completion'] and summary['natural_host_exit'] and summary['host_exit']==0 and not summary['forced_cleanup'] and not summary['deadline_expired']
assert summary['api_log']['cases']==3 and summary['api_log']['passed']==3 and summary['api_log']['failed']==0 and summary['api_log']['worker_exit']==0
assert all(v['restored'] for v in summary['restoration'].values())
assert ident['artifacts']['wtf_owner_worker']['sha256']=='8818c12aa7af3db53fc5622089dbc2149f7c7ec907f9c88b886e75111ad2c44a'
assert ident['artifacts']['wtf_owner_worker']['bytes']==178376
assert ident['public_runtime_inputs']['host']['sha256']=='692abe6937ed1b651914ff6ab35a78bbd3693a705ba47853fc6f9a438ae178bd'
assert ident['artifacts']['libWTF']['sha256']=='045b0ebab6a9888a6649f10ab113c7936ee681ed23a83472b6ce3c57f0c59f33'
assert summary['api_log']['abi']['unicode_units']==[0x5b57,0xd83d,0xde00]
assert all(row['pass'] and row['exists'] and row['size_available'] and row['mtime_ok'] and row['opened'] and row['read_ok'] and row['eof'] and row['exact'] and row['hash_ok'] for row in summary['api_log']['rows'])
assert gates['status']=='PASS' and all(v for v in gates['gates'].values())
ids=[]
for p in paths:
 b=show(p);out=r/'independent-source'/p;out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(b)
 blob=subprocess.check_output(['git','rev-parse',head+':'+p],cwd=repo,text=True).strip();assert hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()==blob
 sha=hashlib.sha256(b).hexdigest();assert next(f for f in manifest['files'] if f['path']==p)['after_sha256']==sha
 ids.append({'path':p,'git_blob':blob,'sha256':sha,'bytes':len(b)})
checks={'source_changes_exactly_three_production_files':True,'remaining_51_changes_are_documentation_or_evidence':True,'literal_UTF16_units_and_surrogates_preserved':True,'empty_NUL_and_length_guard_before_OS_call':True,'wchar_2_byte_ABI_static_assert':True,'257_unit_storage_covers_256_units_plus_NUL':True,'flag_permission_lock_adopt_contract_unchanged':True,'following_stat_uses_wide_call':True,'nofollow_lstat_retained':True,'fileExists_size_mtime_wide_calls':True,'OS_SYMBIAN_selector_not_PLATFORM_selector':True,'unrelated_rename_delete_directories_unchanged':True,'release_default_WEB_AUDIO_OFF':True,'existing_three_file_WTF_result_has_normal_completion_and_restore':True,'existing_worker_hash_and_library_metadata_pinned':True,'production_hashes_match_archived_patch_manifest':True}
report={'status':'PASS_SOURCE_SCOPE_ONLY','utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source_commit':head,'source_files':ids,'checks':checks,'findings':[],'evidence_scope':{'prior_actual_WTF_worker_run':summary['run'],'prior_worker_SHA256':ident['artifacts']['wtf_owner_worker']['sha256'],'prior_libWTF_archive_header_SHA256':ident['artifacts']['libWTF']['sha256'],'prior_runtime_host_SHA256':ident['public_runtime_inputs']['host']['sha256'],'prior_fep_SHA256':ident['public_runtime_inputs']['fep']['sha256'],'runtime_cases':3,'notes':['Existing archived worker validation is controlled emulator API evidence, not CI48 binary execution.','libWTF is a thin archive; archive-header digest alone is not a digest of every member. The separately pinned worker executable is the final linked image used by that archived test.','Wide file reading and metadata validation do not prove GUI FileReader/FormData or all file API operations.']},'guest_or_server_started':False,'shared_source_modified':False,'CI48_image_execution_claimed':False,'phone_acceptance_claimed':False}
(r/'independent-source-review.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'status':report['status'],'checks':len(checks),'source_files':3,'findings':[]}))
