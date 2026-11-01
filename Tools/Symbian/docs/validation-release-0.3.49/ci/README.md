# CI49 实际安装包核验

通过 GitHub 插件跟进 [Run 41293400197](https://github.com/yeatse/WebKit-Symbian/actions/runs/41293400197)，并实际下载 [sis artifact 13078106906](https://github.com/yeatse/WebKit-Symbian/actions/runs/41293400197/artifacts/13078106906)。Run49 / job124809155850 均 completed/success，来源为 `2.52.6-symbian` 精确提交 `0885f13ce4252a96b77d4c8cac0579ba2b8ba618`，push 触发。

实际 ZIP 为 **33,763,135 bytes**，SHA256 `fa7910e07e06a9d47de50799f2cdf78abfbd2cc45b4661b56f0c8e6eb692d9be`，与 GitHub artifact digest 匹配。四个 ZIP 成员及每个成员 CRC 均通过；只下载 sis，没有下载 ELF。

三个实际 unsigned SIS 版本均为 **0.3.49**，由 controller 内容读取核对。SHA256 均匹配 artifact 自带 SHA256SUMS。SIS UID/header CRC、controller/data section CRC、13个目标、全部解压长度和 payload SHA-1、两个 E32 UID3/能力字段及 VFPv2 标记通过。

| SIS | bytes | SHA256 |
| --- | ---: | --- |
| symbian-webkit-icu.sis | 4,768,936 | `caa47a14c0a8c7a1c45403ce19b01f8e6957098e4177d13a96cc1229913f8520` |
| jsc.sis | 6,895,108 | `ee13440bf73f44b398c66beb30dd361d1a009a1bd14f401fbab3477dcfa30abf` |
| browser.sis | 22,098,360 | `0dab330ef0b910314e693fe53750527582da8f19d2a02ce78d59b58b25af23cd` |

CI browser.exe：58,888,704 bytes，SHA256 `5c4290dc55e9413a34e3d38c9cf7e628b29c219ad5cb3286f5c70d1c00562c92`。
CI jsc.exe：18,831,228 bytes，SHA256 `297fa6154dff968e504705ba7223e33a228af38895db54138fe67e18386272fa`。

四个 JS、test.html 和 fonts.conf 与精确来源提交的 git blob 逐字节相同。固定 release CA 为188,900 bytes，SHA256 `a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505`，与来源 package-sis.sh 中的固定值一致。实际解压 ICU 数据为10,261,824 bytes，SHA256 `808fc9bd37589371288b409a8433cf3434ba792ca9d640109c5da41454895d0d`。

使用来源提交导出的 run.py/probe.cpp/icu-filter.py，对**本次 SIS 实际解压数据**重新运行8组 host IDNA 检查，覆盖 existing package、三种过滤结果、缺失 UTS46 负对照及 stubdata 隔离。完整工具身份、命令和原始输出已归档；并非复制 CI41/42 报告作为本次通过证据。

本次 CI 的 E32 converter 阶段已完成6个 native patch 的真实构建和 native E32 ctest；构建后再次调用 helper，完整文件 postimage 校验返回 `ALREADY_APPLIED`、`from_prefix=6`。归档记录了实际阶段日志和6个 patch 的精确源 SHA256；CI44 失败报告继续独立保留，不因 CI49 成功删除。

实际 SIS 内 `browser_ui.rsc` 为242 bytes，SHA256 `fb8e0b7154cb40458860c74bbb5334da613b34339cd47906a1cae0d6d7290ac8`，与固定255资源的已知产物一致。精确来源提交的 Auth API 与 RSS 的11项静态检查及 SDK 公开枚举上限255也通过。此项仅核验资源与源契约，不代表本次包已运行密码输入框。

本次精确来源另外核对了25项网络源码合同：nghttp2 1.68.0固定摘要、IPv6/threaded resolver/mbedTLS/H2开关、8/8/4连接池、8流、256KiB窗口、512KiB session allocator、WSS固定H1、两个CI缓存helper输入、native resolver三文件/selector最终CMake属性、16地址与10秒边界、完整IPv6地址/scope、curl自有结果分配以及三条链接路径的esock/insock。原始源码及git blob身份已归档。本次实际依赖缓存命中，WebCore libraries构建步骤合法skipped，当前job未重建nghttp2/curl，也未执行其nm后置门槛。实际日志恢复的key为`deps-belle-0.3.0-09f38b17fd2e94235b45d0af889918556bd521fa2c73e5ad242d981c58ea85e6`，与已观察CI47成功构建后保存的key相同；两提交全部cache key源输入及13个网络源文件逐字节一致。归档保留当前cache-hit/restore行、CI47保存同一key的原始摘录及生产者报告。CI47的curl子进程曾受精确源码中不可绕过的llvm-nm门槛约束，但其重定向nm原表并未下载；这次不能伪称门槛重新执行或直接审过nm输出。实际CI49 WebCore/browser/SIS步骤成功，实际SIS browser包含nghttp2/1.68.0/mbedTLS字节；缓存内容未独立下载/逐成员哈希、未取最终ELF或link map。这是源码、cache key来源及实际包字节核验，不是本包DNS/H2/IPv6/ALPN运行证明。

本次提交仅更新6个host filesystem测试文件及文档，0个生产源码改动。3个UTF-16生产文件和OptionsSymbian的精确git blob与已独立复核16项合同的CI48来源逐字节相同；本次另做9项source/scope核对，默认ENABLE_WEB_AUDIO仍为OFF。实际CI中Belle回归步骤未跳过且成功，native-filesystem全部9组PASS真实输出已归档，包含原有断言和新增ASCII/空文件/BMP+surrogate文件名的metadata、NUL/256/257及no-follow门槛。测试用显式-fshort-wchar和边界iconv适配真实host stat/access，保持实际生产helper及其wchar2断言，结果仅是host控制流证据，不能当作目标Open C/ARM ABI或本CI浏览器读取Unicode/GUI上传已验收。CI48原FAIL与无SIS记录继续保留。详细见independent-source-review.md和host-filesystem-CI-verification.json。

**本次没有安装或运行 CI49 镜像，也没有真机验收。** 本地C25/host0032或其他browser/host/FEP组合的DNS/H2/IPv6、UI、下载、触摸或剪贴板结果均不能移作CI49 runtime结果；本次包不含开发树CookieStore/Multi WIP，也不部署EKA0032。0029 密码遮罩另有独立 host/FEP 身份和 UI 证据，不在这里宣称本包已运行。

原始 ZIP、SIS 和13个解压 payload 留在 `/workspace/symbian-ci49-validation/`，不进入 Git。候选文档只保留元数据、报告、固定源输入、核验脚本及文本输出。`SHA256SUMS` 覆盖全部归档文件。本次核包阶段未改共享源码、workflow 或 guest；本候选核验文档尚未提交或 push。

复核 SIS：把 ZIP 四个成员放到 `ARTIFACT_DIR/sis/` 后，运行 `python3 verify-payloads.py ARTIFACT_DIR --tools-root verification-tools`。来源工具是固定 git blob 的副本；原始命令见 commands.json。离线源码比对另需保持 source-inputs 的相对目录结构，命令文件中的绝对路径记录的是本次真实执行位置。

后续空间维护（2026-11-01）：实际核验和独立终审完成后，为保留并行runtime恢复空间，仅删除本目录外的派生sis/与payloads/解包，释放121,943,853B。canonical ci49-sis.zip及其GitHub匹配摘要、ZIP CRC、3SIS/13payload逐项摘要、全部原报告/脚本/源均保留；原文件可从canonical ZIP精确重建。清理前再次逐项核对全部字节/摘要，见derived-extraction-cleanup.json及cleanup-derived.py。这里的通过仍是本次实际解包时的核验记录，不依赖派生副本永久存在。
