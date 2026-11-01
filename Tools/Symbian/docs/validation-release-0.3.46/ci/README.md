# CI46 实际安装包核验

通过 GitHub 插件跟进 [Run 41282488166](https://github.com/yeatse/WebKit-Symbian/actions/runs/41282488166)，并实际下载 [sis artifact 13074172523](https://github.com/yeatse/WebKit-Symbian/actions/runs/41282488166/artifacts/13074172523)。Run46 / job124777066796 均 completed/success，来源为 `2.52.6-symbian` 精确提交 `79aee8089ce6cec9816b7ed512ee91f0278491b6`，push 触发。

实际 ZIP 为 **33,762,119 bytes**，SHA256 `86247a47fa7d7c983951e3a4023274bf9836084a778e4e0610306419d115f21b`，与 GitHub artifact digest 匹配。四个 ZIP 成员及每个成员 CRC 均通过；只下载 sis，没有下载 ELF。

三个实际 unsigned SIS 版本均为 **0.3.46**，由 controller 内容读取核对。SHA256 均匹配 artifact 自带 SHA256SUMS。SIS UID/header CRC、controller/data section CRC、13个目标、全部解压长度和 payload SHA-1、两个 E32 UID3/能力字段及 VFPv2 标记通过。

| SIS | bytes | SHA256 |
| --- | ---: | --- |
| symbian-webkit-icu.sis | 4,768,936 | `a333fc02d705457495abd823dcc879d29830164c3bdef1af893b726a2a8e8dfd` |
| jsc.sis | 6,895,044 | `0e0bb3941a586ce29d51a18186a64c3fc9af6f3218ff7eaa9b612a451aeb1217` |
| browser.sis | 22,097,408 | `ee9db20dd3a75a74a5f7ae2c4d8a71157bab8840c4bc04b5f8add170d62d80fc` |

CI browser.exe：58,884,100 bytes，SHA256 `4025656bd637ac1b1d1c02312919ebb111a1a7e64c44a9ee2a727b0e179ca113`。
CI jsc.exe：18,830,912 bytes，SHA256 `f17ac6010f39bcc5a7dabcdc7d4b5a666591ca032070dc873d1555969c4e7324`。

四个 JS、test.html 和 fonts.conf 与精确来源提交的 git blob 逐字节相同。固定 release CA 为188,900 bytes，SHA256 `a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505`，与来源 package-sis.sh 中的固定值一致。实际解压 ICU 数据为10,261,824 bytes，SHA256 `808fc9bd37589371288b409a8433cf3434ba792ca9d640109c5da41454895d0d`。

使用来源提交导出的 run.py/probe.cpp/icu-filter.py，对**本次 SIS 实际解压数据**重新运行8组 host IDNA 检查，覆盖 existing package、三种过滤结果、缺失 UTS46 负对照及 stubdata 隔离。完整工具身份、命令和原始输出已归档；并非复制 CI41/42 报告作为本次通过证据。

本次 CI 的 E32 converter 阶段已完成6个 native patch 的真实构建和 native E32 ctest；构建后再次调用 helper，完整文件 postimage 校验返回 `ALREADY_APPLIED`、`from_prefix=6`。归档记录了实际阶段日志和6个 patch 的精确源 SHA256；CI44 失败报告继续独立保留，不因 CI46 成功删除。

实际 SIS 内 `browser_ui.rsc` 为242 bytes，SHA256 `fb8e0b7154cb40458860c74bbb5334da613b34339cd47906a1cae0d6d7290ac8`，与固定255资源的已知产物一致。精确来源提交的 Auth API 与 RSS 的11项静态检查及 SDK 公开枚举上限255也通过。此项仅核验资源与源契约，不代表本次包已运行密码输入框。

本次精确来源另外核对了13项网络源码合同：nghttp2 1.68.0固定摘要、IPv6/threaded resolver/mbedTLS/H2开关、8/8/4连接池、8流、256KiB窗口、512KiB session allocator、WSS固定H1和两个CI缓存helper输入。原始源码及git blob身份已归档。实际CI日志也确认nghttp2/curl各构建一次，产物libnghttp2.a为352222B、libcurl.a为883660B；实际SIS browser包含nghttp2/1.68.0/mbedTLS相关字节。完整证据见network-artifact-source-build-verification.json；未取ELF、未审最终ELF符号或link map，源码单份依赖声明和字符串不被当作本包runtime证明。这是源码/构建契约核验，不能仅由代码或二进制字符串推断本包已经运行H2/IPv6/ALPN。

**本次没有安装或运行 CI46 镜像，也没有真机验收。** 本地其他 browser/host/FEP 组合的 UI、下载、触摸或剪贴板结果均不能移作 CI46 runtime 结果。0029 密码遮罩另有独立 host/FEP 身份和 UI 证据，不在这里宣称本包已运行。

原始 ZIP、SIS 和13个解压 payload 留在 `/workspace/symbian-ci46-validation/`，不进入 Git。候选文档只保留元数据、报告、固定源输入、核验脚本及文本输出。`SHA256SUMS` 覆盖全部归档文件。本次核包阶段未改共享源码、workflow 或 guest；验包档案随后由主任务以独立文档提交归档。

复核 SIS：把 ZIP 四个成员放到 `ARTIFACT_DIR/sis/` 后，运行 `python3 verify-payloads.py ARTIFACT_DIR --tools-root verification-tools`。来源工具是固定 git blob 的副本；原始命令见 commands.json。离线源码比对另需保持 source-inputs 的相对目录结构，命令文件中的绝对路径记录的是本次真实执行位置。
