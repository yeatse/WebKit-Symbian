# C16 同名文件下载记录：PASS

本目录仅归档已结束的 existing-c16-02。原 state 与严格 validator 都为 **PASS**，
没有重写任何原始结果，也没有运行 guest、连接 display 或启动构建。
本轮唯一 token 是 dl-e-3917b1c007b2。

- Browser SHA256：654a620f3f94c6d130fb649dbd45f8f49d2142416c244ad59b1cb9a1738de37e。
- 本轮 EKA host SHA256：277db2b3e44c69d4b860b2f76cf194178e820556671ad32f4891fcae2cb4c2ff。
- Launcher/validator 退出码：0/0。
- 同名尝试后实际重新打开 Save，再真正取消；严格 validator 通过，原 sentinel 的 bytes/inode/mtime 保持相同。
- 专用 sentinel 的实际 bytes、SHA、大小、mtime、inode 和目录清单前后相同。
  profile/资源/目标目录与 config 恢复均为 true，恢复错误为空。

Root 确认实际序列为 Start → 目录 → FEP 输入 payload → OK → Replace Yes
→ 原生 Save 再次打开 → 真正 Cancel。新 guest log 有取消 terminal，严格
validator PASS；root 工具 session 82748 退出 0，config/profile/资源恢复成功。

本轮没有截图捕到短暂 refusal note，因此不声称拥有拒绝提示截图，也不据截图
缺失伪造提示文本。已观察并记录的是 Save 再次打开和之后真实取消；sentinel
bytes/inode/mtime 保持相同。本次 PASS 仅属于该 C16/本轮 host/token，不覆盖
C15 失败或 C16-01 timeout 失败记录。

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
