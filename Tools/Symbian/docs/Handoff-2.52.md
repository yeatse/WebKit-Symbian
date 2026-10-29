下面正文可以直接保存为 **`WebKit-2.52.6-Symbian-交接.md`**，发给新会话。

**注意：此前移植代码没有 commit 或 push，原环境已丢失。本文是重建说明和验证记录，不包含完整源码补丁。新环境必须重新实施移植；此前通过的结果不能视为新环境已经通过。**

---

# WebKit 2.52.6 Symbian 移植交接

## 1. 用户目标

用户原始要求：

> EKA2L1 切到 webkit-symbian 分支，用 add_repo 挂上 yeatse/WebKit-Symbian 的 2.32.4-symbian 分支，读 Tools/Symbian/docs/Handoff.md；然后基于 2.52.6 commit 切出来一个 2.52.6-symbian 分支，参考 2.32 的 Symbian 移植工作，适配一版 2.52.6 的 Symbian WebKit。

最新要求：在新环境继续工作，并及时把改动和交接文档提交到仓库，避免再次丢失。

沟通使用中文。用户希望直接推进，不要反复询问例行实现选择。

## 2. 重要事实：旧环境的工作未进入 Git

旧环境已经无法恢复。

此前没有创建移植 commit，没有 push，也没有发布标签。源码改动包含暂存、未暂存和未跟踪文件，最后只存在于旧工作区。

环境配置草稿保存过详细交接文字，但**没有保存完整源码 diff**。因此：

- 不要假定远端已有此前完成的移植。
- 不要假定下文提及的构建产物、日志、测试页面或临时工具仍然存在。
- 下文的绝对路径是旧环境路径，可作为新环境目录规划参考。
- 首先检查远端真实状态；若没有其他会话保存的新提交，就按本文重建。
- 重建过程中及时创建阶段提交，交接文档也应进入 Git。

## 3. 仓库和准确基线

### EKA2L1

- 仓库：`https://github.com/yeatse/EKA2L1`
- 分支：`webkit-symbian`
- 旧环境基线：

```text
5bbc0435cf9a3a3d0f08d20259d35f60255c3a8a
```

不要误挂同名的上游 `EKA2L1/EKA2L1`，应使用用户 fork。

### WebKit

- 仓库：`https://github.com/yeatse/WebKit-Symbian`
- 参考分支：`2.32.4-symbian`
- 参考分支旧环境 HEAD：

```text
4dca1b91da49f8b27310822304dfacd9d474fe7d
```

- 新分支：`2.52.6-symbian`
- 准确上游基线是 **WebKitGTK 2.52.6**，标签 `webkitgtk-2.52.6` 对应提交：

```text
4fb33923db2f945803df49546f75867980365c08
```

不要误用同版本号的 WPE 标签提交。

旧版参考分支的上游基线曾定位为 `a81ec020`；重建时应自行核验，使用三方迁移思路，而不是整目录覆盖新 WebKit。

### 首先阅读

从参考分支读取：

```text
Tools/Symbian/docs/Handoff.md
Tools/Symbian/docs/Development.md
Tools/Symbian/README.md
Tools/Symbian/scripts/
Tools/Symbian/patches/
```

旧 Handoff 中的 N8/X7、ARM JIT、HTTPS 和在线页面成功结果属于 **2.32**，不能当成 2.52 的验证。

此前 `add_repo` 工具不可用，使用 HTTPS Git clone 加环境配置登记代替。新环境若有 `add_repo` 可以直接使用。

## 4. 新版移植基线

此前采用以下可行路线：

- Symbian^3 / Anna / Belle，ARMv6、VFPv2、softfp。
- C++23。
- symbian-platform SDK **0.3.0**。
- JavaScriptCore 使用 **C_LOOP 解释器**。
- 关闭 JIT、DFG、FTL、WebAssembly、sampling profiler。
- 不直接移植 2.32 恢复的 `ARM_TRADITIONAL` 汇编/JIT 后端。
- ICU 升级到 **70.1**。
- Cairo / FreeType / HarfBuzz 软件绘图。
- AVKON 原生浏览器壳，通过 WebKitLegacy 接入。
- curl / mbedTLS 网络与 TLS。
- TLS 始终保留证书和主机名验证。
- Service Worker、Shared Worker、Cache API 在设置中关闭。

SDK 下载地址：

```text
https://github.com/hpnkv/symbian-platform/releases/download/v0.3.0/symbian-sdk-0.3.0-linux-x86_64.tar.gz
```

