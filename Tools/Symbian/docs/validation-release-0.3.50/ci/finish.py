#!/usr/bin/env python3
from pathlib import Path
import datetime,hashlib,json,shutil
r=Path(__file__).resolve().parent
load=lambda n:json.loads((r/n).read_text())
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
payload=load('sis-payload-verification.json');assert payload['status']=='PASS' and payload['payload_count']==13
assert payload['head_sha']=='886d227f1780298cc17cf99695a3c78c0b7d9ff6'
idna=load('icu-payload-idna.json');assert idna['result']=='PASS' and len(idna['checks'])==8 and all(e['result']=='PASS' for e in idna['checks'])
assert next(e for e in idna['checks'] if e['name']=='existing package')['data_sha256']=='808fc9bd37589371288b409a8433cf3434ba792ca9d640109c5da41454895d0d'
assert all(p['signature_chains']==0 for p in payload['packages'])
assert load('source-payload-verification.json')['status']=='PASS'
auth=load('auth-resource-verification.json');assert auth['status']=='PASS'
assert auth['resource']['bytes']==242 and auth['resource']['sha256']=='fb8e0b7154cb40458860c74bbb5334da613b34339cd47906a1cae0d6d7290ac8'
native=load('native-patch-stack-ci-verification.json');assert native['status']=='PASS' and native['patch_count']==9 and native['post_build_status']=='ALREADY_APPLIED'
assert native['post_build_from_prefix']==9 and native['native_E32_ctest_passed'] is True
network=load('source-network-contract-verification.json');assert network['source_commit']==payload['head_sha'] and all(network['checks'].values())
network_actual=load('network-artifact-source-build-verification.json');assert network_actual['status']=='PASS_ARTIFACT_SOURCE_BUILD_CONTRACT' and network_actual['source_commit']==payload['head_sha']
a=load('archive-verification.json');assert a['status']=='PASS' and a['workflow_conclusion']=='success' and a['job_id']==124825909684
run=load('run.json');assert run['id']==41299027063 and run['head_sha']==payload['head_sha'] and run['status']=='completed' and run['conclusion']=='success'
head=payload['head_sha']
classic=load('classic-source-verification.json');assert classic['source_commit']==head and classic['source_count']==13 and all(classic['checks'].values())
sdk_audio=load('SDK-audio-CI-verification.json');assert sdk_audio['status']=='PASS_SOURCE_GATED_ACTUAL_CI_PREREQUISITES' and sdk_audio['native_patch_count']==9 and sdk_audio['actual_patch_stack_host_tests']==18
imports=load('e32-imports-verification.json');assert imports['status']=='PASS' and imports['source_commit']==head and imports['method']=='parsed-e32-import-table' and 'mediaclientaudiostream.dll' in imports['canonical_imported_dlls']
summary={'status':'PASS','scope':'Actual downloaded artifact/SIS/source payload and host IDNA verification only; no guest installation/runtime',
 'completed_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'repo':'yeatse/WebKit-Symbian',
 'run_id':41299027063,'job_id':124825909684,'head_sha':head,'branch':'2.52.6-symbian',
 'sis_artifact':{'id':a['artifact_id'],'bytes':a['bytes'],'sha256':a['sha256'],'matches_github_digest':True},
 'workflow_status':'success','all_three_sis_sha256_match_artifact_manifest':True,'package_version':'0.3.50',
 'payload_count':13,'all_package_headers_sections_crc_valid':True,'all_decompressed_payload_sha1_valid':True,
 'all_payload_sizes_valid':True,'unsigned':True,'source_backed_payloads_exact_match':6,'pinned_ca_match':True,
 'icu_data_sha256':'808fc9bd37589371288b409a8433cf3434ba792ca9d640109c5da41454895d0d',
 'idna_host_groups':8,'verification_tools_pinned_to_source_commit':True,
 'auth_password_source_API_and_RSS_limit':255,'auth_resource':auth['resource'],
 'network_source_contract_checks':len(network['checks']),'network_runtime_verification_performed':False,'network_artifact_source_build_contract':network_actual,
 'native_patch_stack_CI_build_and_repeat_verified':True,'native_patch_stack_patches':9,
 'packages':[{'name':p['name'],'bytes':(r/'sis'/p['name']).stat().st_size,'sha256':sha(r/'sis'/p['name'])} for p in payload['packages']],
 'release_default_WEB_AUDIO':'ON','default_classic_WEB_AUDIO':'ON','classic_source_contract_checks':9,'classic_source_snapshots':13,'AudioWorklet_default_enabled':False,'SDK_math_complex_actual_CI_prerequisites':sdk_audio,'actual_browser_E32_imports':imports,'local_C28_results_reused_as_CI_runtime':False,
 'guest_installation_performed':False,'guest_runtime_performed':False,'physical_phone_verification_performed':False,
 'elf_artifact_downloaded':False,'local_browser_UI_results_reused':False}
