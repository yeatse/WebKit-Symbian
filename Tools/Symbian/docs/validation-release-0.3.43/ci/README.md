# CI43 实际安装包核验

通过 GitHub 插件跟进 [Run 41271896689](https://github.com/yeatse/WebKit-Symbian/actions/runs/41271896689)，并实际下载 [sis artifact 13070386049](https://github.com/yeatse/WebKit-Symbian/actions/runs/41271896689/artifacts/13070386049)。Run43 / job124746192350 均 completed/success，来源为 `2.52.6-symbian` 精确提交 `903ab1fb43113ea312d280bbcddddd1b9cbb3fd9`，push 触发。

实际 ZIP 为 **33,695,059 bytes**，SHA256 `0005f0a862bc5d3d2784f45afb06493fb38c3f2cffc73233799092cfa6d03ff4`，与 GitHub artifact digest 匹配。四个 ZIP 成员及每个成员 CRC 均通过；只下载 sis，没有下载 ELF。

三个实际 unsigned SIS 版本均为 **0.3.43**，由 controller 内容读取核对。SHA256 均匹配 artifact 自带 SHA256SUMS。SIS UID/header CRC、controller/data section CRC、13个目标、全部解压长度和 payload SHA-1、两个 E32 UID3/能力字段及 VFPv2 标记通过。

| SIS | bytes | SHA256 |
| --- | ---: | --- |
| symbian-webkit-icu.sis | 4,768,936 | `44eeb4b1503fe02ee62db95d68577357f4c4f5126ba782f5a760b39c32c3f18d` |
| jsc.sis | 6,895,044 | `1e140e42fd2ddfb2f50a2af4fcbc1b3d87d7e8f4af93254b675539b30328d994` |
| browser.sis | 22,030,348 | `dfca44a27884201735e4717d8cc8092431a15c90da59a622810a3f742c359cdf` |

CI browser.exe：58,734,880 bytes，SHA256 `2cffa5174e19cfa27a45fe5083dc664fd22027d72e9429f18b2c1e37c283404a`。
CI jsc.exe：18,830,912 bytes，SHA256 `f17ac6010f39bcc5a7dabcdc7d4b5a666591ca032070dc873d1555969c4e7324`。

四个 JS、test.html 和 fonts.conf 与精确来源提交的 git blob 逐字节相同。固定 release CA 为188,900 bytes，SHA256 `a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505`，与来源 package-sis.sh 中的固定值一致。实际解压 ICU 数据为10,261,824 bytes，SHA256 `808fc9bd37589371288b409a8433cf3434ba792ca9d640109c5da41454895d0d`。

使用来源提交导出的 run.py/probe.cpp/icu-filter.py，对**本次 SIS 实际解压数据**重新运行8组 host IDNA 检查，覆盖 existing package、三种过滤结果、缺失 UTS46 负对照及 stubdata 隔离。完整工具身份、命令和原始输出已归档；并非复制 CI41/42 报告作为本次通过证据。

**本次没有安装或运行 CI43 镜像，也没有真机验收。** 本地其他 browser/host/FEP 组合的 UI、下载、触摸或剪贴板结果均不能移作 CI43 runtime 结果。0029 密码遮罩另有独立 host/FEP 身份和 UI 证据，不在这里宣称本包已运行。

原始 ZIP、SIS 和13个解压 payload 留在 `/workspace/symbian-ci43-validation/`，不进入 Git。候选文档只保留元数据、报告、固定源输入、核验脚本及文本输出。`SHA256SUMS` 覆盖全部归档文件。核验未改共享源码、workflow 或 guest，也未提交或 push。

复核 SIS：把 ZIP 四个成员放到 `ARTIFACT_DIR/sis/` 后，运行 `python3 verify-payloads.py ARTIFACT_DIR --tools-root verification-tools`。来源工具是固定 git blob 的副本；原始命令见 commands.json。离线源码比对另需保持 source-inputs 的相对目录结构，命令文件中的绝对路径记录的是本次真实执行位置。
