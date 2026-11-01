# Belle 系统剪贴板：C14 浏览器键盘 9/9

2026-11-01（Asia/Shanghai）。本记录覆盖 C14 的一次真实原生键盘运行，9 个页面契约全部通过；Options 菜单、实际点编辑字段弹出 FEP 和 FEP 取消语义不在通过范围内。原生 adapter 的31项检查另有证据，不以它替代本次网页编辑路径。

## 运行身份与完成条件

- run：`clipboard-keyboard-c14-20261101-01`，`mode=keyboard`。
- URL：`http://localhost:8802/index.html?run=clipboard-keyboard-c14-20261101-01&mode=keyboard`。
- input driver：2026-11-01 10:36:19 至10:36:53（Asia/Shanghai；原始记录保留UTC）。
- browser.exe：58,793,144 bytes，SHA256 `b222b858e58703c057c538a8ee65c642e7ceb0b20cdcad47ea523305c86da065`。
- EKA host0027：SHA256 `b2ce947b4781d7253658552e157c6b1b342868effdfbe7bbae3f8c2993cd2fd8`。
- C14 含精确手指滚动、浏览器 Ctrl 映射、无hover fake-move策略和按真实编辑目标的 FEP 准入；不含 CookieStore/IPv6HTTP2/IAP proxy。
- `candidate14/` 的 build/source identities 来自不可变 C14 保存目录；归档时已核对全部5个产物及70个源码快照文件。snapshot记录的head不是“工作树无WIP”声明。

driver记录9次真正 Start 指针点击、12次真正 Ctrl 操作（V×8、C×2、X×1、A×1），逐组页面PASS和最终HTTP报告完全一致；actions.jsonl 与 interaction.records 全量相同。未注入DOM/剪贴板数据，没有直接写原生剪贴板，DOM Paste 权限没有放开。

独立 verify.py 已由 root 运行 exit0，原输出保留在 `keyboard-c14/validation.log`。该验证器自身只校验页面/报告，因此原 interaction/result 的分层 `PENDING_RUNTIME` / `PENDING_PAGE_VALIDATOR` 状态原样保留；`acceptance-summary.json` 另外结合以下证据给出本轮 keyboard 9/9 PASS：

- root观察 input driver 和 preservation wrapper 进程均 exit0；wrapper记录 standard launcher exit0。
- 标准 runner 在新STORED最终标记后观察5秒，正常Qt关闭host，日志检查通过；这是正常host关闭证明，不声称官方EndTask应用生命周期通过。
- `original_clipboard_restored=true`；本轮原clipboard文件及其父目录原本不存在，结束后恢复此状态。
- profile guard child_exit0、退出时无活动guest、原config与profile有无状态均恢复。仅保存其状态/哈希metadata，没有保存原config/profile内容。

没有捕获最终截图，不能补造。页面报告、真实输入ledger和完整新日志是本轮证据。

## 九项结果

| case | 真实输入与检查 | 结果 |
| --- | --- | --- |
| Unicode multiline copy | 真实Start中的受信DOM Copy，Ctrl+V；精确中文/emoji/多行、选择区和UTF-16长度 | PASS |
| partial Cut | 真实Start中的部分DOM Cut，Ctrl+V；精确删除片段、目标选择区和输入事件 | PASS |
| contenteditable | DOM Copy后Ctrl+V；纯文本、无HTML/文件数据 | PASS |
| DOM Paste 权限 | 只有真实Start，不发送额外Paste；脚本Paste仍拒绝 | PASS |
| password guard | Copy/Cut不得替换clipboard；Ctrl+V读回先前已知fixture内容 | PASS |
| readonly guard | Cut不得修改只读内容或clipboard；Ctrl+V核对 | PASS |
| native Copy/Paste | Ctrl+C、Ctrl+V；真实copy/paste/keydown/input事件 | PASS |
| native Cut/Paste | Ctrl+X、Ctrl+V；原文修改及真实事件 | PASS |
| SelectAll/Copy/Paste | Ctrl+A、Ctrl+C、Ctrl+V；原生选择区和完整文本 | PASS |

每组Start的isTrusted、key值、clipboard类型/files、paste内容及编辑结果由原fixture独立断言。完整页面报告保存在 `page-report.json`，未以日志摘录或“看起来相似”的文本替代。

## C12 失败尝试保留

`prior-attempts/` 分开存放两次原始尝试，不能与C14拼接成成功运行。

- `clipboard-keyboard-c12-20261101-01`：第一组PASS；第二组部分Cut/Paste报精确选择区内容不匹配。driver还记录同PID FEP打开而停止；wrapper/页面FAIL，原clipboard、config/profile恢复成功。
- `clipboard-keyboard-c12-20261101-02`：第一组PASS；发送过1次真实FEP Escape后，driver因键盘焦点离开EKA停止，页面随后报45秒未收到native paste。该路径的功能验收为INCOMPLETE；原wrapper/页面FAIL未改写，状态恢复成功。它不证明Escape保留选择区。

C14运行没有 `--fep-escape`，ledger中FEP Escape次数为0。尚未部署的 EKA FEP cancel 候选没有被本轮验证；不能因C14通过就宣称它已通过。真正点击input/contenteditable仍能打开FEP也需要独立UI回归。

## 文件与完整性

`keyboard-c14/` 保存新guest log、gzip后的完整host/runner日志、HTTP报告、输入ledger、driver输出、validator输出、wrapper和恢复metadata；没有previous-log、旧clipboard、profile DB或private config。

`fixture/` 为原index/suite/server/contract/verify/test/README副本；`harness/` 为本轮driver、原生XTest模块、wrapper/profile guard和公开opt-in editor键盘绑定源。它不包含原用户profile内容。`provenance.json` 记录原路径、原始bytes/SHA及gzip对应关系；driver/input/contract的hash与运行中interaction记录一致。其余fixture/harness副本标识归档时的实际文件，未伪造缺失的运行前manifest。

`SHA256SUMS` 校验全部归档文件；gzip可解压后按provenance检查原始bytes/SHA。没有重新运行guest、功能测试、构建、修改共享源或提交；本目录供root审查后选择归档。