(r/'verification.json').write_text(json.dumps(summary,indent=2)+'\n')
doc=r/'docs-candidate/validation-release-0.3.50/ci';doc.mkdir(parents=True,exist_ok=False)
for name in ['run.json','artifacts.json','jobs.json','archive-verification.json','sis-payload-verification.json',
 'package-verification.json','source-payload-verification.json','icu-payload-idna.json','icu-payload-idna.log',
 'verification-script-identities.json','prepared-source-identities.json','packaging-source-identity.json',
 'commands.json','acquisition.json','source-network-contract-verification.json','network-artifact-source-build-verification.json','network-CI-build-excerpt.log','check-network-artifact.py','verify-network.log','preparation-check.json','prepare.py','finish.py','verification.json','verify-payloads.py','verify-payloads.log','check-downloaded.py','check-source-payloads.py',
 'source-auth-limit-verification.json','check-auth-resource.py','auth-resource-verification.json','verify-auth.log',
 'check-native-patch-stack-ci.py','native-patch-stack-source-identities.json','native-patch-stack-ci-verification.json','native-patch-stack-ci.log','verify-native-stack.log']:
 shutil.copy2(r/name,doc/name)
for sub in ['tests/icu-idna/run.py','tests/icu-idna/probe.cpp','scripts/icu-filter.py','docs/validation-idna/package/verify-icu-sis.py']:
 src=r/'verification-tools/Tools/Symbian'/sub;dst=doc/'verification-tools'/sub;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
