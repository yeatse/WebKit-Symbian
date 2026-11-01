# 普通下载：C11 后的实际浏览器验收

本目录准备本机 fixture、真实 XTest 输入录制、只读文件快照和离线校验器。
准备阶段没有启动 EKA、连接其 X display、写入 guest 目录或修改生产源码。
实际执行由 root 在 C11 完成后，沿用正常 runner 的独占锁、启动与正常关闭流程进行。
当前代码中 FrameLoader/WebContainer/DownloadUI 已接线；仓库 DOWNLOADS.md 的
“未接线”文字已滞后，本次不改冻结中的树。

## 七例及顺序

每例用新的 run token、专用用户目录、证据目录和一次正常浏览器运行。

|case|真正的原生操作|必需结果|
|---|---|---|
|post|点击 HTML POST 表单并保存|`/post-attachment` 总请求恰好1次、POST、正文精确 `token=post-once`；落盘524288字节及正确SHA256；terminal=0|
|get|普通附件导航并保存|一次GET `/attachment`；相同大小/hash；terminal=0|
|gzip|普通gzip附件并保存|一次GET `/gzip`；落盘是解压后的相同payload；terminal=0|
|chunked|普通分块附件并保存|一次GET `/chunked`；落盘去掉HTTP framing后的相同payload；terminal=0|
|cancel-save|真正保存对话框取消|一次请求；terminal=1,error=0,cleanup=0；整个专用目录不变|
|cancel-transfer|保存后已收到正数字节，Options → Cancel download|同ID正progress和terminal=1,error=0,cleanup=0；目标不存在且无临时文件；fixture完成收尾|
|existing|尝试同名保存，看见拒绝，然后真正取消对话框|拒绝提示截图；原文件内容/hash/大小/mtime/inode及目录均不变；terminal=1|

`existing` 须预放与下载payload**不同内容**的小sentinel `payload.bin`。相同内容不能证明
没有覆盖。不把点击、服务器sent或Saved提示单独当作落盘通过。本轮不访问旧fixture的
协议拒绝、超限、截断或异常名字端点。

## Fixture与文件准备

```sh
export DOWNLOAD_WORK=/workspace/symbian-download-runtime-work
export DOWNLOAD_CASE=post
export DOWNLOAD_RUN="dl-${DOWNLOAD_CASE}-$(date -u +%Y%m%dT%H%M%SZ)"
export DOWNLOAD_EVIDENCE="$DOWNLOAD_WORK/runs/$DOWNLOAD_RUN"
mkdir -p "$DOWNLOAD_EVIDENCE"
PYTHONDONTWRITEBYTECODE=1 python3 "$DOWNLOAD_WORK/fixture.py" \
  --run "$DOWNLOAD_RUN" --case "$DOWNLOAD_CASE" --port 8800 \
  --output "$DOWNLOAD_EVIDENCE/fixture" > "$DOWNLOAD_EVIDENCE/fixture.log" 2>&1 &
DOWNLOAD_FIXTURE_PID=$!
```

`fixture/ready.json` 给出URL、payload身份和复用的仓库server.py身份。wrapper继承原HTTP
handler/payload，只提供本例一个入口。不要curl/wget预取下载端点，否则恰好一次检查失败。
`cancel-transfer` 使用普通慢响应：发8192字节后等待 `/release`，默认180秒，可加
`--gate-seconds 300`。原 `/slow` 约6.4秒，容易在原生选择完成前已发完，无法可靠测试取消。

root在独占emulator数据目录时、guest启动前创建可浏览的专用普通用户目录，例如
`C:\Data\DownloadRuntime\<run>\`，确认实际host映射后设 `DOWNLOAD_DEST_DIR` 和
`DOWNLOAD_GUEST_DIR`。existing仅在该新目录放sentinel；其他案例目标先不存在。
不要运行中替换产物或触及其他用户文件。先记录目录及原文件：

```sh
python3 "$DOWNLOAD_WORK/snapshot.py" \
  --directory "$DOWNLOAD_DEST_DIR" --guest-directory "$DOWNLOAD_GUEST_DIR" \
  --run "$DOWNLOAD_RUN" --case "$DOWNLOAD_CASE" \
  --output "$DOWNLOAD_EVIDENCE/before.json" --copy-to "$DOWNLOAD_EVIDENCE/before-files"
```

## 正常runner与真实输入

`DOWNLOAD_BROWSER_EXE` 必须是root确认的C11 **E32 browser.exe**，相邻browser_ui.rsc同构建；
不能误用browser.elf或旧产物。保持root已有EKA2L1、DISPLAY、XDG、ICU_DIR等环境。
本目录不另起guest/Xvfb或另一把锁。用root异步工具会话运行正常runner，同时操作UI：

```sh
DOWNLOAD_URL=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["url"])' \
  "$DOWNLOAD_EVIDENCE/fixture/ready.json")
WAIT=300 EKA2L1_EXPECT_PASS="READY: belle-download-runtime" \
  /workspace/WebKit-Symbian-graphics/Tools/Symbian/eka2l1/run-browser.sh \
  "$DOWNLOAD_BROWSER_EXE" "$DOWNLOAD_URL" > "$DOWNLOAD_EVIDENCE/runner.log" 2>&1
```

保留实际runner退出码和它打印的日志目录。若首次校准耗尽时间，保留INCOMPLETE，用新run
重试；不要中断runner来制造通过。root从自己的运行会话取得精确EKA_PID，然后只capture：

```sh
python3 "$DOWNLOAD_WORK/native.py" capture --pid "$EKA_PID" \
  --run "$DOWNLOAD_RUN" --case "$DOWNLOAD_CASE" --output "$DOWNLOAD_EVIDENCE/cal-page"
