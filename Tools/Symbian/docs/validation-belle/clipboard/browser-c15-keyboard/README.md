# C15 浏览器剪贴板：真实键盘 9/9

2026-11-01（Asia/Shanghai）。本记录是 C15、host0028 和新部署 FEP 下的一次独立真实键盘回归。
页面九例通过、标准 launcher 退出0，原剪贴板和键盘 config/profile 状态恢复成功。
本次只整理原证据，没有重跑 guest。

- run：`clipboard-keyboard-c15-20261101-01`，mode=keyboard。
- C15 browser.exe：58,793,784 bytes，SHA256 `473679d5c12d7dfeb057955ebd2ec0291cdd5db4654b6a3a2c2dba7c31fb995e`。
- host：`277db2b3e44c69d4b860b2f76cf194178e820556671ad32f4891fcae2cb4c2ff`。
- FEP：`aad1a4c6a4a52c70d3ac430d45dce671a8ff664071c921146bc65f754a612f6b`。
  FEP 身份来自 root 提供的部署记录及同批 clock result；原 clipboard wrapper 本身只 pin browser/host。
- input driver 原始 UTC 为02:59:01至02:59:14，即北京时间10:59:01至10:59:14。

`input/actions.jsonl` 与 interaction.records 全量一致：真正 Start 点击9次、Ctrl操作12次
（V×8、C×2、X×1、A×1），9组 case-passed 与保存的 HTTP report 对应，report SHA 与 interaction 一致。
没有直接读写原生剪贴板或注入 DOM；fixture 检查 trusted click/keydown、真实 copy/cut/paste/input、
精确选区与UTF-16文本、纯文本类型及文件列表。

| case | 结果 |
| --- | --- |
| trusted Unicode multiline copy and native paste | PASS |
| trusted partial cut and native paste | PASS |
| contenteditable copy and plain native paste | PASS |
| DOM paste remains denied with a trusted click | PASS |
| password copy and cut cannot replace clipboard | PASS |
| readonly cut preserves text and clipboard | PASS |
| native Copy and native Paste | PASS |
| native Cut and native Paste | PASS |
| native SelectAll then Copy and Paste | PASS |

原始 result 的 `OBSERVATION_COMPLETE_PENDING_PAGE_VALIDATOR` 与 interaction 的
`INPUT_AND_PAGE_CONTRACT_PASS_PENDING_RUNTIME` 原样保留。`acceptance-summary.json`
组合独立 page/report validator PASS、真实输入ledger、launcher0与恢复证据给出 keyboard 9/9 PASS。
标准 runner 观察本轮最终 STORED 后正常 Qt close；这不是官方 EndTask 应用退出生命周期验收。

原剪贴板文件及父目录原本均不存在，wrapper记录恢复此状态为true；profile guard记录child_exit0、
恢复时无活guest、原config和profile有无状态恢复true。只保留状态/hash元数据，未复制实际私有内容。
`harness/webkit-editor.yml` 是公开的 opt-in 测试绑定源，其hash匹配guard，不是原用户profile。

本轮没有FEP Escape、没有捕获截图，不能补造。新FEP已部署的事实不证明Cancel语义已被本轮触发；
Options菜单、实际点编辑字段弹出FEP、FEP取消/焦点/导航生命周期均须独立验收。
此前 C14 与 C12 的证据仍在原目录，本轮没有删除、改写或把不同轮次拼接为成功。

`keyboard-c15/` 保存新guest log、gzip完整host/runner日志、HTTP report、原始输入ledger、
validator与恢复metadata。`fixture/` 和 `harness/` 保存输入工具/标准runner/公开绑定源。
记录中的driver、contract、input module、report、profile hash均已核对；其余工具副本标识归档时字节，
没有伪造缺失的运行前manifest。`candidate15/` 的五个产物和72个源码身份已核对，未复制EXE/DLL。
`provenance.json` 保留原件/gzip对应关系；`SHA256SUMS` 校验所有保留文件。没有previous-log、
profile DB、原始config/clipboard或凭据，没有修改共享源码或进行新的构建/测试。
