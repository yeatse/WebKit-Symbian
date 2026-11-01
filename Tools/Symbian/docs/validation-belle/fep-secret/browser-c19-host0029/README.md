# 普通 password FEP：C19 browser / host0029

2026-11-01，公开固定测试数据的三个普通浏览器 password FEP 用例均由 root 验收 PASS。真实点击密码字段打开原生单行密码编辑区；root 检查全部47张遮罩采样图（包括35张逐字即时帧），画面均为圆点遮罩或空白。实际 Escape、Submit、清空结果由页面精确断言；三例 driver/runner 均退出0，config 与七项受保护目录/资源全部恢复。

| case | 原生操作 | 精确页面结果 | 即时按键帧 | root 审查遮罩图 |
|---|---|---|---:|---:|
| cancel | 全选、清空、真实键入公开 draft、Escape | 初值保留，UTF16长度17，选区 `[1,4]` | 17 | 21 |
| submit | 全选、清空、真实键入公开 edited、原生 Submit | exact replacement，UTF16长度18，caret `[18,18]` | 18 | 22 |
| empty | 全选、清空、原生 Submit | exact 空串，caret `[0,0]` | 0 | 4 |

初值 `public-mask-check`、取消前 draft `public-mask-draft`、提交值 `public-mask-edited` 均为公开固定非凭据测试字符串。日志只记录长度/选区；本次没有 auth、实际账户密码或 clipboard 数据。

每例原始 `result.json`、`input/open-result.json`、`input/interaction.json` 保留当时的 pending 状态和原始字节。root 人工审图与最终验收单独记录在各例 `acceptance.json`；归档没有修改 raw 状态。`summary.json` 是对这些原始文件、截图哈希、日志与恢复结果的独立复核结果。

遮罩结论来自 root 的逐图人工审查；`verify.py` 只确认图片身份、审查覆盖、精确事件结果和恢复元数据，不替代像素审查。这里保存的是即时/settled 截图采样，不能扩展为任意时刻连续视频证明、其他 frontend 的行为或真实 Nokia 硬件验证。真实手机与 Android/iOS 不在此验收范围。C20 CookieStore 或其他候选功能不属于此 C19 结果。

## 固定身份

| 输入 | SHA256 |
|---|---|
| C19 browser.exe | `6b6e8e0a430ff9c1ce0be935161d4b7e61804444e2947cb0218e74cb2e5420ae` |
| host0029 eka2l1_qt | `07af513045bf26e200f7d711c682e219ee4c7cecc9e30f2fa3b08732fc1012e3` |
| secret avkonfep_general.dll | `fd6092f2ad61d39ccb47475229c367f5494fe68a8bd2bd8d0e04dc1bafcdc7ae` |
| C19 browser_ui.rsc | `251cfbede521cc5d0a3e36400cf108b12b6bb8eb7956e33a6781d8ef4bf5bc3d` |
| public index.html | `2277d7851bb9752895a51e6bd7bb6a721ba753fcb94184065a4d2aa26de02a6b` |
| two-phase drive.py | `0252e03ca0799b0c3aea27d23018b0b68e393b69b3ac885aff575ada103f90a5` |
| native-interact.py | `3b31229833fc4544cdd17b1a6357e766bb1de40c165e2efa4f7452e41947b2f0` |

各例 wrapper `result.inputs`、open/session/finish pins 与源码身份互相一致。实际主窗口900×700、内容360×532、secret FEP布局以每例实际 `calibration.json` 与截图为准。本归档没有复制大型 browser/host 二进制，以 pins 和运行报告记录身份。

## 目录与核验

- `fixture/`：实际 fixture、两阶段真实输入 driver、固定8813服务、root `run-preserved.py`、准备期合同检查及 pins/calibration 模板。`PREPARATION-README.md` 是运行前原始说明，其“尚未执行”状态仅描述当时准备阶段；最终结果见本说明及三例 acceptance。
- `harness/`：native input helper、wrapper 公开依赖 `runtime_support.py`、runner/正常关闭脚本及 common.sh 快照。原脚本中的绝对路径保留供审查；复制此目录不会自动启动任何服务。除 native helper 的 run-recorded hash 外，其余通用依赖是归档时参考快照，原 wrapper 没有逐个记录 run-specific hash。
- `cases/{cancel,submit,empty}/`：root acceptance、raw reports、实际 calibration、open/finish/runner日志、所有53张输入截图、三例原始浏览器日志与 gzip 压缩 emulator 原始日志；cancel 另含1张主窗口校准图。合计54张公开截图。
- `archive-sources.json`：103个原文件的来源路径、原始字节哈希及归档哈希。emulator.log 以确定性 gzip 保存，解压内容的哈希与原件一致。其余文件逐字节复制。
- `SHA256SUMS`：完整候选归档文件的哈希；`summary.json`、`verify.py` 为此次新增复核产物。

在此目录执行，完全不访问原工作区、guest、DISPLAY 或网络：

```bash
sha256sum -c SHA256SUMS
python3 verify.py
```

每例 raw browser log 都恰好包含本轮 `READY → ARMED → FIELD-TAP → PAGE-PASS`；Finish 对 password 类型、可信实际字段 tap、精确值及 selection/caret 作断言。driver open/finish 图片及 acceptance 的47张审查图片哈希均被核对；Cancel/Submit/empty 即时帧分别17/18/0。三例正常关闭由 runner log 的完成记录、wrapper launcher_exit0 和各自 acceptance runner_exit0支撑。

恢复只复核已生成报告：每例 `config_restored=true`，`restoration.restored=true`，七项受保护项均 `restored=true` 且 before/after 完全一致，包括 Data/WebKit 原目录的 inode 元数据。config 具体内容、用户 profile、private 文件、previous 日志、originals 备份及任何凭据均未读取或复制。raw result 中的 private/backup 路径字符串和资源哈希只是恢复元数据，保留用于审计，不包含这些文件内容。

新归档没有启动 guest/display/server，没有写共享源，未修改原五例普通 FEP 证据或本次三例 raw 结果。后续将整个 `validation-belle/fep-secret/browser-c19-host0029` 目录复制到仓库文档对应路径即可。
