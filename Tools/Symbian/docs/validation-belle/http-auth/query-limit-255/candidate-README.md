# HTTP auth secret-query 最大长度合同修复候选

已确定当前源码违反真实 SDK 的 secret editor 最大长度合同：`SECRETED.num_letters=256` 与 password `CAknTextQueryDialog::SetMaxLength(256)` 都超出255上限。官方 EIKCTL13 是 `EEikPanicSecretEditorTooLong`。`ECodeLayout + SECRETED` 组合正确，不能改成普通 EDWIN 来避开错误。

本候选没有复现 guest/display，没有 diagnostic shim、dump、凭据输入或私有文件读取；共享源未修改。公开校准03的guest EIKCTL13事实保留，host正常退出0不构成校准成功；旧02关闭也仍为非PASS。这里证明源级API/resource合同错误和修正，未定位当前ROM具体触发指令。

## 官方依据

- 真实 SDK `include/native/eikseced.h:64`：`EMaxSecEdSecArrayLength=255`；113、228行分别明确 ConstructFromResourceL / SetMaxLength 超限触发 EIKCTL13。完整公开 header 快照见 `reference/SDK-eikseced.h`。
- [classicui EIKPANIC.H:38](https://github.com/SymbianSource/oss.FCL.sf.mw.classicui/blob/dcea899751dfa099dcca7a5508cf32eab64afa7a/commonuisupport/uikon/coreinc/EIKPANIC.H#L38)：从0计数的 enum 第13值是 `EEikPanicSecretEditorTooLong`。
- [EIKSECED.CPP:297](https://github.com/SymbianSource/oss.FCL.sf.mw.classicui/blob/dcea899751dfa099dcca7a5508cf32eab64afa7a/uifw/eikctl/src/EIKSECED.CPP#L297)：resource用ReadUint16读取num_letters后执行 always断言≤255；509–510行 SetMaxLength再次检查。
- `reference/AknQueryControl.cpp:538`：ECodeLayout创建CEikSecretEditor并直接ConstructFromResourceL。当前BrowserQuery.rh的SECRETED单WORD定义与官方 `reference/EIKON.RH:921` 一致。
- [aknquerydata.cpp:25](https://github.com/SymbianSource/oss.FCL.sf.mw.classicui/blob/dcea899751dfa099dcca7a5508cf32eab64afa7a/uifw/AvKon/src/aknquerydata.cpp#L25)：有效API max优先于resource max。只改资源255仍会被API256覆盖，通过 `AknQueryDialog.cpp:1194 → AknQueryControl.cpp:2175 → CEikSecretEditor::SetMaxLength` 再报13。

上述固定commit公开原文节选、blob身份和链接通过GitHub插件读取，保存在 `reference/classicui-excerpts.json`。这些是API/公开实现依据，不声称是当前ROM逐指令证据。

## 最小修改

`auth-query-limit.patch` 仅两个生产文件：constructor接受独立maximumLength，username保持256，password使用真实SDK enum `CEikSecretEditor::EMaxSecEdSecArrayLength`（255）；password资源num_letters255。transfer descriptor容量仍256，足够接收原生上限，清零、Cancel、空凭据、生命周期与ECodeLayout遮罩均保留。四个既有资源ID保持。

生产patch SHA256：`6595becb1f4788bd4fbb641cc9af6a7620e22bff4da1e484b0819e5481c8eb97`。

`auth-query-limit-tests.patch` 为标准http-auth host adapter补独立secret上限、记录两步API max，增加username256/password255边界；原成功、空值、两步Cancel、identifier取消、nested lifetime、Leave、attempt bounds、容器modal期间release全部保留。新增fake enum先与真实SDK核对。test patch SHA256：`5e956949d69d4bead7f67866106b97bd3b80f0e4e19d82db937c701294cbba5d`。

两个patch在准备时的共享树上 `git apply --check` 均通过；root自行集成，按hunk应用即可，不复制整个overlay。

## 已完成检查

`python3 /workspace/symbian-auth-query-contract-work/check.py`：

- 真实SDK clang/rcomp编译before/after。WORD256语法可编译，但违反控件运行合同；修后WORD255合法。RSC均242B、四个ID相同，二进制仅offset169/170同一WORD从256改255。新RSC SHA256 `fb8e0b7154cb40458860c74bbb5334da613b34339cd47906a1cae0d6d7290ac8`。
- 复用browser-belle20实际ARM TU参数、外部VFS overlay与生成resource header，AuthenticationUI语法和ARM object均退出0；未全量构建或链接。
- 新host合同预期拒绝旧API256源码；修后原场景及边界在ASan/UBSan下通过、nativeAllocations归零。host负例禁用core dump，不涉及guest诊断。

命令/退出码见 `check-commands.json`、`validation.json`、`resource-contract.json` 和各日志；before/overlay/support、公开参考和检查产物有完整哈希。

修后真实native auth UI/网络仍待新候选验收。不能把公开校准02/03或普通password FEP三例PASS移用于auth。root集成构建后可先用公开空值路径确认password dialog正常打开/关闭，再进入既有授权范围的回归。
