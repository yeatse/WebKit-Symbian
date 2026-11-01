from pathlib import Path
import hashlib,json,re,subprocess
r=Path(__file__).resolve().parent;repo=Path('/workspace/WebKit-Symbian');load=lambda n:json.loads((r/n).read_text());sha=lambda data:hashlib.sha256(data).hexdigest()
source=load('source-network-contract-verification.json');run=load('run.json');head='0885f13ce4252a96b77d4c8cac0579ba2b8ba618';producer_head='81e1e14d260899c1d7e78e254c4cfa347640861a'
assert source['status']=='PASS_SOURCE_ONLY_ARTIFACT_PENDING' and all(source['checks'].values()) and run['head_sha']==head==source['source_commit'] and run['conclusion']=='success'
job=next(x for x in load('jobs.json')['jobs'] if x['id']==124809155850)
for name in ['Restore the WebCore libraries','WebCore','browser.exe','SIS packages']:
 step=next(s for s in job['steps'] if s['name']==name);assert step['status']=='completed' and step['conclusion']=='success'
assert next(s for s in job['steps'] if s['name']=='WebCore libraries')['conclusion']=='skipped'
raw=(r/'job.log').read_text();clean=re.sub(r'^\d{4}-\d\d-\d\dT[0-9:.]+Z ?', '',raw,flags=re.M)
keys=re.findall(r'^Cache restored from key: (deps-belle-[^\s]+)$',clean,re.M);assert len(keys)==1;key=keys[0]
assert clean.splitlines().count('Cache hit for: '+key)==1 and '== curl' not in clean and '== nghttp2' not in clean
wf=(r/'network-source/.github/workflows/symbian.yml').read_text();assert "if: steps.deps.outputs.cache-hit != 'true'" in wf
# Actual producer evidence is retained separately; do not imply its gate reran now.
producer=Path('/workspace/symbian-ci47-validation');prior=json.loads((producer/'network-artifact-source-build-verification.json').read_text());prior_run=json.loads((producer/'run.json').read_text());prior_raw=(producer/'job.log').read_text();prior_clean=re.sub(r'^\d{4}-\d\d-\d\dT[0-9:.]+Z ?', '',prior_raw,flags=re.M)
assert prior['status']=='PASS_ARTIFACT_SOURCE_BUILD_CONTRACT' and prior['source_commit']==producer_head and prior['run_id']==41287584566
assert prior_run['conclusion']=='success' and prior_run['head_sha']==producer_head and 'Cache saved with key: '+key in prior_clean
assert prior['actual_CI_native_nm_postcheck']['source_gated_CI_postcheck_completed'] and not prior['actual_CI_native_nm_postcheck']['direct_nm_output_audited']
inputs=['Tools/Symbian/scripts/build-deps.sh','Tools/Symbian/scripts/patch-curl-network.py','Tools/Symbian/scripts/patch-fontconfig-paths.py','Tools/Symbian/cmake/woff2','Tools/Symbian/shims','Tools/Symbian/scripts/common.sh','Tools/Symbian/toolchain','Tools/Symbian/tests/idb-quota/run-blob.sh','Tools/Symbian/tests/curl-address-family/run-host.sh']
changed=subprocess.check_output(['git','diff','--name-only',producer_head,head,'--',*inputs],cwd=repo,text=True);assert not changed
for item in source['files']:
 data=(r/'network-source'/item['source_path']).read_bytes();assert data==subprocess.check_output(['git','show',producer_head+':'+item['source_path']],cwd=repo)
p=r/'payloads/browser.exe';b=p.read_bytes();counts={s.decode():b.count(s) for s in [b'nghttp2',b'1.68.0',b'mbedTLS',b'HTTP/2']};assert counts['nghttp2']>0 and counts['1.68.0']==1 and counts['mbedTLS']>0
(r/'network-CI-build-excerpt.log').write_text('\n'.join(line for line in raw.splitlines() if key in line)+'\n')
(r/'network-cache-producer-excerpt.log').write_text('\n'.join(line for line in prior_raw.splitlines() if key in line)+'\n')
for name in ['network-artifact-source-build-verification.json','run.json']:
 (r/('network-cache-producer-'+name)).write_bytes((producer/name).read_bytes())
report={'status':'PASS_ARTIFACT_SOURCE_CACHE_CONTRACT','source_commit':head,'run_id':run['id'],'job_id':job['id'],'source_contract_checks':len(source['checks']),'actual_CI_build_steps_passed':['WebCore','browser.exe','SIS packages'],'actual_CI_dependency_cache':{'cache_key':key,'restore_step':'success','actual_cache_hit_and_restore_observed':True,'dependency_build_step':'skipped','nghttp2_builds_this_run':0,'curl_builds_this_run':0,'producer_run':41287584566,'producer_head':producer_head,'same_key_saved_by_observed_producer':True,'all_cache_key_source_inputs_unchanged_from_producer':True,'network_source_files_unchanged_from_producer':len(source['files']),'producer_report_sha256':sha((producer/'network-artifact-source-build-verification.json').read_bytes()),'cache_contents_independently_hashed_or_downloaded':False},'actual_CI_native_nm_postcheck':{'executed_this_run':False,'prior_source_gated_producer_postcheck_completed':True,'direct_nm_output_audited':False,'raw_nm_output_downloaded':False,'scope':'Dependencies restored under the exact key observed saved by successful CI47; source inputs unchanged. The fail-closed native selector/archive nm gate was source-gated in that producer, not executed or directly audited in this run. No final ELF symbol audit.'},'actual_browser_payload':{'bytes':len(b),'sha256':sha(b),'embedded_string_occurrences':counts},'CI_cache_excerpt_sha256':sha((r/'network-CI-build-excerpt.log').read_bytes()),'producer_cache_excerpt_sha256':sha((r/'network-cache-producer-excerpt.log').read_bytes()),'binary_strings_are_runtime_proof':False,'ELF_artifact_downloaded':False,'guest_or_phone_execution':False,'scope':'Actual SIS bytes, successful browser/WebCore/package steps, exact source contracts and observed cache-key provenance. Dependency contents/raw nm/final ELF were not independently fetched; no CI49 DNS/IPv6/H2/ALPN/WSS runtime inference.'}
(r/'network-artifact-source-build-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
