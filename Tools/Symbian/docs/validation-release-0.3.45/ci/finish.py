#!/usr/bin/env python3
from pathlib import Path
import datetime,hashlib,json,shutil
r=Path(__file__).resolve().parent
load=lambda n:json.loads((r/n).read_text())
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
payload=load('sis-payload-verification.json');assert payload['status']=='PASS' and payload['payload_count']==13
assert payload['head_sha']=='e0e2f321d4e2d4eac98c33c3e51eca0f43ffaac5'
idna=load('icu-payload-idna.json');assert idna['result']=='PASS' and len(idna['checks'])==8 and all(e['result']=='PASS' for e in idna['checks'])
assert next(e for e in idna['checks'] if e['name']=='existing package')['data_sha256']=='808fc9bd37589371288b409a8433cf3434ba792ca9d640109c5da41454895d0d'
assert all(p['signature_chains']==0 for p in payload['packages'])
assert load('source-payload-verification.json')['status']=='PASS'
auth=load('auth-resource-verification.json');assert auth['status']=='PASS'
assert auth['resource']['bytes']==242 and auth['resource']['sha256']=='fb8e0b7154cb40458860c74bbb5334da613b34339cd47906a1cae0d6d7290ac8'
native=load('native-patch-stack-ci-verification.json');assert native['status']=='PASS' and native['patch_count']==6 and native['post_build_status']=='ALREADY_APPLIED'
assert native['post_build_from_prefix']==6 and native['native_E32_ctest_passed'] is True
a=load('archive-verification.json');assert a['status']=='PASS' and a['workflow_conclusion']=='success' and a['job_id']==124767283934
run=load('run.json');assert run['id']==41279144446 and run['head_sha']==payload['head_sha'] and run['status']=='completed' and run['conclusion']=='success'
head=payload['head_sha']
summary={'status':'PASS','scope':'Actual downloaded artifact/SIS/source payload and host IDNA verification only; no guest installation/runtime',
 'completed_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'repo':'yeatse/WebKit-Symbian',
 'run_id':41279144446,'job_id':124767283934,'head_sha':head,'branch':'2.52.6-symbian',
 'sis_artifact':{'id':a['artifact_id'],'bytes':a['bytes'],'sha256':a['sha256'],'matches_github_digest':True},
 'workflow_status':'success','all_three_sis_sha256_match_artifact_manifest':True,'package_version':'0.3.45',
 'payload_count':13,'all_package_headers_sections_crc_valid':True,'all_decompressed_payload_sha1_valid':True,
 'all_payload_sizes_valid':True,'unsigned':True,'source_backed_payloads_exact_match':6,'pinned_ca_match':True,
 'icu_data_sha256':'808fc9bd37589371288b409a8433cf3434ba792ca9d640109c5da41454895d0d',
 'idna_host_groups':8,'verification_tools_pinned_to_source_commit':True,
 'auth_password_source_API_and_RSS_limit':255,'auth_resource':auth['resource'],
 'native_patch_stack_CI_build_and_repeat_verified':True,'native_patch_stack_patches':6,
 'packages':[{'name':p['name'],'bytes':(r/'sis'/p['name']).stat().st_size,'sha256':sha(r/'sis'/p['name'])} for p in payload['packages']],
 'guest_installation_performed':False,'guest_runtime_performed':False,'physical_phone_verification_performed':False,
 'elf_artifact_downloaded':False,'local_browser_UI_results_reused':False}
(r/'verification.json').write_text(json.dumps(summary,indent=2)+'\n')
doc=r/'docs-candidate/validation-release-0.3.45/ci';doc.mkdir(parents=True,exist_ok=False)
for name in ['run.json','artifacts.json','jobs.json','archive-verification.json','sis-payload-verification.json',
 'package-verification.json','source-payload-verification.json','icu-payload-idna.json','icu-payload-idna.log',
 'verification-script-identities.json','prepared-source-identities.json','packaging-source-identity.json',
 'commands.json','acquisition.json','finish.py','verification.json','verify-payloads.py','verify-payloads.log','check-downloaded.py','check-source-payloads.py',
 'source-auth-limit-verification.json','check-auth-resource.py','auth-resource-verification.json','verify-auth.log',
 'check-native-patch-stack-ci.py','native-patch-stack-source-identities.json','native-patch-stack-ci-verification.json','native-patch-stack-ci.log','verify-native-stack.log']:
 shutil.copy2(r/name,doc/name)
for sub in ['tests/icu-idna/run.py','tests/icu-idna/probe.cpp','scripts/icu-filter.py','docs/validation-idna/package/verify-icu-sis.py']:
 src=r/'verification-tools/Tools/Symbian'/sub;dst=doc/'verification-tools'/sub;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
