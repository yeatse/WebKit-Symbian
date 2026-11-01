#!/usr/bin/env python3
"""Archive-only public-record/hash checks; never compile, launch or use IPC."""
import hashlib,importlib.util,json,re,sys
from pathlib import Path
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent
checks={}
def need(name,value):
 checks[name]=value is True
 if value is not True:raise ValueError(name)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def pairs(rows):
 value={}
 for key,item in rows:
  if key in value:raise ValueError('duplicate field')
  value[key]=item
 return value
def load(name):return json.loads((ROOT/name).read_text(),object_pairs_hook=pairs,
 parse_constant=lambda value:(_ for _ in ()).throw(ValueError('nonfinite')))
def main():
 files={str(p.relative_to(ROOT)):p for p in ROOT.rglob('*')if p.is_file()}
 allowed={'README.md','SHA256SUMS','scope.json','normal-api-run.json','native-public-record.txt',
 'identities.json','copy-provenance.json','validate.py','cow/cow_canary.cpp',
 'cow/posix-private-cow-readonly.patch','cow/file-api-results.json','verify_public_archive.py'}
 need('exact_public_file_whitelist',set(files)==allowed and not any(p.is_symlink()for p in ROOT.rglob('*')))
 entries={}
 for line in (ROOT/'SHA256SUMS').read_text().splitlines():
  match=re.fullmatch(r'([0-9a-f]{64})  (.+)',line)
  need('checksum_line_'+str(len(entries)),match is not None)
  need('checksum_unique_'+match[2],match[2]not in entries)
  entries[match[2]]=match[1]
 need('manifest_exact_file_set',set(entries)==allowed-{'SHA256SUMS'})
 for name,digest in entries.items():need('sha256:'+name,sha(files[name])==digest)
 fixed={'validate.py':'667fa28ff1ce68efcf131ba410b7497755b74bf2480361e77a36862a6b7d3eba',
 'cow/cow_canary.cpp':'71dfa86fcc6ddef51757493ab82b18bb28c1023940609d791decf28ecae69c9a',
 'cow/posix-private-cow-readonly.patch':'5f8c13d28a5de3cd928e3e7f309c698558dd8840316b8111703747258a9ba785'}
 for name,digest in fixed.items():need('frozen_public_source:'+name,sha(ROOT/name)==digest)
 copies=load('copy-provenance.json')['unchanged_public_copies']
 need('only_four_unchanged_public_copies',{row['archive']for row in copies}==set(fixed)|{'native-public-record.txt'} and len(copies)==4)
 for row in copies:need('copied:'+row['archive'],set(row)=={'archive','public_source','bytes','sha256'} and (ROOT/row['archive']).stat().st_size==row['bytes']and sha(ROOT/row['archive'])==row['sha256'])
 spec=importlib.util.spec_from_file_location('pinned_public_validator',ROOT/'validate.py')
 validator=importlib.util.module_from_spec(spec);spec.loader.exec_module(validator)
 run=load('normal-api-run.json')
 need('run_whitelist',set(run)=={'status','run','booleans','sandbox','snapshot','normal_completion_evidence'})
 b=run['booleans'];true_keys={'native_guest_launch_attempted','native_guest_started','drives_restored',
 'configuration_restored','host_stopped','native_record_present','original_ROM_namespace_readonly_verified',
 'original_ROM_bytes_unchanged','original_j2me_registry_bytes_unchanged','inputs_unchanged'}
 false_keys={'commdb_scope_known','routing_verified','forced_cleanup','originals_retained','lockkeeper_retained'}
 need('boolean_whitelist_and_types',set(b)==true_keys|false_keys and all(type(v)is bool for v in b.values()))
 need('normal_completion_and_restoration',run['status']=='NORMAL_API_SNAPSHOT_COMPLETE'and all(b[key]is True for key in true_keys)and all(b[key]is False for key in false_keys))
 need('strict_outer_gates',run['sandbox']=={'exit_code':0,'stopped':True,'forced_cleanup':False,'guard_ok':True,'audit_ok':True,'denied_mutations':0,'unsupported_async':0,'output_fatal':False})
 parsed=validator.validate((ROOT/'native-public-record.txt').read_text(),run['run'])
 need('strict_native_record_matches',parsed==run['snapshot'])
 need('actual_normal_API_AVAILABLE',parsed['capability']=='AVAILABLE'and parsed['native_status']==0 and parsed['native_api_statuses']=={'iap_rc':0,'network_rc':0,'service_rc':0,'start_rc':0}and parsed['stable_snapshot']is True and parsed['owned_handle_closed']is True and parsed['timed_out']is False)
 need('native_record_scope',parsed['routing_verified']is False and parsed['storage_scope_verified_by_record']is False and parsed['requires_outer_health_and_restore']is True)
 ident=load('identities.json')
 need('identity_field_whitelist',set(ident)=={'host0034','native_canary','source_harness','harness_source_hashes','readonly_backing_contract','COW_public_source_hashes','patch_sha256','production_TU_compile','canonical_COW_evidence','binary_bytes_copied'})
 need('actual_host0034_identity',ident['host0034']=={'bytes':65488744,'sha256':'b87e3961f7321bd0e8ce5cb3adc1ce608f624ecfbf3b75af434801addacfa773'})
 need('same_native_canary_identity',ident['native_canary']=={'bytes':84716,'sha256':'3f52a5ca2f5241bbb719f740cbbdefe68e2865fcc447675bfe900b2d917a5895','source_sha256':'3d52a60725fd9f71fa59640fcdd13a50abb3d20f6fd0115cfd727d68cd0bfdfe'})
 need('exact_harness_identities',ident['harness_source_hashes']=={'adapter.py':'875b3781dc50be2885ad43d1bd4b421a7f180d5020fc699c1638e638534592cd','os_sandbox.py':'6240ac0be26b6574392425e96224fd940c9d9592af6d9f7334a379f0ad3960f0','sandbox_guard.py':'01597566923823fda2c8b11c14aedf72c57df83aa308f0ebf5897d5f3d8f1d88','validate.py':fixed['validate.py'],'canary.cpp':ident['native_canary']['source_sha256']})
 need('readonly_contract_not_working_copy',ident['readonly_backing_contract']=={'explicit_RO_file_bind_declared':True,'working_ROM_copy_present':False,'writable_ROM_bind_present':False,'guard_checks_visible_kernel_readonly_flags':True})
 source_pins={'cow_canary.cpp':fixed['cow/cow_canary.cpp'],'before_virtualmem.cpp':'cd696d049d12923ba9a747c6fa0878957d47ac71aed91cd3b45d4b5290b8091e','after_virtualmem.cpp':'0e4371b2aa0ee18687147d5ddb6d52d8dcbfdfcb9d3759f51057c8d8e48bb02c','types.cpp':'4f5532674df71a1d9c78fcd88d3081c1557823a6c14b603112460e0c467a77b9','virtualmem.h':'ee5635b449061e43af53553724806e3eff2bdedab43c693d9a6c326fe5a9e85a'}
 need('public_COW_source_pins_only',ident['COW_public_source_hashes']==source_pins and ident['patch_sha256']==fixed['cow/posix-private-cow-readonly.patch'])
 c=ident['canonical_COW_evidence']
 need('canonical_18_file_evidence_reference',set(c)=={'public_directory','SHA256SUMS_sha256','entries','all_entries_verified_readonly_for_archive'}and c['entries']==18 and c['SHA256SUMS_sha256']=='a68666701fe93157683c16f09ede9a9709ae241b691d0e8bbc3d3d71b28e19f6'and c['all_entries_verified_readonly_for_archive']is True)
 compile=ident['production_TU_compile']
 need('historical_compile_source_binding',compile['status']=='COMPILE_PASS_NOT_EXECUTED'and compile['exit_code']==0 and compile['source_sha256']==source_pins['after_virtualmem.cpp']and compile['object_bytes']==4640 and compile['object_sha256']=='5d08027f24d5c638b5c2fdce2c00dac76bef2ec871c25e6b066f4d4b45069b46'and compile['host_or_guest_executed']is False)
 need('no_binary_bytes',ident['binary_bytes_copied']is False)
 cow=load('cow/file-api-results.json')
 need('file_API_field_whitelist',set(cow)=={'status','before','patched','baseline_regression_retained','other_open_flags_unchanged','original_task_bytes_mode_unchanged','actual_COW_file_operation','other_modes_are_synthetic_open_flag_checks','ROM_or_DB_accessed_by_public_file_canary'})
 common={'fixture_valid':True,'original_bytes_unchanged':True,'original_mode_unchanged':True,'read_flags_unchanged':True,'write_flags_unchanged':True,'other_rw_flags_unchanged':True,'unsupported_flags_unchanged':True}
 changed=['private_open_readonly','map_succeeded','cow_byte_changed','mapping_released']
 need('baseline_normal_failure_preserved',cow['before']=={'status':'FAIL',**common,**{key:False for key in changed}})
 need('patched_normal_file_API_pass',cow['patched']=={'status':'PASS',**common,**{key:True for key in changed}})
 need('file_API_scope',cow['status']=='PASS'and all(cow[key]is True for key in ['baseline_regression_retained','other_open_flags_unchanged','original_task_bytes_mode_unchanged','actual_COW_file_operation','other_modes_are_synthetic_open_flag_checks'])and cow['ROM_or_DB_accessed_by_public_file_canary']is False)
 scope=load('scope.json');false_scope=['Nameclone_verified','DNS_verified','Native_CommDB_verified','proxy_routing_verified','phone_verified','installer_verified','shared_mapping_or_all_mapping_modes_verified','archive_creation_repeated_runtime_or_file_canary']
 need('scope_field_whitelist',set(scope)=={'status','scope',*false_scope,'previous_archive','no_ROM_J2ME_values_contents_hashes_or_original_paths_archived'})
 need('no_scope_expansion',scope['status']=='HOST0034_READONLY_BACKING_AND_NORMAL_API_ADDENDUM'and all(scope[key]is False for key in false_scope)and scope['no_ROM_J2ME_values_contents_hashes_or_original_paths_archived']is True)
 need('previous_26_file_archive_reference',scope['previous_archive']=={'relative':'../service-host0033-control0032','files':26,'SHA256SUMS_sha256':'8b6acf2bb423ba9c4ecdc433ee1b37b3f6dfeac20326b97fd43555ebc2edea72','modified_by_this_addendum':False})
 return {'status':'PASS_PUBLIC_HOST0034_ADDENDUM','checks':len(checks),'all_checks_true':all(checks.values()),'files':len(files),'root_digest':sha(ROOT/'SHA256SUMS'),'native_or_file_canary_executed':False,'original_private_paths_accessed':False,'Native_CommDB_verified':False,'proxy_routing_verified':False}
if __name__=='__main__':
 try:result=main()
 except BaseException as error:result={'status':'FAIL_PUBLIC_HOST0034_ADDENDUM','failed_check':str(error),'checks':len(checks),'native_or_file_canary_executed':False}
 print(json.dumps(result,indent=2,sort_keys=True));raise SystemExit(0 if result['status']=='PASS_PUBLIC_HOST0034_ADDENDUM'else 1)
