# 普通 password FEP 真 UI 候选

仅公开固定字符串：初值 `public-mask-check`、取消前草稿 `public-mask-draft`、提交值 `public-mask-edited`。没有凭据、auth 流程、clipboard、私有配置读取或 JS 注入。此目录是新候选，不改原 5 case 证据；当前未运行 guest、显示环境或服务器，不能标为 UI PASS。

2026-11-01 root 确认部署后，已只读核对 `pins.c19-host0029.json` 的 C19 browser、0029 host、secret FEP 三个文件 SHA256。driver 每阶段会重新核对，文件改变即拒绝输入。`pins.template.json` 仍为空模板，不沿用旧 host/FEP。host 的 DATA/patch 同步及实际加载身份沿用 root 部署证据，此 driver 不声称从进程内读取模块身份。

## root 启动步骤

1. 按正常独占 UI runner 流程，先启动新服务（此准备过程没有启动它）：

   ```bash
   python3 /workspace/symbian-fep-secret-runtime-work/server.py
   ```

   仅 loopback 8813，只有固定 index 页面，无目录浏览。每次运行使用新 token、独立输出目录，三例分别运行 `cancel`、`submit`、`empty`。

2. 复制 `calibration.template.json` 到本轮独立 `calibration.json`。模板 main `[900,700]` 与 content `[270,77,360,532]` 是历史参考，须先根据当前真实主窗口截图确认；确认后仅将 `main_reviewed_actual_screenshot` 设为 true，必要时校准尺寸/矩形。secret FEP 初次打开前保留其他 calibration 为 null/false。

3. 用正常 `run-browser.sh` 运行固定 C19。以下为 cancel 示例，目录名/token 必须新建，不复用失败输出：

   ```bash
   FEP_ROOT=/workspace/symbian-fep-secret-runtime-work
   FEP_TOKEN=secret-c19-h29-cancel-01
   FEP_CASE=cancel
   FEP_OUT=/workspace/symbian-fep-secret-runtime-work/run-c19-h29-cancel-01
   mkdir "$FEP_OUT"
   EKA2L1=/workspace/EKA2L1-validation/build/bin/eka2l1_qt \
   ICU_DIR=/workspace/symbian-idna-work/icu \
   SYMBIAN_WEBKIT_WORK="$FEP_OUT" WAIT=300 \
   EKA2L1_EXPECT_PASS="SECRET-FEP $FEP_TOKEN $FEP_CASE PAGE-PASS" \
   EKA2L1_COMPLETION_GRACE=5 \
   bash /workspace/WebKit-Symbian-graphics/Tools/Symbian/eka2l1/run-browser.sh \
     /workspace/symbian-belle-work/candidate19/browser.exe \
     "http://localhost:8813/?run=$FEP_TOKEN&case=$FEP_CASE" \
     >"$FEP_OUT/runner.log" 2>&1
   ```

   runner 应在独立受控终端/工具会话运行，保留 exit code；不要将 READY 当完成。此示例前台命令留在 runner 会话，后续 driver 从另一会话执行。fixture 从真实 Start 起限时 120 秒，首轮 root 校准须及时完成，超时保留 FAIL，用新 token 重跑。

4. 从正常 runner 确定本轮唯一 emulator PID，先 `open`。`PID` 是实际十进制 PID，不能照抄占位符。其余示例变量在 driver 会话重新设定为上面的同一值：

   ```bash
   python3 /workspace/symbian-fep-secret-runtime-work/drive.py \
     --phase open --pid PID --run secret-c19-h29-cancel-01 --case cancel \
     --guest-log /workspace/eka-validation/xdg/data/EKA2L1/data/drives/c/Data/WebKit/log.txt \
     --pins /workspace/symbian-fep-secret-runtime-work/pins.c19-host0029.json \
     --calibration /path/to/current-run/calibration.json \
     --output /workspace/symbian-fep-secret-runtime-work/run-c19-h29-cancel-01/input
   ```

   此阶段真实点击 Start，确认程序化 focus 没有弹 FEP，再真实点 password 字段；页面点击设原选区 `[1,4]`，随后平台打开 FEP。输出 `opened-fep.png`、`session.json`；FEP 保持打开供 root 观察。它只识别同 PID、属于主窗口的 `Enter the text` modal。

5. root 查看 `input/opened-fep.png`：应是单行密码编辑区，仅遮罩/选择高亮、Submit；不能出现 `public-mask-check` 明文。按实际 screenshot 与 `session.json.fep_geometry` 填 `fep_size`、`edit_point`（密码编辑区内）、`submit_point`（原生 Submit 按钮内），均为 FEP 窗口相对坐标；将 `fep_reviewed_actual_screenshot` 设为 true。**不能照搬旧 multiline 400×300 坐标。** 不改变已使用的 main calibration，不提前点窗口。

6. 使用完全相同参数，仅把 `--phase open` 改为 `--phase finish`。driver 拒绝换 PID、token、case、FEP 窗口、窗口几何、pins、fixture/driver 或重放同一次 finish。它真实点击密码编辑区，Ctrl+A、Backspace，再逐字符键入固定公开串，保存逐字即时帧和 settled 帧，然后真正 Escape / 点击 Submit。最后真实点击页面 Verify，只读验证 exact value/selection。

7. 等 runner 正常退出 0，留存原始完整 log、runner log 与 exit code。root 审查本轮 `opened-fep.png`、`selected-original-fep.png`、`cleared-fep.png`、全部 `typed-*-immediate-fep.png`、`typed-settled-fep.png`：初值与键入过程都只应显示遮罩，清空应空白；不能把原生窗口标题或测试页面自身公开文案当成密码泄露。截图是采样证据，不能单靠一张最终图片断言整个输入过程。结合 QLineEdit::Password 源/既有 widget 合同测试及这些实际画面给出遮罩结论。

8. 最终记录应明确分列：driver 退出 0、对应 PAGE-PASS、runner 正常退出 0、root 遮罩人工审图通过。driver 的状态始终是 `...PENDING_RUNNER_AND_MASK_REVIEW`，不自动宣称总体 PASS。任一步失败保留原输出，不改 fixture/断言，不在同 token 补做伪装成原轮 PASS。下一 case 用新 token/目录，可复用经过真实截图确认的相同布局 calibration；driver 仍逐轮验证尺寸。

## 三例预期

| case | 原生操作 | 页面必须精确成立 |
|---|---|---|
| cancel | 原值遮罩→全选清空→逐字输入公开 draft→真实 Escape | 原 `public-mask-check` 保留，选区 `[1,4]` 保留 |
| submit | 原值遮罩→全选清空→逐字输入公开 edited→点击原生 Submit | exact `public-mask-edited`，长度18，caret `[18,18]` |
| empty | 原值遮罩→全选清空→点击原生 Submit | exact 空串，选区 `[0,0]` |

日志只输出长度与选区。页面仍使用 `type=password`；Start、实际字段 tap、Verify 都必须是可信输入，driver 没有通过 DOM 改结果。公开固定字符串仅由 fixture 初始设置和真实 native 键盘输入使用。

## 准备阶段核验

`python3 check-contracts.py` 仅验证 pins 拒绝/匹配、实际校准准入、坐标边界、旧 token 隔离、FAIL 与重复 READY 不可吞掉，不连接 DISPLAY/HTTP/guest。Python 编译和 JS `node --check` 也只做语法检查。真实 UI、Cancel 选区恢复、Submit/empty、遮罩与正常关闭都仍待 root 运行。
