# C16 同名文件下载记录：FAIL

本目录仅归档已结束的 existing-c16-01。原 state 与严格 validator 都为 **FAIL**，
没有重写任何原始结果，也没有运行 guest、连接 display 或启动构建。
本轮唯一 token 是 dl-e-01238fe7825b。

- Browser SHA256：654a620f3f94c6d130fb649dbd45f8f49d2142416c244ad59b1cb9a1738de37e。
- 本轮 EKA host SHA256：277db2b3e44c69d4b860b2f76cf194178e820556671ad32f4891fcae2cb4c2ff。
- Launcher/validator 退出码：1/1。
- 300 秒观察期限内操作未完成，之后控制命令报告 browser already ended；没有实际取消，也没有 terminal，属于本轮校准/操作未完成的边界。
- 专用 sentinel 的实际 bytes、SHA、大小、mtime、inode 和目录清单前后相同。
  profile/资源/目标目录与 config 恢复均为 true，恢复错误为空。

这次到达 300 秒期限时，原生同名尝试和实际取消尚未完成；之后控制命令
报告 browser already ended。原 validator 缺 existing-attempt、existing-dismiss
和 terminal 等证据。保留原 FAIL，并限定为本轮校准/操作未完成，不据此另报生产
代码缺陷，也不存在真实取消成功的证据。原 runner report 的 host_exit_code 为
null、normal_close 为 false、forced_cleanup 为 null，保持原值。

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
