# FEP 原生构建与可迁移交付

本目录归档 2026-11-01 的实际本地 ARM 构建、原生格式检查及部署证据。DLL/ELF 保留在
原外部工作目录，没有加入仓库。

实际 DLL：99,272 bytes，SHA256
`aad1a4c6a4a52c70d3ac430d45dce671a8ff664071c921146bc65f754a612f6b`。
UID2 `0x10009d8d`、UID3 `0x100056de`、capabilities 0、唯一导出 ordinal 1，
implementation UID `0x101fd65a`。实际 EKA parser 分别读取旧压缩 DLL 与新 DLL，UID/caps/导出数一致。
新的 native inspector 验证新镜像的 CRC、EHABI descriptor、imports/exports/relocations；
官方 fepbase 的 168 DEF 项中 129 present binding 精确匹配，实际 FEP 有 32 个 fepbase import slot。

原外部 converter 构建通过 62 项 GTest、13 项 CLI、原生头 canary 和 style 检查，默认 DLL
输出保持与旧工具逐字节相同。完整日志使用 gzip 保留。原始 JSON 路径和输入哈希保持不变。
原构建曾遇到 broad Avkon generated-resource guard 及内部 linker symbol 暴露，实际最终工程
用所需 frozen proxies 并隐藏内部 linker 标签，完整 8/8 构建退出 0；没有放宽未知符号检查。

`deployment.json` 记录 root 在无 guest 时部署到实际 runtime patch 目录，host SHA 为
`277db2b3e44c69d4b860b2f76cf194178e820556671ad32f4891fcae2cb4c2ff`。
该部署记录的 native_UI_verified 为 false，本目录不声称 FEP/clipboard UI 验收通过。

可迁移构建入口：[build.py](../../../eka2l1/fep/build.py)，使用 `--eka-source`、
`SYMBIAN_SDK_PREFIX` 和 `WORK`。官方头/DEF/EPL 原文随入口保留，来源和真实 Git blob 精确固定。
converter 改动在 SDK 源补丁 `patches/symbian-native/0006-*`，包含六个新增 GTest 和现有
symbian_e32_test 接线，现有 build-symbian-native.sh 可应用/运行。

本轮可迁移整理仅执行 AST/help/实际 prepare-only、源码组装一致性、补丁顺序重放检查和独立审查。
**没有完整运行新通用脚本的构建，也没有 guest**。历史临时头比官方 Git blob 多一个 EOF LF；
可迁移副本已恢复官方准确字节，详见 portable-script-checks.json。

今后每次 EKA host 构建后，恢复并核对新 FEP DLL 与原 MMF 两 DLL/两 map 共五个资产，见
five-asset-identities.json。当前 `.elf.map` 是 GNU ARM linker map；FEP 维持原 UID/ordinal，
无额外 runtime ordinal remap。新的完整构建与 UI 结果需要独立记录，不能继承旧结果。