```

**必须先用图片工具查看** `cal-page/calibration-main.png`，若有则查看 `calibration-fep.png`。
所有point相对各自窗口；不使用桌面坐标。本次没有提供数字坐标或虚构已成功菜单序列。
F1/F2也需确认本次键位和焦点。按实际所见创建plan：

```json
{
  "calibration_sha256": "SHA256_OF_ACTUAL_CALIBRATION_JSON",
  "screenshots_reviewed": true,
  "actions": [
    {"target": "main", "role": "start", "point": ["ACTUAL_X", "ACTUAL_Y"], "wait": 1.0}
  ]
}
```

上面是结构示意，字符串坐标会被拒绝；必须换成当前截图量得的整数。用sha256sum取得
实际calibration.json哈希，然后执行：

```sh
python3 "$DOWNLOAD_WORK/native.py" act --pid "$EKA_PID" \
  --run "$DOWNLOAD_RUN" --case "$DOWNLOAD_CASE" \
  --calibration "$DOWNLOAD_EVIDENCE/cal-page/calibration.json" \
  --plan "$DOWNLOAD_EVIDENCE/start-plan.json" --output "$DOWNLOAD_EVIDENCE/input-start"
```

native.py复用仓库native-interact.py的XInput，同PID/窗口尺寸/焦点/FEP身份检查仍有效，
逐步保存输入前后图片/hash。没有JS注入、替换选择结果或guest文件写入。
保存对话框、路径编辑、同名拒绝和Options菜单均应重新capture/查看后建立下一段plan。
resize、缺FEP或未校准FEP会停止，必须重校准。操作允许point、`key:"Return"`、
`text:"payload.bin"`；key可配`modifier:"Control_L"`/`"Shift_L"`。text只输入普通ASCII测试名。
若需FEP改名：点击真实输入框，Control_L+a、BackSpace、text，再点击真实Submit。
FEP Submit不等于原生Save/Cancel；还需完成下面AVKON对话框。

角色：首次网页点击`start`（恰好一次）；文件夹导航`native-step`；改名`edit-name`；
真正Save=`save-accept`；保存取消=`save-cancel`；Options取消=`transfer-cancel`；
同名尝试=`existing-attempt`；确认拒绝后最终取消=`existing-dismiss`；中间提示关闭=`dismiss-note`。
每段状态仅INPUT_RECORDED，校验时用重复`--interaction`传入全部段。

## 取消收尾与实际落盘

cancel-transfer先在当前guest log确认同ID `download ID: N/-1 bytes`且N>0、fixture有
gate-wait，再查看Options截图并点击Cancel download。收到同ID terminal=1之后才释放：

```sh
curl --noproxy '*' --fail --silent --show-error --request POST \
  "http://127.0.0.1:8800/release?run=$DOWNLOAD_RUN"
```

保留release-requested和最终sent/disconnected。socket buffering可能让server最终sent，
不能据此单独否定取消；gate-timeout则失败。保存对话框取消不等于传输取消。
各例保存原生Saved/Cancelled/Already exists截图，等正常runner关闭刷新VFS后再采最终文件：

```sh
python3 "$DOWNLOAD_WORK/snapshot.py" \
  --directory "$DOWNLOAD_DEST_DIR" --guest-directory "$DOWNLOAD_GUEST_DIR" \
  --run "$DOWNLOAD_RUN" --case "$DOWNLOAD_CASE" \
  --output "$DOWNLOAD_EVIDENCE/after.json" --copy-to "$DOWNLOAD_EVIDENCE/after-files"
```

保留after-files实际bytes、前后目录清单、原始guest/emulator日志、runner输出/退出码、
全部截图和fixture日志。不要仅摘一条terminal或手改日志。

## 完整结果格式和校验

runner-report.template.json故意不预填成功值。root从实际运行生成runner-report.json：
run/case/url精确匹配；真实host_exit_code=0、normal_close=true、forced_cleanup=false；
实际browser.exe/emulator SHA256；完成UTC；native_ui_reviewed=true。除cancel-save外，
selected_guest_path必须是截图确认的guest_directory+目标名。

```sh
python3 "$DOWNLOAD_WORK/validate.py" \
  --fixture "$DOWNLOAD_EVIDENCE/fixture" \
  --before "$DOWNLOAD_EVIDENCE/before.json" --after "$DOWNLOAD_EVIDENCE/after.json" \
  --destination-name payload.bin \
  --interaction "$DOWNLOAD_EVIDENCE/input-start/interaction.json" \
  --interaction "$DOWNLOAD_EVIDENCE/input-save/interaction.json" \
  --guest-log "$ACTUAL_RUNNER_LOG_DIR/log.txt" \
  --emulator-log "$ACTUAL_RUNNER_LOG_DIR/emulator.log" \
  --runner-report "$DOWNLOAD_EVIDENCE/runner-report.json" > "$DOWNLOAD_EVIDENCE/result.json"
```

按实际段数调整interaction参数，destination-name须是真正选择的basename。
退出码0/1/2分别PASS/FAIL/INCOMPLETE。缺图片、完整日志、runner身份或UI人工审阅不能完整PASS。
七例独立报告均PASS才报告七项通过，未执行项保留NOT RUN。采证完成只停止本例fixture PID，
保留证据目录，不清理其他数据。

## 准备阶段host检查

```sh
PYTHONDONTWRITEBYTECODE=1 python3 /workspace/symbian-download-runtime-work/test_tools.py -v
PYTHONDONTWRITEBYTECODE=1 python3 /workspace/symbian-download-runtime-work/test_validate.py -v
```

这只覆盖loopback实际bytes/gzip/chunking/POST记录/门控、只读快照、计划校验、证据校验器
正反例；没有连接X11或执行Symbian UI，不是guest通过记录。
