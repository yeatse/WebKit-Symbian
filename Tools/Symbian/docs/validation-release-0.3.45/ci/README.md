# CI45 实际安装包核验

通过 GitHub 插件跟进 [Run 41279144446](https://github.com/yeatse/WebKit-Symbian/actions/runs/41279144446)，并实际下载 [sis artifact 13073290575](https://github.com/yeatse/WebKit-Symbian/actions/runs/41279144446/artifacts/13073290575)。Run45 / job124767283934 均 completed/success，来源为 `2.52.6-symbian` 精确提交 `e0e2f321d4e2d4eac98c33c3e51eca0f43ffaac5`，push 触发。

实际 ZIP 为 **33,695,107 bytes**，SHA256 `ecc98707ca1edb7e9ede0bbe68add4b913eefac6e9ff69458f97fe0b42600844`，与 GitHub artifact digest 匹配。四个 ZIP 成员及每个成员 CRC 均通过；只下载 sis，没有下载 ELF。

三个实际 unsigned SIS 版本均为 **0.3.45**，由 controller 内容读取核对。SHA256 均匹配 artifact 自带 SHA256SUMS。SIS UID/header CRC、controller/data section CRC、13个目标、全部解压长度和 payload SHA-1、两个 E32 UID3/能力字段及 VFPv2 标记通过。

| SIS | bytes | SHA256 |
| --- | ---: | --- |
| symbian-webkit-icu.sis | 4,768,936 | `4b08a2c7e046b0376b50b0122aa5a19616e70fb0a06fd503c82cb37ad225463b` |
| jsc.sis | 6,895,044 | `46695e10b9e1360b04d0527f0d34b22f47db44c16096ee2e0f345f75e882cf1f` |
| browser.sis | 22,030,396 | `4ec17521c0e74611e8c4f8a4fe0221e32e8fd8b2d751803223ad1320c8b198fb` |

CI browser.exe：58,734,680 bytes，SHA256 `00bc0eb3a62bcb31e4a2cceef8169fc7d23e71ab33e274093434f8aa54ce894d`。
CI jsc.exe：18,830,912 bytes，SHA256 `f17ac6010f39bcc5a7dabcdc7d4b5a666591ca032070dc873d1555969c4e7324`。

四个 JS、test.html 和 fonts.conf 与精确来源提交的 git blob 逐字节相同。固定 release CA 为188,900 bytes，SHA256 `a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505`，与来源 package-sis.sh 中的固定值一致。实际解压 ICU 数据为10,261,824 bytes，SHA256 `808fc9bd37589371288b409a8433cf3434ba792ca9d640109c5da41454895d0d`。

使用来源提交导出的 run.py/probe.cpp/icu-filter.py，对**本次 SIS 实际解压数据**重新运行8组 host IDNA 检查，覆盖 existing package、三种过滤结果、缺失 UTS46 负对照及 stubdata 隔离。完整工具身份、命令和原始输出已归档；并非复制 CI41/42 报告作为本次通过证据。

本次 CI 的 E32 converter 阶段已完成6个 native patch 的真实构建和 native E32 ctest；构建后再次调用 helper，完整文件 postimage 校验返回 `ALREADY_APPLIED`、`from_prefix=6`。归档记录了实际阶段日志和6个 patch 的精确源 SHA256；CI44 失败报告继续独立保留，不因 CI45 成功删除。

实际 SIS 内 `browser_ui.rsc` 为242 bytes，SHA256 `fb8e0b7154cb40458860c74bbb5334da613b34339cd47906a1cae0d6d7290ac8`，与固定255资源的已知产物一致。精确来源提交的 Auth API 与 RSS 的11项静态检查及 SDK 公开枚举上限255也通过。此项仅核验资源与源契约，不代表本次包已运行密码输入框。

**本次没有安装或运行 CI45 镜像，也没有真机验收。** 本地其他 browser/host/FEP 组合的 UI、下载、触摸或剪贴板结果均不能移作 CI45 runtime 结果。0029 密码遮罩另有独立 host/FEP 身份和 UI 证据，不在这里宣称本包已运行。

原始 ZIP、SIS 和13个解压 payload 留在 `/workspace/symbian-ci45-validation/`，不进入 Git。候选文档只保留元数据、报告、固定源输入、核验脚本及文本输出。`SHA256SUMS` 覆盖全部归档文件。本次核包阶段未改共享源码、workflow 或 guest；核验文档由后续单独的文档提交归档。

复核 SIS：把 ZIP 四个成员放到 `ARTIFACT_DIR/sis/` 后，运行 `python3 verify-payloads.py ARTIFACT_DIR --tools-root verification-tools`。来源工具是固定 git blob 的副本；原始命令见 commands.json。离线源码比对另需保持 source-inputs 的相对目录结构，命令文件中的绝对路径记录的是本次真实执行位置。