shutil.copytree(r/'source-inputs',doc/'source-inputs')
rows='\n'.join('| '+p['name']+' | '+format(p['bytes'],',')+' | `'+p['sha256']+'` |' for p in summary['packages'])
exe={Path(i['extracted_path']).name:i for p in payload['packages'] for i in p['payloads'] if i['target'].endswith('.exe')}
(doc/'README.md').write_text(f'''# CI45 实际安装包核验

通过 GitHub 插件跟进 [Run 41279144446](https://github.com/yeatse/WebKit-Symbian/actions/runs/41279144446)，并实际下载 [sis artifact {a['artifact_id']}](https://github.com/yeatse/WebKit-Symbian/actions/runs/41279144446/artifacts/{a['artifact_id']})。Run45 / job124767283934 均 completed/success，来源为 `2.52.6-symbian` 精确提交 `{head}`，push 触发。

实际 ZIP 为 **{a['bytes']:,} bytes**，SHA256 `{a['sha256']}`，与 GitHub artifact digest 匹配。四个 ZIP 成员及每个成员 CRC 均通过；只下载 sis，没有下载 ELF。

三个实际 unsigned SIS 版本均为 **0.3.45**，由 controller 内容读取核对。SHA256 均匹配 artifact 自带 SHA256SUMS。SIS UID/header CRC、controller/data section CRC、13个目标、全部解压长度和 payload SHA-1、两个 E32 UID3/能力字段及 VFPv2 标记通过。

| SIS | bytes | SHA256 |
| --- | ---: | --- |
{rows}

CI browser.exe：{exe['browser.exe']['bytes']:,} bytes，SHA256 `{exe['browser.exe']['sha256']}`。
CI jsc.exe：{exe['jsc.exe']['bytes']:,} bytes，SHA256 `{exe['jsc.exe']['sha256']}`。

四个 JS、test.html 和 fonts.conf 与精确来源提交的 git blob 逐字节相同。固定 release CA 为188,900 bytes，SHA256 `a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505`，与来源 package-sis.sh 中的固定值一致。实际解压 ICU 数据为10,261,824 bytes，SHA256 `808fc9bd37589371288b409a8433cf3434ba792ca9d640109c5da41454895d0d`。

使用来源提交导出的 run.py/probe.cpp/icu-filter.py，对**本次 SIS 实际解压数据**重新运行8组 host IDNA 检查，覆盖 existing package、三种过滤结果、缺失 UTS46 负对照及 stubdata 隔离。完整工具身份、命令和原始输出已归档；并非复制 CI41/42 报告作为本次通过证据。

本次 CI 的 E32 converter 阶段已完成6个 native patch 的真实构建和 native E32 ctest；构建后再次调用 helper，完整文件 postimage 校验返回 `ALREADY_APPLIED`、`from_prefix=6`。归档记录了实际阶段日志和6个 patch 的精确源 SHA256；CI44 失败报告继续独立保留，不因 CI45 成功删除。

实际 SIS 内 `browser_ui.rsc` 为242 bytes，SHA256 `fb8e0b7154cb40458860c74bbb5334da613b34339cd47906a1cae0d6d7290ac8`，与固定255资源的已知产物一致。精确来源提交的 Auth API 与 RSS 的11项静态检查及 SDK 公开枚举上限255也通过。此项仅核验资源与源契约，不代表本次包已运行密码输入框。

**本次没有安装或运行 CI45 镜像，也没有真机验收。** 本地其他 browser/host/FEP 组合的 UI、下载、触摸或剪贴板结果均不能移作 CI45 runtime 结果。0029 密码遮罩另有独立 host/FEP 身份和 UI 证据，不在这里宣称本包已运行。

原始 ZIP、SIS 和13个解压 payload 留在 `/workspace/symbian-ci45-validation/`，不进入 Git。候选文档只保留元数据、报告、固定源输入、核验脚本及文本输出。`SHA256SUMS` 覆盖全部归档文件。本次核包阶段未改共享源码、workflow 或 guest；核验文档由后续单独的文档提交归档。

复核 SIS：把 ZIP 四个成员放到 `ARTIFACT_DIR/sis/` 后，运行 `python3 verify-payloads.py ARTIFACT_DIR --tools-root verification-tools`。来源工具是固定 git blob 的副本；原始命令见 commands.json。离线源码比对另需保持 source-inputs 的相对目录结构，命令文件中的绝对路径记录的是本次真实执行位置。
''')
(doc/'SHA256SUMS').write_text(''.join(sha(p)+'  '+p.relative_to(doc).as_posix()+'\n' for p in sorted(doc.rglob('*')) if p.is_file() and p!=doc/'SHA256SUMS'))
for line in (doc/'SHA256SUMS').read_text().splitlines():
 expected,name=line.split('  ',1);assert sha(doc/name)==expected
(r/'delivery.json').write_text(json.dumps({'status':'PASS','docs_candidate':str(doc),'artifact_dir':str(r),
 'report_sha256':sha(r/'verification.json'),'docs_sha256sums_sha256':sha(doc/'SHA256SUMS'),
 'guest_mutations':False,'shared_source_mutations':False,'elf_downloaded':False},indent=2)+'\n')
print(json.dumps({'status':'PASS','docs':str(doc),'summary':summary,'exe':exe},indent=2))
