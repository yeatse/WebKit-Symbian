# C13：七组真实单指触摸验收

2026-11-01（Asia/Shanghai），root 固定 EKA host 的身份，以普通浏览器 runner 和真实 XTest
完成七次独立启动；本目录整理原始证据，没有重新运行 guest。C13 七例 input/launcher 均退出 0，
外层 profile guard 的 child_exit=0，config 与 profile 原有状态均已恢复。

| case | 结果与主要约束 |
| --- | --- |
| prevent-start | PASS；touchstart.preventDefault 抑制兼容 mouse/click |
| prevent-move | PASS；touchmove.preventDefault 保留 DOM 流并禁止滚动 |
| pan | PASS；实际 overflow 滚动超过 5px，touchcancel/pointercancel 各一次，无结束/click |
| pointer-pan | PASS；pointerdown.preventDefault 仍允许原生滚动 |
| none | PASS；touch-action:none 保留 DOM 流并禁止原生滚动 |
| capture | PASS；拖出面板后捕获保留投递，got/lost capture 与释放正确 |
| navigation | PASS；持有原生触点时导航，旧页取消完成，新页检查 sessionStorage 证据 |

本轮只用了 **一个真实原生触点**。maxTouchPoints=8 是设备报告的容量；没有验证物理多指、
独立 native focus-loss cancel 或真机行为。历史 tap 单独记录，不把它计入这七例。

## 产物身份与失败基线

| 轮次 | browser.exe SHA256 | host SHA256 | 实际结果 |
| --- | --- | --- | --- |
| C11 stable viewport | `20f58cef43d6fa4252546cb337940bfa609ba7ef71d0059ef1336bd126ef5288` | `0d2714494b322684872c0fc036c2f206b8514e54d021dc2ec1e3182af5855b98`（0026） | 前两例 PASS，pan FAIL：overflow 没有移动 |
| C12 | `98ed544fae3f5121f72e5db0534f91320d262751a4bf8893a90c6499799d6689` | `b2ce947b4781d7253658552e157c6b1b342868effdfbe7bbae3f8c2993cd2fd8`（0027） | 前两例 PASS，pan FAIL：native contact became mouse pointer |
| C13 | `fe75408d621a0c2cbe1f8882374d5289a1d0b43df8d0a1b6d407ac67e398cd08` | `b2ce947b4781d7253658552e157c6b1b342868effdfbe7bbae3f8c2993cd2fd8`（0027） | 七例完整 PASS |

三份 fixture **逐字节相同**，SHA256
`091623da8197f053bb8c456b8bf8f8235617739d1fde37d25eb6ed53cf035b91`。
`pad.scrollTop > initialScroll + 5`、pointerType/事件数量/兼容 mouse 断言均保留，没有放宽。
C12 在 precise scroll 修复后暴露 fake hovering mouse pointer；C13 的
`SymbianChromeClient::shouldDispatchFakeMouseMoveEvents()` 返回 false。
C11/C12 的 FAIL result、失败输入状态、原始新 guest log、截图与 driver 错误记录都保持原样；
没有从失败轮次删除或重标失败项。也没有把 C13 成功套用到 CI41 或旧二进制。

## 保存内容

- `c13/`：七次 run 的原始 result、fresh guest log、完整 gzip host/driver/input-driver log、
  before/after/final 截图、真实输入 ledger；profile-guard 只保留脱敏状态和哈希，不含私有配置。
- `c11-stable-viewport/`、`c12/`：旧失败 suite 原始文件，包含前两例 PASS 和 pan FAIL。
- `tools/`、`source/`：实际 suite/input/正常 runner/profile guard 源文件、fixture 及保留时 SHA256。
  suite 设置 WAIT=90、最终 DONE PASS 后再观察 5 秒；READY 不会提前完成。
- `identities/`：C11/C12/C13 的 build/source identities；C13 不可变源码副本与同构建资源；
  host0027 构建身份、0027 正常键盘补丁和完整 gzip build log；C13 正常 engine/browser build log。

本归档代理使用 view_image 查看了 C13 所有不重复的输入与最终截图，七张最终页均为 PASS；
另查看了 C11/C12 的 pan 失败图。原始绝对路径留在 JSON 中，归档对应关系和 gzip 原始/压缩
SHA256 在 `retained-files.json`；`SHA256SUMS` 覆盖全部保留文件（自身除外）。
没有程序镜像、固件、previous-log、用户 profile、原始 config 或临时凭据。
