#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,re
r=Path(__file__).resolve().parent;source=json.loads((r/'source-network-contract-verification.json').read_text());assert source['status']=='PASS_SOURCE_ONLY_ARTIFACT_PENDING' and all(source['checks'].values())
run=json.loads((r/'run.json').read_text());assert run['head_sha']==source['source_commit'] and run['conclusion']=='success'
job=next(x for x in json.loads((r/'jobs.json').read_text())['jobs'] if x['id']==124791920159)
for name in ['WebCore libraries','WebCore','browser.exe','SIS packages']:
 step=next(s for s in job['steps'] if s['name']==name);assert step['status']=='completed' and step['conclusion']=='success'
raw=(r/'job.log').read_text();clean=re.sub(r'^\d{4}-\d\d-\d\dT[0-9:.]+Z ?', '',raw,flags=re.M)
assert clean.count('== nghttp2\n')==1 and clean.count('== curl\n')==1
marker='Actual curl native resolver selector and archive definitions verified'
marker_count=clean.splitlines().count(marker)
# curl's complete --one subprocess is redirected; successful step completion,
# exact source control flow and the real curl build marker jointly establish
# that the fail-closed nm gate ran. We have not opened its redirected nm log.
build=(r/'network-source/Tools/Symbian/scripts/build-deps.sh').read_text()
assert 'if [ \"${1:-}\" = --one ]; then\n  set -e\n  \"build_$2\"\n  exit' in build
assert '\"$0\" --one \"$name\" > \"$DEPS_DIR/$name.log\" 2>&1 || { tail -30 \"$DEPS_DIR/$name.log\"; exit 1; }' in build
assert build.index("print('Actual curl native resolver selector and archive definitions verified')") < build.index("printf '2\\n'")

lines=[line for line in clean.splitlines() if line in ['== nghttp2','== curl',marker] or re.search(r'\b(?:libnghttp2|libcurl)\.a$',line) or line.endswith('curl-symbian-network-version')]
def archive_size(name):
 matches=[re.search(r'\s(\d+)\s+\w+\s+\d+\s+[0-9:]+\s+'+re.escape(name)+r'$',line) for line in lines]
 sizes=[int(m[1]) for m in matches if m];assert len(sizes)==1 and sizes[0]>0;return sizes[0]
nghttp=archive_size('libnghttp2.a');curl=archive_size('libcurl.a')
(r/'network-CI-build-excerpt.log').write_text('\n'.join(lines)+'\n')
p=r/'payloads/browser.exe';b=p.read_bytes();counts={s.decode():b.count(s) for s in [b'nghttp2',b'1.68.0',b'mbedTLS',b'HTTP/2']};assert counts['nghttp2']>0 and counts['1.68.0']==1 and counts['mbedTLS']>0
report={'status':'PASS_ARTIFACT_SOURCE_BUILD_CONTRACT','source_commit':source['source_commit'],'run_id':run['id'],'job_id':job['id'],'source_contract_checks':len(source['checks']),'actual_CI_build_steps_passed':['WebCore libraries','WebCore','browser.exe','SIS packages'],'actual_CI_dependency_build_markers':{'nghttp2_builds':1,'curl_builds':1,'libnghttp2_a_bytes':nghttp,'libcurl_a_bytes':curl},'actual_CI_native_nm_postcheck':{'console_marker_count':marker_count,'stdout_redirected_to':'DEPS_DIR/curl.log','redirected_log_downloaded':False,'actual_execution_evidence':'Exact source --one curl subprocess uses set-e, is not skipped, cannot return success before the unconditional llvm-nm assertions and version2 stamp; actual CI curl build marker, archive output and successful unskipped dependency step observed','source_assertions':'curl_addrinfo.c.o undefined Curl_symbian_getaddrinfo and no undefined getaddrinfo/freeaddrinfo; exactly one archive definition each of Curl_symbian_getaddrinfo and symbian_curl_resolve_records','source_gated_CI_postcheck_completed':True,'direct_nm_output_audited':False,'version_stamp_written_after_gate':2,'raw_nm_table_printed':False,'scope':'Source-gated successful CI post-check; redirected raw nm output/success text not collected, no final ELF symbol audit'},'actual_browser_payload':{'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest(),'embedded_string_occurrences':counts},'CI_build_excerpt_sha256':hashlib.sha256((r/'network-CI-build-excerpt.log').read_bytes()).hexdigest(),'source_browser_link_contract':'Exact source CMake links one deps/install/lib/libnghttp2.a, native esock/insock proxies and SDK mbedTLS; no final ELF symbol or link-map audit was performed','binary_strings_are_runtime_proof':False,'ELF_artifact_downloaded':False,'guest_or_phone_execution':False,'scope':'Actual SIS payload metadata and embedded bytes, source-pinned successful CI dependency build with unconditional fail-closed object/archive nm gate (redirected raw output not collected), exact source/link declarations; does not claim this CI image executed native DNS/IPv6/H2/ALPN/WSS or inspect its final ELF symbols'}
(r/'network-artifact-source-build-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