shutil.copytree(r/'source-inputs',doc/'source-inputs')
shutil.copytree(r/'network-source',doc/'network-source')
rows='\n'.join('| '+p['name']+' | '+format(p['bytes'],',')+' | `'+p['sha256']+'` |' for p in summary['packages'])
exe={Path(i['extracted_path']).name:i for p in payload['packages'] for i in p['payloads'] if i['target'].endswith('.exe')}
(doc/'README.md').write_text(f'''# CI50 实际安装包核验

通过 GitHub 插件跟进 [Run 41299027063](https://github.com/yeatse/WebKit-Symbian/actions/runs/41299027063)，并实际下载 [sis artifact {a['artifact_id']}](https://github.com/yeatse/WebKit-Symbian/actions/runs/41299027063/artifacts/{a['artifact_id']})。Run50 / job124825909684 均 completed/success，来源为 `2.52.6-symbian` 精确提交 `{head}`，push 触发。

实际 ZIP 为 **{a['bytes']:,} bytes**，SHA256 `{a['sha256']}`，与 GitHub artifact digest 匹配。四个 ZIP 成员及每个成员 CRC 均通过；只下载 sis，没有下载 ELF。

三个实际 unsigned SIS 版本均为 **0.3.50**，由 controller 内容读取核对。SHA256 均匹配 artifact 自带 SHA256SUMS。SIS UID/header CRC、controller/data section CRC、13个目标、全部解压长度和 payload SHA-1、两个 E32 UID3/能力字段及 VFPv2 标记通过。

| SIS | bytes | SHA256 |
| --- | ---: | --- |
{rows}

CI browser.exe：{exe['browser.exe']['bytes']:,} bytes，SHA256 `{exe['browser.exe']['sha256']}`。
CI jsc.exe：{exe['jsc.exe']['bytes']:,} bytes，SHA256 `{exe['jsc.exe']['sha256']}`。

四个 JS、test.html 和 fonts.conf 与精确来源提交的 git blob 逐字节相同。固定 release CA 为188,900 bytes，SHA256 `a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505`，与来源 package-sis.sh 中的固定值一致。实际解压 ICU 数据为10,261,824 bytes，SHA256 `808fc9bd37589371288b409a8433cf3434ba792ca9d640109c5da41454895d0d`。

使用来源提交导出的 run.py/probe.cpp/icu-filter.py，对**本次 SIS 实际解压数据**重新运行8组 host IDNA 检查，覆盖 existing package、三种过滤结果、缺失 UTS46 负对照及 stubdata 隔离。完整工具身份、命令和原始输出已归档；并非复制 CI41/42 报告作为本次通过证据。

本次 CI 的 E32 converter 阶段已完成9个 native patch 的真实构建和 native E32 ctest；构建后再次调用 helper，完整文件 postimage 校验返回 `ALREADY_APPLIED`、`from_prefix=9`。归档记录了实际阶段日志和9个 patch 的精确源 SHA256；CI44 失败报告继续独立保留，不因 CI50 成功删除。

实际 SIS 内 `browser_ui.rsc` 为242 bytes，SHA256 `fb8e0b7154cb40458860c74bbb5334da613b34339cd47906a1cae0d6d7290ac8`，与固定255资源的已知产物一致。精确来源提交的 Auth API 与 RSS 的11项静态检查及 SDK 公开枚举上限255也通过。此项仅核验资源与源契约，不代表本次包已运行密码输入框。

本次精确来源另外核对了{len(network['checks'])}项网络源码合同：nghttp2 1.68.0固定摘要、IPv6/threaded resolver/mbedTLS/H2开关、8/8/4连接池、8流、256KiB窗口、512KiB session allocator、WSS固定H1、两个CI缓存helper输入、native resolver三文件/selector最终CMake属性、16地址与10秒边界、完整IPv6地址/scope、curl自有结果分配以及三条链接路径的esock/insock。原始源码及git blob身份已归档。实际CI日志也确认nghttp2/curl各构建一次，产物libnghttp2.a为{network_actual['actual_CI_dependency_build_markers']['libnghttp2_a_bytes']}B、libcurl.a为{network_actual['actual_CI_dependency_build_markers']['libcurl_a_bytes']}B；实际SIS browser包含nghttp2/1.68.0/mbedTLS相关字节。本次未跳过的CI依赖构建受精确源码中的llvm-nm后置门槛约束：curl_addrinfo.c.o选中Curl_symbian_getaddrinfo且不再引用getaddrinfo/freeaddrinfo，archive内两个native bridge定义各唯一；版本2 stamp仅在门槛通过后写入。build-deps将整个curl子进程输出重定向到deps/curl.log，成功时console没有nm成功文字或原表；本次没有下载该重定向日志。归档保存真实curl构建标记/库产物行、未跳过且成功的CI步骤，以及独立set-e子进程内不可绕过的完整门槛源码。因此结论是源码门槛约束下的CI成功，不伪称直接审过CI nm输出，也不当作最终ELF符号审计。完整证据见network-artifact-source-build-verification.json；未取ELF、未审最终ELF符号或link map，源码单份依赖声明和字符串不被当作本包runtime证明。这是源码/构建契约核验，不能仅由代码或二进制字符串推断本包已经运行H2/IPv6/ALPN。

**本次没有安装或运行 CI50 镜像，也没有真机验收。** 本地C25/host0032或其他browser/host/FEP组合的DNS/H2/IPv6、UI、下载、触摸或剪贴板结果均不能移作CI50 runtime结果；本次包不含开发树CookieStore/Multi WIP，也不部署EKA0032。0029 密码遮罩另有独立 host/FEP 身份和 UI 证据，不在这里宣称本包已运行。

原始 ZIP、SIS 和13个解压 payload 留在 `/workspace/symbian-ci50-validation/`，不进入 Git。候选文档只保留元数据、报告、固定源输入、核验脚本及文本输出。`SHA256SUMS` 覆盖全部归档文件。本次核包阶段未改共享源码、workflow 或 guest；本候选核验文档尚未提交或 push。

复核 SIS：把 ZIP 四个成员放到 `ARTIFACT_DIR/sis/` 后，运行 `python3 verify-payloads.py ARTIFACT_DIR --tools-root verification-tools`。来源工具是固定 git blob 的副本；原始命令见 commands.json。离线源码比对另需保持 source-inputs 的相对目录结构，命令文件中的绝对路径记录的是本次真实执行位置。
''')
for name in ['classic-source-verification.json','prepare-classic-source.py','SDK-audio-CI-verification.json','SDK-math-CI-excerpt.log','SDK-complex-CI-excerpt.log','check-sdk-audio-CI.py','verify-SDK-audio-CI.log','e32-imports-verification.json','check-e32-imports.py','e32-imports-parser.stderr.log','verify-e32-imports.log']:
 shutil.copy2(r/name,doc/name)
