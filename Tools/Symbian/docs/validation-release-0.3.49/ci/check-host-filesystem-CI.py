from pathlib import Path
import hashlib,json,re
r=Path(__file__).resolve().parent
head='0885f13ce4252a96b77d4c8cac0579ba2b8ba618'
load=lambda name:json.loads((r/name).read_text())
job=next(j for j in load('jobs.json')['jobs'] if j['id']==124809155850)
step=next(s for s in job['steps'] if s['name']=='Belle storage, native UI and callback regressions')
assert step['status']=='completed' and step['conclusion']=='success'
review=load('independent-source-review.json');assert review['source_commit']==head and all(review['checks'].values())
source=(r/'host-test-source/Tools/Symbian/tests/native-filesystem/filesystem.cpp').read_text()
expected=re.findall(r'std::puts\("(PASS [^"]+)"\)',source);assert len(expected)==9
raw=(r/'job.log').read_text();clean=re.sub(r'^\d{4}-\d\d-\d\dT[0-9:.]+Z ?', '',raw,flags=re.M)
for marker in expected:assert clean.splitlines().count(marker)==1,marker
excerpt='\n'.join(line for line in raw.splitlines() if any(line.endswith(marker) for marker in expected))+'\n'
(r/'host-filesystem-CI-excerpt.log').write_text(excerpt)
report={'status':'PASS_HOST_CI_CONTROL_FLOW','source_commit':head,'run_id':41293400197,'job_id':124809155850,'step':'Belle storage, native UI and callback regressions','step_conclusion':'success','actual_native_filesystem_PASS_groups':9,'actual_markers':expected,'excerpt_sha256':hashlib.sha256(excerpt.encode()).hexdigest(),'production_Source_files_changed_this_commit':0,'host_adapter_source_files':6,'real_UTF16_helper_unchanged_and_included':True,'host_2byte_wchar_explicit':True,'host_independent_iconv_and_real_stat_access':True,'ARM_ABI_or_OpenC_proven':False,'CI49_guest_or_phone_execution':False,'scope':'Actual host CI suite output and successful unskipped combined step. Includes all original host filesystem assertions and new UTF-16 metadata/control-flow guards; does not execute target Open C or CI49 browser.'}
(r/'host-filesystem-CI-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
