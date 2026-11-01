# FEP secret/password 能力：只读 API 核对

最小可靠来源是当前焦点的 `TCoeInputCapabilities::SupportsSecretText()`。
`coeinput.h` 把 `ESecretText` 明确定义为 **0x00000100**，含义为字符显示成星号；
getter 是 publishedAll/released 类的 public、const、非 leaving API。
官方同 revision 的 COEINPUT.CPP 实现仅返回 `iCapabilities & ESecretText`，
不会把 `EAllText` 当 secret。因此普通多行文字保持普通编辑模式。
**返回 TBool 的真值可能是 256，不能拿它与 ETrue(1) 作相等比较；按非零归一化。**

SDK cone 完整 frozen DEF 已有 `_ZNK21TCoeInputCapabilities18SupportsSecretTextEv`
**ordinal 316**；不需要新代理或新增 SDK 方法。`Capabilities()` 为 ordinal 311，
`FepAwareTextEditor()` 为 ordinal 314。优先用有语义的 getter，不直接读取私有字段布局。

0028 的 `CHostBridgedImeFEP::HandleChangeInFocusL()` 从
`CCoeAppUi::InputCapabilities()` 更新自身 `iInputCapabilities`；manual 分支在能力变化时
先取消旧 dialog，再更新。`OpenDialogInputL()` 已使用同一个对象取得 FEP-aware editor、
原始文本和最大长度；这里可取得 secret 元数据并与该请求一同传给 host，避免查询旧字段。
`CWebContainer::InputCapabilities()` 目前也已在 Password 分支设置 ESecretText，
普通分支只设 EAllText，和该判断一致。

`MCoeFepAwareTextEditor` 是文本/selection/composition 协议，没有直接 password getter；
`fepitfr.h` 只有 inline text 格式取回协议，也没有 secret 状态。
`CAknEdwinState::Flags()`/SetFlags 使用 Avkon editor flags，本次四份头没有提供应猜测的
password bit；无需走 `Extension1()->State(KNullUid)` 或下转 CAknEdwinState 才能判断 secret。

这里的 input capability 是 GUI 文本输入能力，不是平台安全 TCapability。读取该 public
getter 不需要新增进程 capability，也不需要跨进程访问其它 editor。沿用 FEP 的 UI 线程与
焦点/取消生命周期；传给 Qt 的只是每个请求的 secret 布尔值，不应记录或截图真实内容。
本目录没有实现 guest/host 改动，没有运行 guest/display，也没有读取凭据、剪贴板或私有配置。

`evidence.json` 记录四份头的官方来源、准确字节数和 SHA256。coeinput/aknedsts 来自
SymbianSource classicui `dcea899…`，fepbase 来自 inputmethods `51e877b…`；
fepitfr 使用官方精确 blob `72727614…` 的 4389 字节版本（SDK 原 inventory 未收录）。
保留的 `COEINPUT.CPP` 是官方 EPL 原文，blob `172a0333…`；版权声明未删改。