SDK 对应源码仓库：

```text
https://github.com/hpnkv/symbian-platform
```

实际使用过的 SDK Clang 版本为 23.1.2。

## 5. 旧环境已经实现过的主要改动

以下改动需要重新实施。优先从 2.32 参考分支迁移平台和工具代码，再根据 2.52 API 修改。

### 5.1 WTF / JSC

涉及主要位置：

```text
Source/WTF/wtf/PlatformOS.h
Source/WTF/wtf/PlatformHave.h
Source/WTF/wtf/PlatformJSCOnly.cmake
Source/WTF/wtf/PlatformSymbian.cmake
Source/WTF/wtf/symbian/
Source/WTF/wtf/posix/
Source/WTF/wtf/threads/Signals.*
Source/JavaScriptCore/PlatformSymbian.cmake
Source/JavaScriptCore/jsc.cpp
Source/JavaScriptCore/runtime/ArrayBuffer.cpp
Source/JavaScriptCore/runtime/BufferMemoryHandle.cpp
```

关键适配：

- 添加 `OS_SYMBIAN`；平台条件使用 **`OS(SYMBIAN)`**，不要使用未定义的 `PLATFORM(SYMBIAN)`。
- SDK 的 `std::filesystem` 有部分类型，但没有完整文件 IO 实现，应关闭对应能力判断，使用 POSIX 后端。
- 新增 `FileSystemSymbian.cpp`，适配盘符路径、根路径、`.`/`..`、目录递归、复制、临时目录等。
- 文件复制处理短写、禁止覆盖、源目标相同及 inode/symlink 别名。
- SDK `link()` 实际不是可靠的真正硬链接。硬链接接口应明确失败，`hardLinkOrCopyFile` 使用真正复制。
- 卷容量/剩余空间查询返回不可用。
- 跨盘 move fallback 仅处理普通文件。
- Symbian `MAP_FILE` 可定义为 0；不要假定匿名 mmap 可用。
- 临时目录使用 `C:/Data/WebKit/tmp`，使用 `mkstemp`。
- 更新 `RunLoop::currentSingleton`、`Locker`、`std::size`、现代 span/Optional API。
- 适配线程句柄、挂起/恢复、寄存器上下文、栈边界、CPU 时间和 RAM 查询。
- 不支持的可恢复信号路径应明确失败，不能伪装支持。

JSC 其他实际遇到的问题：

- `BlockDirectory.cpp` 的 `mincore` 路径排除 Symbian。
- `JSCBytecodeCacheVersion.cpp` 不使用 `link.h` / `dl_iterate_phdr`，走通用 timestamp fallback。
- `MathCommon.cpp` 将已有 `roundeven` fallback 扩展到 Symbian。
- `jsc.cpp` 将日志重定向到 `C:/Data/jsc.log`，适配 Symbian 命令行。
- Unix SIGUSR/crash signal CLI 功能排除 Symbian。

### 5.2 RChunk 与 Resizable ArrayBuffer

原生桥接主要在：

```text
Source/WTF/wtf/symbian/SymbianSystem.h
Source/WTF/wtf/symbian/SymbianSystem.cpp
Source/WTF/wtf/symbian/OSAllocatorSymbian.cpp
```

此前实现：

- disconnected `RChunk` 预留。
- 对齐、guard 区和整数溢出检查。
- reservation 元数据表。
- 按页记录 committed 状态。
- commit/decommit 幂等。
- commit 部分失败时，只回滚本次新提交的页。
- read/write 请求保留已有数据。
- no-access 用 decommit 实现，重新 commit 清零。

边界：

- EKA2 没有通用 `mprotect`。
- 不支持真正只读保护。
- no-access/decommit 不能用于临时保护需要保留内容的数据。
- 元数据表同时最多 256 个 reservation；超限返回分配失败，释放后槽位复用。
- 普通 system malloc 分配不占这个表。

RAB 必须处理：

- Symbian 扩容时 `tryProtect(ReadWrite)` 可能失败。
- 失败时回滚物理内存计数，返回 OOM，保留旧长度和数据。
- `BufferMemoryHandle` 的共享 bounds 析构路径不要重新提交未使用页再释放。
- 初始 `maxByteLength` 仍可能暂时全部提交，这是已知限制。

### 5.3 安全随机数

**不要沿用 `Math::Random()` 作为安全随机源。**

此前修改为：

