# CI50 实际安装包核验

通过 GitHub 插件跟进 [Run 41299027063](https://github.com/yeatse/WebKit-Symbian/actions/runs/41299027063)，并实际下载 [sis artifact 13082244726](https://github.com/yeatse/WebKit-Symbian/actions/runs/41299027063/artifacts/13082244726)。Run50 / job124825909684 均 completed/success，来源为 `2.52.6-symbian` 精确提交 `886d227f1780298cc17cf99695a3c78c0b7d9ff6`，push 触发。

实际 ZIP 为 **34,183,411 bytes**，SHA256 `c8ba4322797ad1cd26000334b2be5840d76969e32830ca04d410086fd8515aee`，与 GitHub artifact digest 匹配。四个 ZIP 成员及每个成员 CRC 均通过；只下载 sis，没有下载 ELF。

三个实际 unsigned SIS 版本均为 **0.3.50**，由 controller 内容读取核对。SHA256 均匹配 artifact 自带 SHA256SUMS。SIS UID/header CRC、controller/data section CRC、13个目标、全部解压长度和 payload SHA-1、两个 E32 UID3/能力字段及 VFPv2 标记通过。

| SIS | bytes | SHA256 |
| --- | ---: | --- |
| symbian-webkit-icu.sis | 4,768,936 | `6708729acc698ed780a808831d0992a8b72f6151f334d180774bc1a5caaa0305` |
| jsc.sis | 6,895,108 | `ef52eb78135e4d3661ffbd87179a1533859e272d57960a8174b06c3247aaf518` |
| browser.sis | 22,518,636 | `0280ea2ecce01b5dd28b30e1921d3a92a639e448c66c1bd7d22bcf7821d1359a` |

CI browser.exe：59,863,884 bytes，SHA256 `30fd5b179a1ca595216c77da1e4bf4561e9d362dcc2dcde9fd4354d4cb1c0429`。
CI jsc.exe：18,831,228 bytes，SHA256 `f9ed3cdc79af811bd55f221c5cebeb0079c7aa5fe7740668f712bb9621932c2c`。

四个 JS、test.html 和 fonts.conf 与精确来源提交的 git blob 逐字节相同。固定 release CA 为188,900 bytes，SHA256 `a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505`，与来源 package-sis.sh 中的固定值一致。实际解压 ICU 数据为10,261,824 bytes，SHA256 `808fc9bd37589371288b409a8433cf3434ba792ca9d640109c5da41454895d0d`。

使用来源提交导出的 run.py/probe.cpp/icu-filter.py，对**本次 SIS 实际解压数据**重新运行8组 host IDNA 检查，覆盖 existing package、三种过滤结果、缺失 UTS46 负对照及 stubdata 隔离。完整工具身份、命令和原始输出已归档；并非复制 CI41/42 报告作为本次通过证据。

本次 CI 的 E32 converter 阶段已完成9个 native patch 的真实构建和 native E32 ctest；构建后再次调用 helper，完整文件 postimage 校验返回 `ALREADY_APPLIED`、`from_prefix=9`。归档记录了实际阶段日志和9个 patch 的精确源 SHA256；CI44 失败报告继续独立保留，不因 CI50 成功删除。

实际 SIS 内 `browser_ui.rsc` 为242 bytes，SHA256 `fb8e0b7154cb40458860c74bbb5334da613b34339cd47906a1cae0d6d7290ac8`，与固定255资源的已知产物一致。精确来源提交的 Auth API 与 RSS 的11项静态检查及 SDK 公开枚举上限255也通过。此项仅核验资源与源契约，不代表本次包已运行密码输入框。

本次精确来源另外核对了25项网络源码合同：nghttp2 1.68.0固定摘要、IPv6/threaded resolver/mbedTLS/H2开关、8/8/4连接池、8流、256KiB窗口、512KiB session allocator、WSS固定H1、两个CI缓存helper输入、native resolver三文件/selector最终CMake属性、16地址与10秒边界、完整IPv6地址/scope、curl自有结果分配以及三条链接路径的esock/insock。原始源码及git blob身份已归档。实际CI日志也确认nghttp2/curl各构建一次，产物libnghttp2.a为352222B、libcurl.a为891690B；实际SIS browser包含nghttp2/1.68.0/mbedTLS相关字节。本次未跳过的CI依赖构建受精确源码中的llvm-nm后置门槛约束：curl_addrinfo.c.o选中Curl_symbian_getaddrinfo且不再引用getaddrinfo/freeaddrinfo，archive内两个native bridge定义各唯一；版本2 stamp仅在门槛通过后写入。build-deps将整个curl子进程输出重定向到deps/curl.log，成功时console没有nm成功文字或原表；本次没有下载该重定向日志。归档保存真实curl构建标记/库产物行、未跳过且成功的CI步骤，以及独立set-e子进程内不可绕过的完整门槛源码。因此结论是源码门槛约束下的CI成功，不伪称直接审过CI nm输出，也不当作最终ELF符号审计。完整证据见network-artifact-source-build-verification.json；未取ELF、未审最终ELF符号或link map，源码单份依赖声明和字符串不被当作本包runtime证明。这是源码/构建契约核验，不能仅由代码或二进制字符串推断本包已经运行H2/IPv6/ALPN。

本次来源默认Classic WebAudio已ON，13个实际来源快照（12runtime文件+OptionsSymbian）均按精确提交git blob冻结；9项源合同确认classic启用、真实手势要求、AudioWorklet getter/构造器setting关闭、缺失MMF PCM的mediaElement source诚实NotSupported。实际CI中9patch/15文件的完整SDK栈、18项回归、allocator、math和complex步骤均成功；原始stdout明确math三profile INSTALLED在前，固定LLVM源的complex INSTALLED/296原成员检查在后。complex实际构建3profile和std::complex ARMcanary/非空符号/成员保持门槛由精确源helper无条件执行后才会安装打印成功，但远端逐成员原表/原nm输出未下载，不伪称直接审过这些原表。实际SIS browser另外由现有EKA普通E32 parser解析header/import/relocation，并独立验证UID checksum、实际header CRC和完整import bounds/code slots，导入表确含mediaclientaudiostream.dll；不是strings推断，也不是最终ELF符号审计。详见SDK-audio-CI-verification.json和e32-imports-verification.json。

**本次没有安装或运行 CI50 镜像，也没有真机验收。** 本地C25/host0032或其他browser/host/FEP组合的DNS/H2/IPv6、UI、下载、触摸或剪贴板结果均不能移作CI50 runtime结果；本次包不含开发树CookieStore/Multi WIP，也不部署EKA0032。0029 密码遮罩另有独立 host/FEP 身份和 UI 证据，不在这里宣称本包已运行。

原始 ZIP、SIS 和13个解压 payload 留在 `/workspace/symbian-ci50-validation/`，不进入 Git。候选文档只保留元数据、报告、固定源输入、核验脚本及文本输出。`SHA256SUMS` 覆盖全部归档文件。本次核包阶段未改共享源码、workflow 或 guest；本候选核验文档尚未提交或 push。

复核 SIS：把 ZIP 四个成员放到 `ARTIFACT_DIR/sis/` 后，运行 `python3 verify-payloads.py ARTIFACT_DIR --tools-root verification-tools`。来源工具是固定 git blob 的副本；原始命令见 commands.json。离线源码比对另需保持 source-inputs 的相对目录结构，命令文件中的绝对路径记录的是本次真实执行位置。
