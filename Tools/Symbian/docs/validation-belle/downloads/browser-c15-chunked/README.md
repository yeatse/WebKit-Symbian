# C15 chunked 下载实际记录：PASS

2026-11-01（Asia/Shanghai），root 通过真实 XTest 和原生对话框观察本例；
本目录只归档已经结束的运行，没有启动 guest/display 或改写原始记录。

- Run：dl-h-17cb8dbe9f88，动态 localhost URL 见 state/ready。
- Browser：C15，SHA256 473679d5c12d7dfeb057955ebd2ec0291cdd5db4654b6a3a2c2dba7c31fb995e。
- EKA SHA256：277db2b3e44c69d4b860b2f76cf194178e820556671ad32f4891fcae2cb4c2ff；不沿用旧 host 版本标签。
- Root 工具 session：任务未提供具体 ID；retained runner/validator 退出 0/0，原严格校验 PASS。
- 恰好一次 GET /chunked；落盘去掉 HTTP framing，为 524288 bytes 固定 payload。
- Config、profile、资源和专用目录恢复通过，原始逐项恢复记录在 state。
- 实用 supervisor SHA256：156029d6a868a8a9047e9f9ecd9f35e19b14bd0b271fe133689d5516802445c2。
  Controller SHA256：0e4d0892169fb4366d7e54f98e8b03d6d647e850caf6f27aab779e4994a635b0。
  按本轮 state 的自 hash 核对实际工具，未使用旧 GET02 driver 代替。

chunked-c15-01/ 保留全部本轮 fixture/命令/输入/截图/前后清单/实际测试 bytes/
validator/state 和原始 guest log。完整 .log 确定性 gzip 压缩；archive-manifest
记录原始及归档 SHA256，解压后逐字节验证。SHA256SUMS 覆盖其他全部文件。
不复制 previous 日志、原 config/profile 或 private backup。

tools/pinned-inputs/ 每项都与本轮实际 state/ready/input ledger 的 SHA 核对；
browser/host 只记录身份，不放入 Git。c15/ 保存候选源码/构建身份。
本例不继承到 CI41 或其他候选，也不替代其他下载 case 的独立验收。

## 图像与 FEP 身份

ui-identities.json 汇总本轮 PID、各 capture 的 main window ID、main/FEP 几何、
所有实际截图 SHA256 和输入 ledger 的目标窗口 ID；逐一核对原截图哈希。
FEP 标题/同 PID/transient-parent 由保存的原输入 helper 检查，原采证没有单独记录
raw title，不能补造。未获 observer 确认的控制请求单列，不能写成实际已执行。
最终 native_ui_reviewed 保持原值，不因归档时查看图片而改判。