- 原生桥接 `randomBytes` 返回 `bool`。
- 使用 SDK `mbedtls_hardware_poll`。
- 每次最多 1024 字节，循环填充大请求。
- 检查返回值及实际写入长度。
- 错误或短填充时清零整个输出并返回失败。
- WTF `RandomDevice` 对失败使用 fail-closed 处理。
- WTF/JSC E32 链接相应 mbedcrypto / `Symbian::Crypto`。

SDK 熵实现可在源码中检查：

```text
cpp/symbian/entropy/sdk_entropy.cc
```

此前确认它使用受检查的内核安全熵接口，没有回退到 `Math::Random()`。

模拟器通过只证明接口调用与返回路径，不证明真机熵源质量。

### 5.4 WebCore / PAL

主要需要迁移或补充：

```text
Source/WebCore/PlatformSymbian.cmake
Source/WebCore/SourcesSymbian.txt
Source/WebCore/PAL/pal/PlatformSymbian.cmake
Source/WebCore/platform/symbian/
Source/WebCore/platform/MbedTLS.cmake
Source/cmake/OptionsSymbian.cmake
```

已遇到的具体问题：

- 删除旧 source list 中已经不存在的：

```text
platform/generic/ScrollAnimatorGeneric.cpp
platform/posix/SharedBufferPOSIX.cpp
```

- Adwaita 已移动到 `rendering/adwaita`，使用当前 `USE_THEME_ADWAITA` 配置。
- 清理旧版已经无效的 feature option，按当前 `WebKitFeatures.cmake` 核验。
- `JSHTMLMediaElementCustom.cpp` 在 VIDEO 关闭时缺保护，需要正确加 `ENABLE(VIDEO)` guard。
- `DocumentLoader.cpp` 使用 `EventLoopTaskGroup` 时需显式包含 `EventLoop.h`。
- `AV1Utilities.cpp`、`VP9Utilities.cpp` 在 VIDEO 关闭时仍编译，需要把：

```text
Source/WebCore/platform/video-codecs
```

加入 Symbian 私有 include 路径，供 `BitReader.h` 使用。

此前新增平台文件：

```text
AccessibilitySymbian.cpp
SharedMemorySymbian.cpp
SystemFontDatabaseSymbian.cpp
```

实现范围：

- Accessibility 没有原生 wrapper，不应伪装有系统集成。
- SharedMemory 支持本进程零初始化内存，明确拒绝跨进程 handle。
- 字体数据库提供合理的通用字体 fallback，字体选择仍接 Fontconfig/FreeType。

### 5.5 WebKitLegacy 与浏览器壳

主要目录：

```text
Source/WebKitLegacy/symbian/
Tools/Symbian/browser/
```

需要适配新版本：

- ChromeClient
- EditorClient
- FrameLoaderClient
- PlatformStrategies
- BackForwardList
- WebViewImpl
- 当前 `PageConfiguration`、智能指针和客户端所有权 API

最终链接还遇到：

1. 旧键盘状态函数与新上游重复定义。
   - 对齐现代 `currentStateOfModifierKeys()`。
   - 键盘、触摸、滚轮输入更新 modifier 缓存。
   - 失焦时清空。

2. `WebResourceLoadScheduler` 缺 10 个平台错误工厂。
   - 新增 `WebResourceLoadSchedulerSymbian.cpp` 并加入 Legacy CMake。
   - 包含 cancelled、blocked、content blocker、cannot show URL/MIME、policy change、file missing、HTTPS upgrade loop、HTTPS-only、plugin 接管等。
   - 取消、策略中断、plugin 接管使用合适的 Cancellation 类型。
   - 访问限制类错误保留正确分类和错误码。

### 5.6 网络桥

2.52 上游已删除旧 Legacy curl ResourceHandle 实现，需要恢复并适配。

主要位置：

```text
Source/WebCore/platform/network/curl/ResourceHandleCurl.cpp
Source/WebCore/platform/network/ResourceHandle.*
Source/WebCore/platform/network/ResourceHandleInternal.h
Source/WebCore/platform/network/SynchronousLoaderClient.*
Source/WebCore/platform/network/curl/CurlRequest.*
Source/WebCore/platform/network/curl/AuthenticationChallenge.h
Source/WebKitLegacy/symbian/SocketStreamHandleImplCurl.cpp
```

特别注意：

