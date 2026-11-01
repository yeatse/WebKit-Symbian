# C17 传输取消未通过记录

本目录仅归档已经结束的 cancel-transfer-c17-01，run token 为
dl-x-2b6107823e76。原 state 与严格 validator 都是 **FAIL**，保持原判。
Root observer 工具 session 18274 退出 1；launcher/validator 也均退出 1。

- C17 browser SHA256：0d91d91ea1e016b7725097bac0768a9aa2fdf9710b765795769d2da07608a1c8。
- 本轮 EKA host SHA256：277db2b3e44c69d4b860b2f76cf194178e820556671ad32f4891fcae2cb4c2ff。
- browser_ui.rsc 为 242 bytes，精确 SHA 见 state/tool-identities，未混用旧 UI。
- 新 guest log 有 8192 bytes 正 progress，之后是 terminal=4/error=28/cleanup=0，
  不是预期的 terminal=1 用户取消结果。
- Root 记录：Options 点击没有打开可见菜单；F2 只隐藏标签，未出现可见菜单。
  本轮没有真正选择 Cancel download，不能写成 Options/取消流程成功。
- 原 validator 缺 transfer-cancel、fixture release/收尾、实际 UI 最终审阅和
  selected path，并报告 terminal 不符；所有原错误/缺失保留。
- 专用目录前后为空，原 profile/资源/目标目录及 config 恢复成功、错误为空。
  清理成功不能代替实际用户取消通过。

Root 另一次 about:blank 的 C17 普通菜单可见观察属于不同运行，不使本轮通过，
本目录不读取、重跑或归档那次运行。本记录不据错误 28 猜测其他内部原因。

## 完整证据与身份

run/ 原样保留该轮所有 guest log、fixture/命令/输入/计划、main/FEP 截图、
before/after、state、runner report 和原 validator。完整 .log 用确定性 gzip
保存，没有裁剪；previous logs 和原 config/profile 私有文件不复制。

ui-identities.json 保留每份校准/输入的 PID、main window ID、main/FEP 尺寸、
每张图的 SHA 与 geometry、实际 action 的目标和 window ID。没有记录的宿主
窗口标题明确为 null，不把画面上的 dialog 文本虚构成 title。

tool-identities.json 按 state/ready/输入 ledger 的精确 SHA 查找当前源或限定
工具快照，只复制匹配内容。缺失准确工具版本：
无（所有记录输入身份均找到精确匹配）。
大体积 browser/host 只核对身份，小资源和工具保留原字节。

原 report 的 host_exit_code 为 null、normal_close 为 false、forced_cleanup
为 null，保持原值，不改写成正常退出或已证实强制清理。
archive-manifest.json 同时保存源和归档长度/SHA；SHA256SUMS 覆盖所有其他文件。
全部图片 ledger、gzip 解压原字节和文件 hash 已核验，原始 run 文件未变化。
本归档没有启动 guest/display/构建、修改共享源码或 live 工具。
