from pathlib import Path
import hashlib,json,re
r=Path(__file__).resolve().parent;load=lambda n:json.loads((r/n).read_text());head='886d227f1780298cc17cf99695a3c78c0b7d9ff6';job=next(j for j in load('jobs.json')['jobs'] if j['id']==124825909684)
required=['E32 converter','Rebuild and verify the 32-bit SDK allocator','Rebuild and verify the SDK math member','Rebuild and verify the SDK complex members']
for name in required:
 step=next(s for s in job['steps'] if s['name']==name);assert step['status']=='completed' and step['conclusion']=='success'
raw=(r/'job.log').read_text();clean=re.sub(r'^\d{4}-\d\d-\d\dT[0-9:.]+Z ?', '',raw,flags=re.M);decoder=json.JSONDecoder();records=[]
for m in re.finditer(r'\{\s*"status":\s*"INSTALLED"',clean):
 try:obj,length=decoder.raw_decode(clean[m.start():])
 except json.JSONDecodeError:continue
 records.append((m.start(),m.start()+length,obj))
math=[row for row in records if row[2].get('profiles')==['default','streams','native_atomic64'] and row[2].get('installed') is True]
complexes=[row for row in records if row[2].get('upstream_revision')=='85ac560262434c9ccfc0c183ec22d4138ed647fb' and row[2].get('installed') is True]
assert len(math)==1 and len(complexes)==1 and math[0][0]<complexes[0][0]
manifest=json.loads((r/'build-source/Tools/Symbian/patches/sdk-0.3.0/complex/manifest.json').read_text());complex_summary=complexes[0][2];assert complex_summary['source_files']==manifest['upstream_files'] and complex_summary['original_members_unchanged']==296
assert re.search(r'Ran 18 tests in [0-9.]+s',clean)
patch=load('native-patch-stack-ci-verification.json');assert patch['patch_count']==9 and patch['verified_files']==15 and patch['post_build_from_prefix']==9
for name,row in [('math',math[0]),('complex',complexes[0])]:
 (r/f'SDK-{name}-CI-excerpt.log').write_text(clean[row[0]:row[1]]+'\n')
report={'status':'PASS_SOURCE_GATED_ACTUAL_CI_PREREQUISITES','source_commit':head,'run_id':41299027063,'job_id':124825909684,'actual_steps_success':required,'native_patch_count':9,'native_owned_files':15,'actual_patch_stack_host_tests':18,'actual_math_install_summary':math[0][2],'actual_complex_install_summary':complex_summary,'math_before_complex_observed':True,'exact_builder_source_gate_scope':{'math_profiles':3,'complex_profiles':3,'complex_actual_ARM_canary_link_gate_completed':True,'complex_complete_positive_definition_gate_completed':True,'original_complex_input_members_checked_unchanged':296,'complex_per_member_hash_tables_downloaded':False,'complex_raw_nm_output_downloaded_or_directly_audited':False,'actual_evidence':'Successful unskipped step and exact printed INSTALLED/source SHA summary; committed helper source unconditionally checks members, definitions and links all three actual staged archives before install/print.'},'scope':'Actual CI steps and stdout summaries, bounded by exact committed helper control flow. No downloaded SDK archive/raw nm table or final ELF symbol audit; no guest/phone result.'}
(r/'SDK-audio-CI-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