- 同步加载消息队列的 kill/append 竞争。
- request 与 queue 的引用环。
- cancel 后停止 failure timer，避免回调已取消 client。
- redirect 后忽略旧 request 的迟到回调。
- 嵌套 UI loop 下保持 deferred callback FIFO。
- HTTP 真正 pause/resume；`file://` 只延迟回调，不能假定 curl file 支持 pause。
- 同源重定向按新路径重新计算 Cookie。
- 跨源重定向在 client 修改请求后再次清理授权和 Cookie。
- 拒绝 HTTP 重定向到本地协议，检查非法端口和地址。
- POST→GET 重定向清理 body headers。
- data URI 回调异步并遵守 defer。
- 同步 401 拒绝认证后正常完成响应，不能 assert。
- WebSocket 队列发送、关闭和对象生命周期要适配新 CurlStream API。
- 保留 TLS peer/hostname verification。

### 5.7 WebCrypto

此前新增：

```text
Source/WebCore/crypto/mbedtls/
Source/WebCore/platform/MbedTLS.cmake
Source/WebCore/PAL/pal/crypto/mbedtls/CryptoDigestMbedTLS.cpp
```

已实现并注册：

- SHA-1/256/384/512
- HMAC
- HKDF
- PBKDF2
- AES-CBC
- AES-CFB8
- AES-GCM
- AES-KW

未实现的 AES-CTR、RSA、EC、Ed25519、X25519 不注册，返回 `NotSupportedError`，不能用空实现伪装成功。

没有可用系统 master-key 存储，持久 CryptoKey wrap 明确拒绝。

## 6. 依赖和构建工具

ICU：

- 使用 `release-70-1`。
- 数据文件 `icudt70l.dat`。
- 默认目录 `C:/resource/icu`。
- Symbian data path 分隔符补丁保留。
- ICU LSTM 的 `ctgmath` 改为 `cmath`，避免 SDK 缺少 long-double 接口。
- 数据 SIS 使用新 UID `0xE1A5C013`，避免更新 JSC 时卸载旧 ICU63。

此前第三方依赖沿用参考移植的版本：

- pixman 0.40.0
- cairo 1.16
- sqlite 3350500
- harfbuzz 2.7.4
- libxml2 2.9.12
- libxslt 1.1.35
- fontconfig 2.13.94
- libpsl 0.21.1
- libwebp 1.2
- curl 7.88.1
- SDK mbedTLS

注意 libxml2 原 tarball CMake 曾错误报告 2.9.1，导致选错 API；要修正成 2.9.12 / 20912。

编译 wrapper 中过滤宿主 `-lm/-ldl/-lpthread` 时，要显式链接 Symbian 对应 import proxies，特别是实际 `libm.dso`。

Sparse checkout / CI 需要包含：

```text
Source/ThirdParty/unifdef
```

不能只复制旧版列表。

构建顺序：

```sh
export SYMBIAN_SDK_PREFIX=/path/to/symbian-sdk
export SYMBIAN_WEBKIT_WORK=/path/to/work
export JOBS=4

Tools/Symbian/scripts/build-symbian-native.sh
Tools/Symbian/scripts/build-icu.sh
Tools/Symbian/scripts/build-jsc.sh
Tools/Symbian/scripts/link-e32.sh
Tools/Symbian/scripts/build-deps.sh
Tools/Symbian/scripts/build-webcore.sh
Tools/Symbian/scripts/build-browser.sh
Tools/Symbian/scripts/package-sis.sh
```

WebCore 脚本构建明确目标 `WebCore WebKitLegacy`，避免无关测试程序。

## 7. E32 转换器：最后的精确断点

参考分支已有两个 SDK 补丁：

```text
Tools/Symbian/patches/symbian-native/
0001-Raise-E32-converter-limits-for-large-images.patch
0002-Accept-LLD-PIC-range-extension-thunks-to-the-PLT.patch
```

重建时需要保留并扩展它们。

### 已经遇到并解决过的问题

- 大量 absolute pointer / import / relocation 的旧限制过低。
- LLD PIC ARM 长跳板。
- ARM 单条无条件 B 跳板转到 PLT。

单 B 跳板支持必须严格验证目标 PLT/import slot，拒绝错误 slot、条件跳转、越界等情况，不能笼统接受任意 branch。

此前这部分测试达到 **44 项通过**。

### 最后遇到的问题

完整浏览器 ELF 已链接成功，但 E32 转换失败：

```text
DATA_LOSS: Invalid ARM exception index
cpp/symbian/e32/e32.cc:95
```

旧 converter 在 `ExceptionDescriptorOffset` 中有：

