# C17 clipboard：每次原生菜单点击前看图

本目录是外部输入与保存工具。没有改共享源码。最新方式只发送 **F2 打开 Options → 保存本次一张菜单图 → root 审核期望命令行的实际坐标 → 真实点击该行**。没有 End/Up/Return 导航，也没有 Ctrl 快捷键、DOM注入、字段重选或合成clipboard event。原9例ACTION、trusted事件、UTF-16精确内容和HTTP report断言保留；driver和页面时限未延长。

固定C17 browser `0d91d91e…`、host `277db2b3…`、FEP `aad1a4c6…`，C17 UI resource为242 bytes / `251cfbed…`。wrapper使用已建立的opt-in webkit-editor profile，不依赖Home/End。

## 保留的失败校准

第1轮 `clipboard-menu-c17-cal-20261101-01` 真实打开了包含编辑命令的菜单，但End没有形成预期选择，两次Up选中了Exit而非Paste。root未批准，也未发送Return。该轮仍保持失败/不完整，原始记录未改。此前源码推导的End/Up方案停止使用；对应工具源码快照保存在 `versions/end-navigation/`，用于解释旧运行哈希，不能再据此批准命令。

## 新校准流程

确认root独占EKA，Xvfb/Pulse正常，localhost:8802原fixture server运行。wrapper不启动服务器。先用fresh token只做第一例Paste：

```sh
. /workspace/eka-validation/env.sh
. /workspace/symbian-media-work/env.sh
python3 /workspace/symbian-clipboard-menu-c17-work/run-c17.py \
  --execute --through-step 1 \
  --run clipboard-menu-c17-cal-20261101-02 \
  --output /workspace/symbian-belle-work/p1-validation/clipboard-menu-c17-cal-20261101-02
```

`input-driver.log` 出现 `REVIEW .../gate.json expected=Paste pid=...` 时，root用图片工具查看该gate的 `opened.png`，找到真实Paste行，测量**相对这张主窗口图片**的点。不能沿用旧run的行号/坐标。然后批准本次图、PID和坐标：

```sh
python3 /workspace/symbian-clipboard-menu-c17-work/approve.py \
  --gate /absolute/OUTPUT/input/menu-reviews/01-step1-paste/gate.json \
  --label Paste --opened-sha256 ACTUAL_SHA_FROM_THIS_GATE \
  --pid ACTUAL_PID_FROM_THIS_GATE --point ACTUAL_X ACTUAL_Y
```

approve文件同时绑定run、step、operation、标签、该PNG SHA、PID、window ID和完整geometry。driver在点击前重新检查samePID/focus/geometry、FEP不存在、PNG哈希和本轮guest日志。坐标越界、浮点/布尔坐标、PID/窗口/图/标签/geometry不符都会拒绝。approve.py只写一次审批文件；driver之后才调用既有XTest `click_window`，不修改页面选区。

每个gate仍只允许32秒审核，完整driver仍450秒，页面每个native命令仍45秒。若菜单项不可见、画面未知或未及时审核，保留本轮不完整/失败，用新token重新校准；不点猜测的命令。旧approval不能用到新gate。

第一组通过只算 `CALIBRATION_PARTIAL`，不能拼接后续case冒充一次9例PASS。

## 完整9例

使用新的token/目录，省略through-step：

```sh
. /workspace/eka-validation/env.sh
. /workspace/symbian-media-work/env.sh
python3 /workspace/symbian-clipboard-menu-c17-work/run-c17.py \
  --execute --run clipboard-menu-c17-20261101-02 \
  --output /workspace/symbian-belle-work/p1-validation/clipboard-menu-c17-20261101-02
```

共12个原生菜单操作、每次一张当前菜单图和一个root审核点。四种标签严格为 `Copy`、`Cut`、`Paste`、`Select all`。菜单编辑项按canExecuteEditCommand动态出现，因此每次都看本次图，不能按固定索引点击。无需键盘导航到末项。

完整driver成功后，wrapper等待标准runner正常关闭，再调用原verify.py。9例driver、原校验器、runner和文件恢复全部通过才写PASS。正常Qt close只证明本次host退出，不替代官方EndTask生命周期验收。

## 原状态保存

wrapper持jsc/browser两锁。整套C:/Data/WebKit profile和被覆盖的browser/resources/ICU/fontconfig由SavedFiles私下rename隔离；原config、已有webkit-editor profile、D:/System/Data/Clpboard.cbd另保存，原config只在临时副本运行。原clipboard不会被复制进测试页面或报告。最后检查原inode、SHA、mode、mtime/atime、文件和clipboard父目录原不存在状态。

任何失败均尝试正常关闭自己的Qt主窗口。若仍有活guest，保留private backups/journal，不覆盖运行中的资源；若某项恢复失败，仍继续尝试恢复其他项目并记录失败。不得公开归档OUTPUT/private。

default不加execute只读核对产物身份。最新point方式只完成AST和纯guard检查，未由本代理连接display或运行guest；真实功能验收由root执行。
