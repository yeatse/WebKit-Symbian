# CI51 实际 SIS artifact 核验

[Actions 51](https://github.com/yeatse/WebKit-Symbian/actions/runs/41808546697) 与 build job `126501600220` 已 completed/success，来源为 `2.52.6-symbian` 的精确提交 `857a05456d0c87c104577ccf213dd0b7e2b2d688`，push 触发。实际 [sis artifact 13310974170](https://github.com/yeatse/WebKit-Symbian/actions/runs/41808546697/artifacts/13310974170) 已下载核验。

ZIP 为 **34,187,395 bytes**，SHA256 `43016d08f6b14233f1556360470e3c2d9977e441696b90f4037040f48f56e11c`，与 GitHub artifact digest 匹配。四个 ZIP 成员及 CRC 通过；只下载 SIS，没有下载 ELF。CLI 临时重定向曾返回403并留下0B文件，失败元数据保留；随后正常 GitHub 插件获取同一 artifact，临时文件引用只在内存中传递，没有写入 URL、签名查询或凭据。

三个 unsigned SIS 均从实际 controller 读取版本 **0.3.51**。UID/header/section CRC、13 个固定目标、压缩长度、SHA-1、artifact 内 SHA256SUMS、两个 E32 UID3/capability/VFPv2 字段均通过。

| SIS | bytes | SHA256 |
| --- | ---: | --- |
| symbian-webkit-icu.sis | 4,768,936 | `58b20151b06c02f7019cced560492abf6ff5198f416f0225ea203048e975765d` |
| jsc.sis | 6,895,108 | `f4d5451b592d93dc365886eb915b0cbf32f0f5acd0c31a7606977a9bb1a9ea95` |
| browser.sis | 22,522,620 | `0dfe042f58e14822fe471b8dc5e5810c0984b5c0a59f10d7660a05a949d13749` |

实际 browser.exe 为 **59,872,996 bytes**，SHA256 `a9b800ec90d947a09631783d2debe87d715c92d6493cbd7be78728baa3bdf998`。普通 EKA E32 parser 加独立 CRC/import-bounds frontend 解析了实际导入表：30 个 DLL、1687 个 slots，包含 `surfacemanager.dll`，保留 `mediaclientaudiostream.dll`，没有 `surfaceupdateclient.dll`。这是实际 import-table 证据，不是 strings 或最终 ELF 符号审计。

四个 JS、test.html 和 fonts.conf 与精确来源 git blob 逐字节匹配；CA 与 ICU 固定数据摘要也通过。对本次实际解压的 `icudt70l.dat` 重新运行了八组 ICU host 检查，全部通过。Auth 的 11 项来源合同及本次实际 242B / `fb8e0b7154cb40458860c74bbb5334da613b34339cd47906a1cae0d6d7290ac8` 资源通过；没有拿本地含旧 WIP 的 C33 383B 资源替代本包。

公开 CI job stdout 经正常 GitHub 插件读取，只在内存及匿名 stdin 中处理，没有写原始日志。固定摘要证明：10 patches / 20 owned files 的完整前后 helper，21 项 stack host 回归及 native E32 ctest 通过；math 三 profile、complex 精确来源及296个原成员保留的安装摘要通过。原 nm/member 表没有下载，结论受精确 helper 控制流约束。

新增 ScreenPlay step 的 47 场景 / 298 断言 / UBSan 和私有客户端 8 场景 / 75 断言通过，所报实际生产源 hash 与本次 commit 相同。CI 私有客户端没有 `--sanitize`，因此其 `sanitizers=[]`；不能把 Root 本地 ASan/UBSan 结果改写成此次 CI 的 sanitizer 结果。SDK ScreenPlay 安装摘要新增12项，App include 已添加，runtime archives 未改变，没有执行 guest native target。

本次 nghttp2/curl build-invocation marker 均为0；没有声称重建依赖对象或直接查看新的 nm 表。25项网络和9项 Classic WebAudio 来源合同保持通过，它们不替代本次镜像的网络、音频、显示或手势运行。

独立复核重新计算实际 ZIP digest/CRC，内存重解析三个 SIS/13载荷并核对真实 payload，复核39项来源身份、Auth/ICU输入，重新运行普通 E32 parser，均通过。匿名 CI 原流没有留盘，因此独立复核没有冒称再次重播该原流。

**本验包任务没有安装或运行 CI51 镜像，没有 guest/display、真机或性能验收。** Root 的本地 ScreenPlay API/页面结果属于另一组明确记录的镜像与 host。branch push 上传的是 artifacts；本次没有发布 GitHub Release，Release 步骤只由 `symbian-v*` tag 触发。

原始 ZIP、三 SIS 和13个实际 payload 留在 `/workspace/symbian-screenplay-ci-51-validation/`，不进入 Git。本目录只保留公开固定字段、报告、精确来源、许可与检查脚本；没有 payload/ELF、原始 CI/runtime 日志、临时下载查询或私人配置。`manifest.json` 与 `SHA256SUMS` 覆盖公开档案。

复核时把同一已校验 ZIP 放在 `ARTIFACT_DIR/sis.zip`，保留本目录结构与 `spec.json`；还需提供 `parser-pin.json` 所指的普通 E32 parser（可由 `e32-parser` 公开源码/构建身份重建）以及已固定 ICU host 源/工具。执行 `python3 verify-artifact.py ARTIFACT_DIR --icu-root ICU_ROOT`；它要求 fresh `sis/`、payload 输出和 ICU 结果，避免拿旧结果当本次通过。CI摘要可从相同 job 重新经匿名 stdin 运行 `check-ci-log.py ARTIFACT_DIR`。
