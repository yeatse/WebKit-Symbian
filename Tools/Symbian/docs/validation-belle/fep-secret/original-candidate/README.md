# 0029 候选：由官方 secret capability 驱动 FEP 密码遮罩

当前已部署的 0028 guest FEP 只传 initial/max/status，Qt 端统一使用 QPlainTextEdit，
没有传递 secret 信息。它不能保证密码不被明文显示。本目录提供独立 0029 源码候选；
没有修改共享 EKA/SDK，没有完整构建 host 或 FEP DLL，也没有 guest/真实 UI 验收。

## 官方依据与请求合同

`TCoeInputCapabilities::SupportsSecretText() const` 是现有公开、非 leaving getter；
官方 COEINPUT.CPP 返回 `iCapabilities & ESecretText`，真值可能为 **256**。
Guest 按 `!= EFalse` 归一化，不能用 `== ETrue`。官方 ESecretText=0x100 的说明是字符以星号显示。
现有 cone proxy 已有 getter ordinal 316，不需要新 SDK 导入代理。
来源 commit、原文 blob、四份头及准确 SHA 在 `official-api-evidence.json`。
浏览器的 Password 分支已经设置 ESecretText；无需经 CAknEdwinState 的私有标志猜测。

旧 A0 HLE ABI 保持，新增 A6：
`EHUIOpenGlobalTextViewWithSecret(func_id, initial_text, max_length, TBool secret, request_status*)`。
本次请求的 secret 状态从 FEP capabilities 取得，经独立 selector、Qt driver/main_window 传入对话框。
请求 ID、迟到 launch/close/completion 屏蔽和 0028 的 Submit/空 Submit/Cancel 合同保持。

## Qt 行为

只有 secret 请求显示 QLineEdit，echoMode 固定 **QLineEdit::Password**，在填充初始内容之前设置；
不是 PasswordEchoOnEdit，没有 reveal 切换。隐藏/敏感/no-predictive 输入提示同时设置。
秘密不会写进 QPlainTextEdit 的 QTextDocument；普通请求继续原有多行 Unicode 编辑器。
Submit 返回真实逻辑值，Cancel 只发送取消与空结果，不提交 draft；状态结束后清除 main_window
暂存值。此工作不声称 QString 内存被安全抹除。

Android/iOS 当前 plaintext-only frontend 对 secret 请求明确拒绝，普通请求保持原路径。
没有在本环境编译它们，不能声称 mobile 密码输入已支持。

## 本地检查

- 五个实际 host TU 编译0：mainwindow、dialog_driver、symbian_input_dialog、hui、register。
- 实际 Qt widget 的21项单测0失败：Password属性、displayText遮罩、普通多行/Unicode、
  隐藏 plaintext document 为空、maxLength、Submit/空 Submit/Cancel、secret/plain复用。
  单测使用 Qt offscreen，DISPLAY为空；没有启动或连接任何 X display server，没有截图。
- 提取实际 Qt/dispatch 方法的51项 ASan/UBSan状态断言通过，保留旧取消/迟到回调回归，
  新增 secret请求取消后新plain请求不继承模式、请求内容清理等。
- 提取实际 A0/A6 wrapper 的12项参数传递检查通过，含 TBool 256；它们不替代真实ARM调度执行。
- Guest 实际 ARM TU及 A0/A6 dispatch ASM编译通过；40项提取方法检查通过，
  覆盖secret 0/256、max值和原Submit/Cancel/selection语义；错误 `== ETrue` 对照被拒绝。
- 0029在当前共享0028树apply-check通过；小型源码副本重放、逐文件SHA及reverse-check通过。

host/guest检查报告、完整命令和日志均保留。本阶段没有使用真实密码、认证服务、用户配置或私有文件。

## root 续接

先合入 `0029-Mask-secret-FEP-text-before-host-display.patch`，再完整构建匹配的 host 与 native FEP DLL。
现有可迁移 FEP builder 会读取新的 dispatch 源并保留新增 A6；无需改变 plugin UID/ordinal。
普通 host 构建仍可能复制旧 FEP，所以构建后恢复/核对新 FEP 与原 MMF两DLL/两map，共五个资产。

新的 secret控件是单行QLineEdit，实际编辑区域可能与多行FEP不同。auth输入前先用空查询确认
secret模式/实际控件和坐标，再输入临时凭据；旧的400×300多行编辑坐标不能当新控件已校准。
当前候选只证明源码、原生编译与host单测，尚不能给已部署旧资产标注密码遮罩PASS。