shutil.copytree(r/'source',doc/'source')
shutil.copytree(r/'build-source',doc/'build-source')
parser_doc=doc/'e32-parser';parser_doc.mkdir()
for parser_name in ['inspect-general.cpp','build.py','build-identities.json','build.log','LICENSE.EKA2L1']:
 shutil.copy2(r/'e32-parser'/parser_name,parser_doc/parser_name)
p=doc/'README.md';text=p.read_text();anchor='**本次没有安装或运行 CI50 镜像，也没有真机验收。**'
assert anchor in text
paragraph='本次来源默认Classic WebAudio已ON，13个实际来源快照（12runtime文件+OptionsSymbian）均按精确提交git blob冻结；9项源合同确认classic启用、真实手势要求、AudioWorklet getter/构造器setting关闭、缺失MMF PCM的mediaElement source诚实NotSupported。实际CI中9patch/15文件的完整SDK栈、18项回归、allocator、math和complex步骤均成功；原始stdout明确math三profile INSTALLED在前，固定LLVM源的complex INSTALLED/296原成员检查在后。complex实际构建3profile和std::complex ARMcanary/非空符号/成员保持门槛由精确源helper无条件执行后才会安装打印成功，但远端逐成员原表/原nm输出未下载，不伪称直接审过这些原表。实际SIS browser另外由现有EKA普通E32 parser解析header/import/relocation，并独立验证UID checksum、实际header CRC和完整import bounds/code slots，导入表确含mediaclientaudiostream.dll；不是strings推断，也不是最终ELF符号审计。详见SDK-audio-CI-verification.json和e32-imports-verification.json。\n\n'
p.write_text(text.replace(anchor,paragraph+anchor))
for dirname in ['source-inputs','network-source','verification-tools','source','build-source']:
 sub=doc/dirname
 (sub/'SHA256SUMS').write_text(''.join(sha(p)+'  '+p.relative_to(sub).as_posix()+'\n' for p in sorted(sub.rglob('*')) if p.is_file() and p!=sub/'SHA256SUMS'))

(doc/'SHA256SUMS').write_text(''.join(sha(p)+'  '+p.relative_to(doc).as_posix()+'\n' for p in sorted(doc.rglob('*')) if p.is_file() and p!=doc/'SHA256SUMS'))
for line in (doc/'SHA256SUMS').read_text().splitlines():
 expected,name=line.split('  ',1);assert sha(doc/name)==expected
(r/'delivery.json').write_text(json.dumps({'status':'PASS','docs_candidate':str(doc),'artifact_dir':str(r),
 'report_sha256':sha(r/'verification.json'),'docs_sha256sums_sha256':sha(doc/'SHA256SUMS'),
 'guest_mutations':False,'shared_source_mutations':False,'elf_downloaded':False},indent=2)+'\n')
print(json.dumps({'status':'PASS','docs':str(doc),'summary':summary,'exe':exe},indent=2))
