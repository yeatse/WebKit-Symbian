#!/usr/bin/env python3
"""Read/hash archived public evidence only. Never launch a runtime or network."""
import hashlib, importlib.util, json, re, sys
from pathlib import Path
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parent
CHECKS = {}

def check(name, value):
    CHECKS[name] = value is True
    if value is not True: raise ValueError(name)

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def pairs(rows):
    value = {}
    for key, item in rows:
        if key in value: raise ValueError('duplicate field')
        value[key] = item
    return value
def load(name):
    return json.loads((ROOT/name).read_text(), object_pairs_hook=pairs,
                      parse_constant=lambda value: (_ for _ in ()).throw(ValueError('nonfinite')))

def main():
    files = {str(p.relative_to(ROOT)):p for p in ROOT.rglob('*') if p.is_file()}
    check('no_symlinks', not any(p.is_symlink() for p in ROOT.rglob('*')))
    check('no_binary_or_private_files', not any(
        p.suffix.lower() in {'.exe','.elf','.o','.dll','.map','.sis','.zip','.raw','.log','.pyc'}
        or p.name in {'result.json','pre-run-inputs.json','devices.yml','config.yml'}
        or '__pycache__' in p.parts for p in files.values()))
    entries = {}
    for line in (ROOT/'SHA256SUMS').read_text().splitlines():
        match = re.fullmatch(r'([0-9a-f]{64})  (.+)', line)
        check('manifest_line_'+str(len(entries)), match is not None)
        name = match[2]
        check('manifest_path_'+name, not Path(name).is_absolute() and '..' not in Path(name).parts and name not in entries)
        entries[name] = match[1]
    check('manifest_exact_file_set', set(entries)==set(files)-{'SHA256SUMS'})
    for name, digest in entries.items(): check('sha256:'+name, sha(files[name])==digest)

    for item in load('copy-provenance.json')['unchanged_public_copies']:
        p = ROOT/item['archive']
        check('public_copy:'+item['archive'], p.stat().st_size==item['bytes'] and sha(p)==item['sha256'])
    expected = {
        'harness/adapter.py':'c57e68ff0b8eea527ec65eeaaf4629491c46ed34524a84d8133d87e07f9bcd59',
        'harness/control-host0032-adapter.py':'a6c784b2ad725e32b17e4de190d52ae2cc9e78b5e2248c85ee0f5bbe032ae876',
        'harness/os_sandbox.py':'6240ac0be26b6574392425e96224fd940c9d9592af6d9f7334a379f0ad3960f0',
        'harness/sandbox_guard.py':'01597566923823fda2c8b11c14aedf72c57df83aa308f0ebf5897d5f3d8f1d88',
        'harness/validate.py':'667fa28ff1ce68efcf131ba410b7497755b74bf2480361e77a36862a6b7d3eba',
        'harness/canary.cpp':'3d52a60725fd9f71fa59640fcdd13a50abb3d20f6fd0115cfd727d68cd0bfdfe',
        'harness/helpers/preservation.py':'ff27d4f3e25d2014cc3a21d8133df532966dc9c202bb5aa35eb26a9bcf40d80c',
        'harness/helpers/configuration.py':'e6679c12d66fadb4ad6c69f46f0deede57470f379d5f64591d1e6c312b8015d0',
        'getter/host0032-iap-service.patch':'5ba74dae0161e13d5f4f1e288029999f88a1b815873a9bf497d0654238cedd84',
        'getter/before-connection.cpp':'6741abfde862e518c8c62460aecfd0f4859150a2868fba33154e1a83cb7467d3',
        'getter/after-connection.cpp':'9730695ad8f3936d7d6dea3abd811f1ee156a65bad44376ecf19d2c5f41f9418',
        'peer-review.md':'7655210f6d2eed712bd1159bf92b5c568b710a686650c8188c2512670bc4eab0'}
    for name,digest in expected.items(): check('frozen_source:'+name, sha(ROOT/name)==digest)
    spec = importlib.util.spec_from_file_location('archived_public_validator', ROOT/'harness/validate.py')
    validator = importlib.util.module_from_spec(spec); spec.loader.exec_module(validator)
    runs = {}
    bool_keys = {'native_guest_launch_attempted','native_guest_started','commdb_scope_known','routing_verified',
        'drives_restored','configuration_restored','host_stopped','forced_cleanup','originals_retained',
        'lockkeeper_retained','native_record_present','original_j2me_registry_bytes_unchanged',
        'original_ROM_bytes_unchanged','inputs_unchanged'}
    for role,capability,service,stable in [('host0032','UNAVAILABLE',-1,False),('host0033','AVAILABLE',0,True)]:
        row=load('runs/'+role+'.json'); runs[role]=row
        check(role+':whitelist', set(row)=={'host','status','run','booleans','sandbox','snapshot','normal_completion_evidence'} and set(row['booleans'])==bool_keys)
        check(role+':identity', row['host']==role and row['status']=='NORMAL_API_SNAPSHOT_COMPLETE')
        b=row['booleans']
        check(role+':boolean_types', all(type(v)is bool for v in b.values()))
        check(role+':completion_and_restore', all(b[k] for k in ['native_guest_launch_attempted','native_guest_started','drives_restored','configuration_restored','host_stopped','native_record_present','original_j2me_registry_bytes_unchanged','original_ROM_bytes_unchanged','inputs_unchanged']))
        check(role+':no_overclaim_or_force', all(b[k] is False for k in ['commdb_scope_known','routing_verified','forced_cleanup','originals_retained','lockkeeper_retained']))
        check(role+':sandbox', row['sandbox']=={'exit_code':0,'stopped':True,'forced_cleanup':False,'guard_ok':True,'audit_ok':True,'denied_mutations':0,'unsupported_async':0,'output_fatal':False})
        parsed=validator.validate((ROOT/'records'/f'{role}.txt').read_text(),row['run'])
        check(role+':strict_record_matches', parsed==row['snapshot'])
        check(role+':API_result', parsed['capability']==capability and parsed['native_api_statuses']=={'iap_rc':0,'network_rc':0,'service_rc':service,'start_rc':0} and parsed['stable_snapshot'] is stable and parsed['owned_handle_closed']is True and parsed['timed_out']is False)
        check(role+':scope', parsed['routing_verified']is False and parsed['storage_scope_verified_by_record']is False and parsed['requires_outer_health_and_restore']is True)
    check('fresh_distinct_runs', runs['host0032']['run']!=runs['host0033']['run'])
    ident=load('identities.json')
    check('identity_field_whitelist', set(ident)=={'hosts','canary','public_asset_pins','binary_bytes_copied','root_attestations'} and all(set(row)=={'bytes','sha256','run_image'} for row in ident['hosts'].values()) and set(ident['canary'])=={'source_sha256','exe_bytes','exe_sha256','uid3_requested','capabilities_requested','elf_needed_libraries'})
    check('explicit_host_identities', {key:(value['bytes'],value['sha256']) for key,value in ident['hosts'].items()}=={
        'host0032':(65484680,'692abe6937ed1b651914ff6ab35a78bbd3693a705ba47853fc6f9a438ae178bd'),
        'host0033':(65488776,'a3237ab5e5864d8dbebcf440c3e6c9026c264e052f336d62266439ce2261ba73')})
    check('same_canary_identity', ident['canary']['exe_bytes']==84716 and ident['canary']['exe_sha256']=='3f52a5ca2f5241bbb719f740cbbdefe68e2865fcc447675bfe900b2d917a5895' and ident['canary']['source_sha256']==expected['harness/canary.cpp'] and ident['binary_bytes_copied']is False)
    check('public_assets_only', len(ident['public_asset_pins'])==15 and all(set(row)=={'path','bytes','sha256'} and re.fullmatch('[0-9a-f]{64}',row['sha256']) for row in ident['public_asset_pins']))
    public_names=['mediaclientaudio_general.dll','mediaclientvideo_v100.dll','mediaclientaudio.dll.map','mediaclientvideo.dll.map','avkonfep_general.dll','mediaclientaudiostream_general.dll','mediaclientaudiostream.dll.map']
    public_paths={prefix+'/'+name for prefix in ['/workspace/eka-validation/xdg/data/EKA2L1/patch','/workspace/EKA2L1-validation/build/bin/patch'] for name in public_names}|{'/workspace/EKA2L1-validation/build/bin/eka2l1_qt'}
    check('no_ROM_J2ME_or_private_pin_roles', {row['path'] for row in ident['public_asset_pins']}==public_paths)
    check('Root_postverification_attestation_scoped', set(ident['root_attestations'])=={'all_15_public_asset_pins_postverified_unchanged','source_WIP_files_postverified_unchanged','basis'} and ident['root_attestations']['all_15_public_asset_pins_postverified_unchanged']is True and ident['root_attestations']['source_WIP_files_postverified_unchanged']==72)
    before=(ROOT/'harness/adapter.py').read_text().replace('/workspace/symbian-iap-host0033-relink-work/eka2l1_qt','/workspace/symbian-iap-root-host32-control-20261101/eka2l1_qt').replace('a3237ab5e5864d8dbebcf440c3e6c9026c264e052f336d62266439ce2261ba73','692abe6937ed1b651914ff6ab35a78bbd3693a705ba47853fc6f9a438ae178bd')
    check('control_only_host_and_pin_change', before==(ROOT/'harness/control-host0032-adapter.py').read_text())
    manifest=load('control-manifest.json')
    check('control_manifest', manifest['adapter_sha256']==expected['harness/control-host0032-adapter.py'] and manifest['adapter_difference_only_HOST_and_HOST_SHA']is True and set(manifest['common_source_hashes'])=={'os_sandbox.py','sandbox_guard.py','validate.py','canary.cpp'} and all(expected['harness/'+name]==digest for name,digest in manifest['common_source_hashes'].items()))
    getter=load('getter/evidence.json')
    check('getter_source_binding', getter['before_sha256']==expected['getter/before-connection.cpp'] and getter['after_sha256']==expected['getter/after-connection.cpp'] and getter['patch_sha256']==expected['getter/host0032-iap-service.patch'])
    check('getter_build_and_runtime_distinguished', getter['host_adapter']['status']=='HOST_ADAPTER_PASS_NOT_NATIVE' and getter['host_adapter']['assertions']==37 and getter['host0033_relink']['patched_member_sha256']==getter['host_compile']['object_sha256'] and getter['native_CommDB_or_proxy_runtime_proof']is False)
    failures=load('prior-failures.json')
    check('all_prior_failures_preserved', [row['run_number']for row in failures['runs']]==['01','02','03','04','05','06','08','09','10','11','12','13'] and all(row['status']=='FAIL' and row['reclassified_as_pass']is False for row in failures['runs']))
    check('failure_fields_finite', all(set(row)<= {'run_number','public_output_directory','status','booleans','reclassified_as_pass','error_code','separate_strict_native_record_available','separate_native_record_does_not_override_outer_fail','diagnostic_public_literal_candidates','candidates_prove_call_stack'} and all(type(v)is bool for v in row['booleans'].values()) for row in failures['runs']))
    check('failure_restore_and_health_bools', all(all(row['booleans'][key]is True for key in ['host_stopped','drives_restored','configuration_restored','inputs_unchanged']) and all(row['booleans'][key]is False for key in ['forced_cleanup','originals_retained','lockkeeper_retained']) and all(row['booleans'][key]is True for key in ['original_ROM_bytes_unchanged','original_j2me_registry_bytes_unchanged'] if key in row['booleans']) for row in failures['runs']))
    check('Root08_native_does_not_override_FAIL', failures['runs'][6]['separate_strict_native_record_available']is True and failures['runs'][6]['separate_native_record_does_not_override_outer_fail']is True)
    check('diagnostic_not_stack', failures['runs'][-1]['candidates_prove_call_stack']is False)
    summary=load('summary.json')
    check('summary_field_whitelist', set(summary)=={'status','scope','before','after','native_CommDB_verified','proxy_routing_verified','name_clone_verified','DNS_verified','phone_verified','installer_verified','archive_creation_executed_native_or_guest','no_private_contents_or_ROM_J2ME_hashes_archived'})
    check('scope_explicit', summary['status']=='SANITIZED_NORMAL_API_BEFORE_AFTER' and all(summary[key]is False for key in ['native_CommDB_verified','proxy_routing_verified','name_clone_verified','DNS_verified','phone_verified','installer_verified','archive_creation_executed_native_or_guest']) and summary['no_private_contents_or_ROM_J2ME_hashes_archived']is True)
    check('before_not_whole_failure', summary['before']=={'host':'host0032','whole_run_status':'NORMAL_API_SNAPSHOT_COMPLETE','capability':'UNAVAILABLE','service_rc':-1})
    check('after_scoped_available', summary['after']=={'host':'host0033','whole_run_status':'NORMAL_API_SNAPSHOT_COMPLETE','capability':'AVAILABLE','service_rc':0})
    return {'status':'PASS_SANITIZED_PUBLIC_ARCHIVE','checks':len(CHECKS),'all_checks_true':all(CHECKS.values()),
        'files':len(files),'root_digest':sha(ROOT/'SHA256SUMS'),'native_execution_performed':False,
        'original_private_paths_accessed':False,'routing_verified':False,'native_CommDB_verified':False}

if __name__=='__main__':
    try: result=main()
    except BaseException as error:
        result={'status':'FAIL_SANITIZED_PUBLIC_ARCHIVE','failed_check':str(error),'checks':len(CHECKS),'native_execution_performed':False}
    print(json.dumps(result,indent=2,sort_keys=True))
    raise SystemExit(0 if result['status']=='PASS_SANITIZED_PUBLIC_ARCHIVE' else 1)
