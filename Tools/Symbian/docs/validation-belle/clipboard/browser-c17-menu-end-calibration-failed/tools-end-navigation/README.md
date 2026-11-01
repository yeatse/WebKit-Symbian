# C17 clipboard 菜单真实操作准备

本目录只准备外部文件。没有启动 guest、连接 display、读取原剪贴板或修改共享源码。`preparation.json` 是只读身份核对，Python只做AST检查，均不代表本轮功能验收。

固定C17 browser `0d91d91e…`、host `277db2b3…`、FEP `aad1a4c6…`，UI resource必须是C17 242 bytes / `251cfbed…`。

## 候选导航及校准边界

`BrowserApp.cpp::ShowMenuL` 把已启用的 Cut/Copy/Paste/Select all 按此顺序追加，最后是Exit。Bookmarks/History/Forward/下载项都在编辑项之前，因此从底部导航不依赖这些前项数量。

| 操作 | Options打开后的候选导航 | 必须实际确认的tail |
| --- | --- | --- |
| Paste | End, Up, Up, Return | Paste, Select all, Exit |
| Copy | End, Up×3, Return | Copy, Paste, Select all, Exit |
| Cut | End, Up×4, Return | Cut, Copy, Paste, Select all, Exit |
| Select all | End, Up, Return | Select all, Exit |

**原webkit-editor没有Home/End绑定，不能直接使用上表。** 本目录单独的opt-in `webkit-menu-review.yml`保留原全部映射，追加Qt Key_Home=16777232→SDK EStdKeyHome=0x08、Key_End=16777233→EStdKeyEnd=0x09；官方头文件出处见profile-provenance.json。wrapper私下安装此profile并恢复原config/profile，不改default.yml。绑定成立也不证明popup处理End：root仍须看实际End图片是否选到Exit；若无效，不批准Return，保留本轮INCOMPLETE/FAIL。

root已实际确认F2打开Options、Cancel关闭和Reload选择。但about:blank截图的Copy/Select all来自地址栏，不能当作网页编辑字段的tail校准。每次native命令必须保持页面已经建立的字段选择，不能另点地址栏/字段。

`review-drive.py` 每个native操作先发送F2/End/Up，在Return之前停下，保存 `opened.png`、`end.png`、`selected.png`、`gate.json`。root逐次实际查看菜单，确认End选Exit、Up选期望标签、没有FEP和目标字段误切换后，用匹配该次图片SHA的approval允许Return。任何未知画面都不能按预期标签盲批。每次审核32秒上限，保持在页面45秒原生命令期限内；完整driver仍受450秒期限和原9例断言约束。

没有Ctrl快捷键、DOM注入、字段重选、合成clipboard event或store写入捷径。第4例DOM paste denial完全保留。

## Root执行

先确认root独占EKA，Xvfb/Pulse正常，localhost:8802现有fixture server运行。本wrapper不启动服务器。建议先fresh token做第一例Paste校准：

```sh
. /workspace/eka-validation/env.sh
. /workspace/symbian-media-work/env.sh
python3 /workspace/symbian-clipboard-menu-c17-work/run-c17.py \
  --execute --through-step 1 \
  --run clipboard-menu-c17-cal-20261101-01 \
  --output /workspace/symbian-belle-work/p1-validation/clipboard-menu-c17-cal-20261101-01
```

driver自动真实点击Start并到菜单审核点。读 `OUTPUT/input-driver.log` 的 `REVIEW .../gate.json expected=Paste`，用图片工具查看gate三张PNG；确认实际选中Paste后：

```sh
python3 /workspace/symbian-clipboard-menu-c17-work/approve.py \
  --gate /absolute/OUTPUT/input/menu-reviews/01-step1-paste/gate.json \
  --label Paste --selected-sha256 ACTUAL_SHA256_FROM_THIS_GATE
```

SHA必须来自已查看的本次gate。只校准第一例最多是 `CALIBRATION_PARTIAL`，不能拼接后续片段冒充一次9例PASS。

完整9例使用另一个fresh token，省略through-step：

```sh
. /workspace/eka-validation/env.sh
. /workspace/symbian-media-work/env.sh
python3 /workspace/symbian-clipboard-menu-c17-work/run-c17.py \
  --execute --run clipboard-menu-c17-20261101-01 \
  --output /workspace/symbian-belle-work/p1-validation/clipboard-menu-c17-20261101-01
```

整轮共12个菜单操作，每次按gate图审核；Copy/Cut/Select all第一次出现时特别核实实际tail/高亮。approve标签必须逐次为Copy/Cut/Paste/Select all（最后一个含空格）。根driver所有ACTION、真实事件、精确UTF-16、HTTP report契约保留，随后调用原verify.py。只有9例driver、原校验器、标准runner正常退出和文件恢复全过才写PASS。普通Qt close证明此轮host退出，不是官方EndTask生命周期证据。

## 保存与恢复

执行前持jsc/browser两锁，整套C:/Data/WebKit profile和会覆盖的browser/resources/ICU/fontconfig通过既有SavedFiles私下rename隔离；另外保存原config、webkit-menu-review profile、D:/System/Data/Clpboard.cbd。原config在临时副本运行，原clipboard被私下保存，fixture只使用自己生成的测试文本。最终检查原inode、SHA、mode、mtime/atime、文件及clipboard父目录原不存在状态。

失败后仅正常关闭自己启动的Qt窗口；若仍有活guest，不覆盖文件，保留private backup/journal。`OUTPUT/private/`不可公开归档。default不加execute只读准备，不写guest/profile/clipboard。

本工具尚未运行真实菜单验收；初始源码检查不能替代root实际校准和恢复验证。