```cpp
section.size > 65536
```

这是任意的旧上限。

当时实际浏览器：

- ELF 大小：91,900,208 字节。
- `.ARM.exidx`：732,408 字节。
- 共 91,551 条 EHABI 记录。
- 独立核验排序、function PREL31、2,909 个 extab 引用和 descriptor 四字段均合法。

因此正确方向是移除任意 64KB 限制，保留结构、对齐和 code segment 范围检查；无需修改合法 ELF 布局。

旧环境已经编译了 converter 修改，但新增测试尚未完成诊断：

```text
PreservesExceptionIndexBeyond64KiB
RejectsMalformedLargeExceptionIndex
RejectsLargeExceptionDescriptorBoundsMismatch
```

预期从 44 增至 47 项。

最后测试命令返回 1，但断连前没读到具体日志，因此**不能认为这三项已通过**。需要重新构造 fixture、诊断并验证。

大表 fixture 要包括：

- 严格递增的 PREL31 function addresses。
- 合法 CANTUNWIND。
- descriptor 的四个 ABS32 relocation。
- 大于 64KB 的正例。
- 记录长度损坏、越界 descriptor 等负例。
- 转换、Inspect 和字节保留检查。

建议把大表限制修补及对应测试并入 `0001`。注意两个补丁对同文件的重叠可能破坏构建脚本的逐 patch 反向检查幂等性，必须验证：

1. 干净 SDK 顺序 apply。
2. 逐 patch reverse check。
3. 缓存目录重复运行构建脚本。

## 8. 旧环境实际验证记录

这些结果用于说明路线可行，**新环境仍需重新验证**。

### JSC

最终旧环境 `jsc.exe`：

```text
14,306,160 bytes
SHA256:
85bbd059bcea072f805e7f316567c54c83d7f8a511f58f66be9067de4b80e711
```

包含安全熵接口和 RAB OOM 处理。

EKA2L1 X7 系统内四组测试都通过，且各自有：

```text
jsc exiting 0
```

测试内容：

- basic：基本求值、VM 安全随机初始化。
- language：可选链、nullish、BigInt、正则、对象/字符串/浮点运算、数组复制方法、Promise.allSettled / Promise.any。
- Intl：数字、排序、UTC 日期、复数、相对时间、分词。
- RAB：resize 保留/缩容/重新增长清零/边界；GSAB grow；GC。
- RAB 使用 `--useSharedArrayBuffer=true`。
- 未观察到 panic 或访问违例。

### 浏览器

已经通过：

- WTF/PAL/JSC/WebCore/WebKitLegacy 完整 ARM 编译与归档。
- AVKON 浏览器壳编译。
- 完整浏览器 ELF 链接。

**尚未通过：**

- 最终完整浏览器 E32 转换。
- browser.sis。
- 浏览器页面、WebCrypto、HTTP/TLS/WSS 实际运行。

`STUB=1` 的壳曾成功，但不能代替完整引擎验证。

## 9. 测试与运行要求

### EKA

固件获取及安装方法参考远端旧 `Handoff.md`。此前使用 X7 / RM-707。

CLI 安装补丁和 panic dump 补丁只用于本地验证。

重要：EKA 宿主遇到客体 panic 也可能退出 0。不能只检查宿主退出码。

JSC 运行脚本应：

- 自动加入 `-x`。
- 清除或隔离旧 `jsc.log`。
- 要求本次 `jsc exiting 0`。
- 要求预期 PASS 行。
- 检查 panic/访问违例。
- 支持超时并清理自己的进程组。
- 使用 workspace 下 XDG 目录，不覆盖 `HOME`。

SIS CLI 安装固定选 E 盘，运行路径要核实。旧环境为方便验证，把相同哈希的 exe 复制到 C 盘运行。

### 浏览器验证顺序

1. 本地 HTML，检查 DOM、现代 JS、中文和截图。
2. WebCrypto 标准向量及错误输入。
3. HTTP / WebSocket。
4. HTTPS / WSS 成功。
5. 不受信任证书、错误主机名的拒绝路径。

旧环境准备过但已经丢失、需要重建的测试：

- `tests/es2020.js`
- `tests/intl.js`
- `tests/resizable-arraybuffer.js`
- `tests/webcrypto.html`
- `tests/network.html`
- `tests/serve-network-tests.py`
- `tests/network-tests.md`

网络测试曾设计为 21 项：

