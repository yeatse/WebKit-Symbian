# C17 原生菜单剪贴板：9/9 PASS

2026-11-01（Asia/Shanghai），真实 run `clipboard-menu-c17-20261101-01`。
输入 driver、标准 launcher、页面/report validator 均为0，root wrapper session73903退出0；原profile/resources及private config/profile/clipboard全部恢复。

每例Start为真实指针点击；12个编辑命令各通过F2打开实际原生菜单，root逐张查看当次opened PNG，核对实际命令行并批准绑定PNG SHA/PID/window/geometry的点，再真实点击。记录仅有12次F2，没有End/Up/Return或Ctrl捷径。case9 Select all的位置随动态菜单改变为y420，未复用其他case行号。

覆盖Unicode多行复制、部分剪切、contenteditable纯文本、DOM paste权限拒绝、password/readonly保护、菜单Copy/Cut/Select all及精确目标选择区插入。9次Start、12个菜单命令、9项页面结果与唯一HTTP报告一致。

身份严格绑定C17 `0d91d91e…`、242-byte resource `251cfbed…`、host `277db2b3…`、FEP `aad1a4c6…`。这不是CI41运行或C15 keyboard结果；原先失败的End导航校准另存相邻browser-c17-menu-end-calibration-failed，未重标成功。

run/保留完整原始输入、12张图/gate/approval、guest log、压缩host/runner日志和恢复结果；fixture/、tools/与candidate17/记录输入源码和身份。未复制private备份、原配置/剪贴板内容、previous originals或EXE/DLL。

正常Qt close与launcher0证明本次host退出，不替代官方EndTask生命周期验收，也没有新增真机结论。所有源运行记录保持原样；SHA256SUMS及archive-verification.json仅核对归档完整性。
