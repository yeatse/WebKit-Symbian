#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,re
r=Path(__file__).resolve().parent;source=json.loads((r/'source-network-contract-verification.json').read_text());assert source['status']=='PASS_SOURCE_ONLY_ARTIFACT_PENDING' and all(source['checks'].values())
run=json.loads((r/'run.json').read_text());assert run['head_sha']==source['source_commit'] and run['conclusion']=='success'
job=next(x for x in json.loads((r/'jobs.json').read_text())['jobs'] if x['id']==124777066796)
for name in ['WebCore libraries','WebCore','browser.exe','SIS packages']:
 step=next(s for s in job['steps'] if s['name']==name);assert step['status']=='completed' and step['conclusion']=='success'
raw=(r/'job.log').read_text();clean=re.sub(r'^\d{4}-\d\d-\d\dT[0-9:.]+Z ?', '',raw,flags=re.M)
assert clean.count('== nghttp2\n')==1 and clean.count('== curl\n')==1
lines=[line for line in clean.splitlines() if line in ['== nghttp2','== curl'] or re.search(r'\b(?:libnghttp2|libcurl)\.a$',line) or line.endswith('curl-symbian-network-version')]
assert any(re.search(r'\b352222 .*libnghttp2\.a$',line) for line in lines)
assert any(re.search(r'\b883660 .*libcurl\.a$',line) for line in lines)
(r/'network-CI-build-excerpt.log').write_text('\n'.join(lines)+'\n')
p=r/'payloads/browser.exe';b=p.read_bytes();counts={s.decode():b.count(s) for s in [b'nghttp2',b'1.68.0',b'mbedTLS',b'HTTP/2']};assert counts['nghttp2']>0 and counts['1.68.0']==1 and counts['mbedTLS']>0
report={'status':'PASS_ARTIFACT_SOURCE_BUILD_CONTRACT','source_commit':source['source_commit'],'run_id':run['id'],'job_id':job['id'],'source_contract_checks':len(source['checks']),'actual_CI_build_steps_passed':['WebCore libraries','WebCore','browser.exe','SIS packages'],'actual_CI_dependency_build_markers':{'nghttp2_builds':1,'curl_builds':1,'libnghttp2_a_bytes':352222,'libcurl_a_bytes':883660},'actual_browser_payload':{'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest(),'embedded_string_occurrences':counts},'CI_build_excerpt_sha256':hashlib.sha256((r/'network-CI-build-excerpt.log').read_bytes()).hexdigest(),'source_browser_link_contract':'Exact source CMake links one deps/install/lib/libnghttp2.a plus SDK mbedTLS; no final ELF symbol or link-map audit was performed','binary_strings_are_runtime_proof':False,'ELF_artifact_downloaded':False,'guest_or_phone_execution':False,'scope':'Actual SIS payload metadata and embedded bytes, actual successful CI build markers, exact source/link declarations; does not claim this CI image executed IPv6/H2/ALPN/WSS or inspect its final ELF symbols'}
(r/'network-artifact-source-build-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