- fetch、异步/同步 XHR。
- 分块响应、data URI。
- 302/303/307 重定向。
- 401 拒绝。
- XHR/Fetch 取消。
- Cookie Path、重定向重算、删除。
- 跨端口授权隔离与合法 Cookie 行为。
- WebSocket 文本和二进制回显。
- 失败后新请求仍可成功。
- Cookie 清理回读。

默认服务双端口 8765/8766，页面支持 `?autorun=1`。

同步 XHR 卡住不能被 JS 定时器打断；必须由宿主超时判失败。

TLS 测试：

- 临时 CA 只追加到测试模拟器自己的 `cacert.pem`。
- 保留原字节，测试结束恢复。
- 不修改 SDK/global trust。
- 不把测试 CA 打进发布包。
- 不关闭 TLS peer/hostname verification。
- WSS 负例必须独立证明 `error` 且从未 `open`，不能以 HTTPS 页面未加载替代。
- 证书日期与客体时钟必须匹配。
- 旧环境两天测试证书早已不适合作为长期资产，应重新生成。

### QEMU

Linux ARM QEMU 是补充验证，不覆盖 Symbian 的线程、RChunk、文件系统和 AVKON。

此前：

- ARM 工具链和 ICU70 格式化实跑通过。
- QEMU JSC 构建未完成。
- 不能写成 QEMU JSC 已通过。

旧 qemu 脚本必须从 ARM JIT 路线改成 C_LOOP，并保证失败不被 `|| true` 吞掉。

## 10. 环境准备经验

旧环境是 Debian 13、4 核配额。可按新环境权限选择正常 apt 安装，或使用 workspace 本地 sysroot。

宿主依赖大致包括：

```text
cmake ninja-build ccache pkg-config nasm
clang lld ruby perl libjson-pp-perl gperf flex bison unifdef
autoconf automake libtool
GTK3 PulseAudio ALSA SDL2 开发包
Qt6 base/private/tools/l10n/svg/multimedia 开发包
Wayland xkbcommon EGL
xvfb xauth 7zip
qemu-user g++-arm-linux-gnueabi
```

本地 sysroot 的额外注意：

- Ruby 需要正确 `RUBYLIB`，确保 `erb`、`json` 可加载。
- Bison 需要 `BISON_PKGDATADIR`。
- ccache 必须使用可写目录。
- Debian SDL2 的架构转发头需要 multiarch include 路径。
- 该 host include 只用于 EKA 构建，不能污染 Symbian 交叉编译。
- 7zip 的包装脚本可能硬编码 `/usr/lib/7zip`，本地解包时直接使用实际二进制。
- EKA 内嵌 FFmpeg 有自己的并行参数，避免和 Ninja 叠加超出 CPU 配额。
- Xvfb 使用 `-nolisten tcp`。

## 11. 新会话建议执行顺序

1. 核验远端和分支，确认是否有后来保存的移植提交。
2. 克隆两个用户 fork，读取 2.32 交接文档。
3. 从准确 GTK 2.52.6 commit 创建新分支。
4. 将本文写入仓库，例如：

```text
Tools/Symbian/docs/Handoff-2.52.md
```

5. 迁移工具链、ICU 和 WTF/JSC；先完成可运行 JSC。
6. 每完成一个阶段就提交代码和验证记录。
7. 移植 WebCore/Legacy、网络桥和 WebCrypto。
8. 完整链接浏览器，完善 E32 转换器大表/跳板支持及回归。
9. 打包并完成真实 EKA 页面和网络测试。
10. 更新 README、Development、Port-2.52 和交接文档，明确已验证与未验证范围。
11. 在向远端保存工作时使用用户 fork 的新移植分支；不要把 EKA 本地 panic 诊断补丁推送上游，不发布 release 标签。

## 12. 必须保留的能力边界

这是一版开发移植：

- C_LOOP，无 JIT/DFG/FTL/Wasm。
- 无硬件合成、WebGL、多媒体后端。
- Service Worker、Shared Worker、Cache API 关闭。
- WebCrypto 只注册已实现算法。
- 没有安全的系统 master-key 持久封装实现。
- RChunk 保护能力与 POSIX `mprotect` 不等价。
- 文件系统有明确的硬链接、容量查询与跨盘移动限制。
- 模拟器测试不等于真机验证。
- 编译、ELF 链接、E32 转换、SIS 安装、实际运行是不同验证阶段，必须分别记录。

**优先把可重建的源码和文档提交到 Git，再继续长时间编译。**