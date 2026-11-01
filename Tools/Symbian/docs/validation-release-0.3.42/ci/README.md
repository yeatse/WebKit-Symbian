# CI42 实际安装包核验

2026-11-01（Asia/Shanghai），通过 GitHub connector 查询并下载 [Run 41265079906](https://github.com/yeatse/WebKit-Symbian/actions/runs/41265079906) 的 [sis artifact 13067725061](https://github.com/yeatse/WebKit-Symbian/actions/runs/41265079906/artifacts/13067725061)。Run42/job124726099581 completed/success，来源精确为 `2.52.6-symbian` 提交 `46234c2b06392d938985296f3b52ac07440a320a`，触发类型 push。

实际 ZIP 为 **33,652,863 bytes**，SHA256 `29710b505b67f77b7fe4f6662fe41459b704aed1dcbd9da1666bb241e44a99bf`，与 GitHub artifact digest 精确匹配。四个 ZIP 成员和各成员CRC检查通过；仅下载 sis，没有下载 ELF。

三个实际 unsigned SIS 均为 **0.3.42**，不是由 run number 推断；SHA256 均匹配 artifact 自带 SHA256SUMS。SIS UID/header CRC、controller/data section CRC、13个安装目标、全部解压长度和payload SHA-1、E32 UID3/能力字段及VFPv2标记通过。

| SIS | bytes | SHA256 |
| --- | ---: | --- |
| symbian-webkit-icu.sis | 4,768,936 | `33565aeee99a021c2dead1275da6e706af7e80baea8111775bd73cd075e981fa` |
| jsc.sis | 6,894,492 | `b8abd2412aa66d029e66803eb0aa31d820a52a391991a5acce454bf43b9c43cb` |
| browser.sis | 21,988,704 | `a3dc80af19c39f74483e51275cf85813a0a725bbe4eaecd76622ae93f2960e4a` |

CI browser.exe：58,645,144 bytes，SHA256 `37c7362756c72a94d14e367507e1c28f1102f674ddd1b3aee01dee2cd064329b`。
CI jsc.exe：18,830,172 bytes，SHA256 `4ed9fa1d5157a629cf94f1ba3bfa8cb2b9e11de6add864d738f3f400bf970fad`。

四个 JS、test.html 和 fonts.conf 与该精确源提交的 git blob 逐字节匹配。固定 release CA 为188,900 bytes，SHA256 `a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505`。实际从 ICU SIS 解压的数据为10,261,824 bytes，SHA256 `808fc9bd37589371288b409a8433cf3434ba792ca9d640109c5da41454895d0d`，包含所需 UTS46 修复。

本次用来源提交中固定的 run.py/probe.cpp/icu-filter.py，对**本次实际解压数据**重新运行8组 host IDNA检查，通过实际existing package与3种过滤结果、缺失UTS46负对照及stubdata隔离检查；不是把CI41数据报告直接复制来冒充通过。脚本身份和完整输出均保留。

**没有安装或运行此CI42包，没有真机验收。** C13/C14本地新增触摸hover/FEP等修复不在该来源提交，本地clipboard/touch通过结果不能移用到CI42；本地C10的安装结果也不能归给这里。

原始 ZIP、SIS 和13个解压payload保存在 `/workspace/symbian-ci42-validation/`，不放入Git。这里保存元数据、独立报告、核验脚本与原始文本输出；SHA256SUMS覆盖归档。`verification-tools/` 是从来源提交导出的只读工具副本。

包复核：将ZIP四个成员放入 `ARTIFACT_DIR/sis/` 后运行：

```sh
python3 Tools/Symbian/docs/validation-release-0.3.42/ci/verify-payloads.py ARTIFACT_DIR
```

外部运行可传 `--tools-root /path/to/WebKit/Tools/Symbian`；该默认参数结构适用于合入仓库后的文档路径。IDNA原命令与源工具身份见commands.json、verification-script-identities.json及外部报告；不需要连接模拟器。此核验未改共享源码、安装guest、提交或push。
