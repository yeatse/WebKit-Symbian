# C15 同名文件下载记录：FAIL

本目录仅归档已结束的 existing-c15-01。原 state 与严格 validator 都为 **FAIL**，
没有重写任何原始结果，也没有运行 guest、连接 display 或启动构建。
本轮唯一 token 是 dl-e-96cb1c9bde37。

- Browser SHA256：473679d5c12d7dfeb057955ebd2ec0291cdd5db4654b6a3a2c2dba7c31fb995e。
- 本轮 EKA host SHA256：277db2b3e44c69d4b860b2f76cf194178e820556671ad32f4891fcae2cb4c2ff。
- Launcher/validator 退出码：1/1。
- 原生保存对话框报告 -11，terminal=5/error=-11/cleanup=0；不满足原定同名拒绝后重新选择/取消的验收约定。
- 专用 sentinel 的实际 bytes、SHA、大小、mtime、inode 和目录清单前后相同。
  profile/资源/目标目录与 config 恢复均为 true，恢复错误为空。

原始 guest 记录 download save dialog failed: -11，以及
terminal=5/error=-11/cleanup=0，这不是期望的 terminal=1 取消结果。
原 operator-observation.json 是当时操作员描述，原样保留；主文仅据可核实日志/
report 记录失败，不将 -11 单独归因到某一 backend 内部步骤，也不把系统覆盖
确认、原生同名拒绝、重试取消流程写成成功。文件未变不能代替完整 UI 验收。
原 runner report 的 host_exit_code 为 null、normal_close 为 false、
forced_cleanup 为 null，不将其改写为正常退出或已证实强制清理。

## 原始证据与身份

run/ 保留该轮全部文件：原始 guest log、输入/计划/命令、fixture、main/FEP
校准与前后截图、实际 sentinel bytes、before/after、state、runner report、
原 validator。所有 .log 用确定性 gzip 保存完整原字节，没有裁剪或改判。
原 config/profile 私有文件与 previous logs 不复制。

ui-identities.json 从原 JSON 提取 PID、main window ID、main/FEP 尺寸、
每张图片实际 SHA 与 geometry、每次实际 action 的目标/窗口 ID。
原记录未包含的宿主 window title 明确为 null，不把图上 dialog 文本虚构为标题。

tool-identities.json 按 state/ready/输入 ledger 的精确 SHA 查找当前路径或
限定的已存工具快照，只复制匹配内容。缺失的准确版本：
无（所有记录输入身份都找到精确匹配）。
大体积 browser/host 只核对身份，不复制进 Git；小资源与工具原字节保留。

archive-manifest.json 同时保存源与归档长度/SHA；压缩日志保留源 hash。
SHA256SUMS 覆盖所有其他文件。归档核验了全部图片 ledger hash、gzip 原字节、
所有文件 hash，并确认原 run 文件未变化。
