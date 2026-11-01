# Nokia Belle FP2 能力补全清单

盘点基于本端口的实际构建选项、WebKitLegacy 客户端、WebCore 平台实现与
SDK 0.3.0 的原生头文件/导入库，日期为 2026-10-29。系统有 API 只说明有实现
基础；下表的待办没有因此算作已支持。模拟器验收也不替代真机的驱动、性能和
系统权限验证。具体机型的触摸点数、MMF 编解码器和传感器必须运行时查询。

当前基线已有 ARM11 LLInt/Baseline/DFG、Cairo/FreeType/HarfBuzz、原生 EGL/GLES2
合成、WebGL 1、MMF 音视频、严格校验的 HTTPS/WSS、WebSocket、持久 Cookie、
部分 WebCrypto。内存/JIT、图形、安装及媒体结果分别见
[内存](validation-memory/README.md)、[JIT](ARM11-JIT.md)、
[图形](GRAPHICS.md)、[安装](validation-release-0.3.40/README.md)和
[媒体](MEDIA.md)。不把这些已有实现重新列为新功能。

排序优先考虑常用网页是否能正常保存数据、输入和交换文件，其次是网络与渲染
兼容性，再到需要新平台后端的媒体/设备能力。同一优先级可并行实现，验收后才
更新为完成。不能用开启编译开关或存在 JS 属性代替功能验证。

第二批开发状态（2026-11-01）：RSA 33 项及 EC 30 项浏览器验收已通过并推送。
C13/C17 七组真实单指 touch、C15 九组真实键盘剪贴板和五组原生 FEP 输入/取消通过。
下载已分别通过取消保存、POST 恰好一次、GET、gzip、chunked、重名拒绝后取消；
C19 传输中取消亦通过，保存窗口返回后恢复主软键/布局，菜单显示真实8192字节并可取消。
C17 Options 的九组剪贴板操作也已通过；旧菜单失败原件继续保留。
原生单调时钟已替代固件粗粒度 wall clock；[C15 八次定时回调](validation-belle/monotonic-browser/browser-c15/README.md)
在 453ms 内完成，计时与键盘回归通过。
401 UI、内存压力和连接状态已有候选实现；online 与真实内存压力整轮仍为 FAIL。
CookieStore、IPv6/HTTP2、IAP proxy 已有独立外部候选和主机/ARM检查，尚未合入。
这些本地候选和部分验收不等于第二批已全部完成，也不替代真机验证。

| 顺序 | 能力和现状 | Belle/现有代码基础 | 实施与验收要求 |
| --- | --- | --- | --- |
| P0.1 | **已接通** localStorage 和 BroadcastChannel | 真实 namespace/registry；[20 项运行检查](validation-belle/storage/README.md) | 已验证跨进程重启、localStorage 配额、来源隔离、同来源消息、关闭后不投递及 Worker 通信 |
| P0.2 | **已接通 SameSite** 解析、落盘、迁移和发送筛选 | [两进程 28 项验收](validation-belle/cookie-samesite/README.md)；另修 curl 地址族截断及 SDK IP SAN/SNI | Strict/Lax/None、Secure、跨站导航/子资源/POST/重定向/WSS、嵌套来源、跨协议与重启均通过；旧库降级再升级保留原 24 条记录。HTTPS/WSS 原有 6 项回归通过 |
| P0.3 | **已接通** sessionStorage、Worker 栈修复及 IDB 配额 | [17 项后端检查](validation-belle/backends/README.md)；[三次启动 16 项配额验收](validation-belle/quota-browser/README.md) | 完整 ClientOrigin 32 MiB、全局 128 MiB、持久写准入 16 MiB 磁盘余量及 Blob 回滚；20 MiB Blob 重启后逐字节读回，配额重建、删除及空间复用通过。磁盘余量为准入检查，不是物理预留 |
| P1.1 | **已接通原生单文件上传**；下载已接入本地候选 | [实际 UI 8 项](validation-belle/ui/README.md)；C11–C16 原生保存及实际落盘验收 | 上传取消/Unicode/实际字节/frame 生命周期已验证；多选目前仅单文件。下载取消保存、POST 恰好一次、GET、gzip、[chunked](validation-belle/downloads/browser-c15-chunked/README.md)、[重名拒绝后取消](validation-belle/downloads/browser-c16-existing-pass/README.md)通过；原文件内容/inode/mtime不变。[C19传输中取消](validation-belle/downloads/browser-c19-cancel-transfer/README.md)通过，8192字节后真实菜单取消、清理和正常退出通过；其余异常路径待验收 |
| P1.2 | **本地 C13/C17 单指 touch/pointer 七组通过**；原生 tap 另有 C11 证据 | Window Server 指针事件、HAL；按原生查询报告点数（本次模拟器为 8），不以报告值冒充实际多指 | preventDefault、普通/pointer 拖动、touch-action:none、capture、导航取消均通过；修复精确滚动和错误桌面 hover 事件。实际多指、独立原生失焦取消及真机仍待验收，原生 pinch 未实现 |
| P1.3 | **已打包字体配置并接通 WOFF2** | [字体运行证据](validation-belle-fonts/README.md)；固定版本 Brotli/WOFF2，修复 fontconfig 盘符路径 | WOFF2 7 项通过；系统字体 7 项分别在两次启动通过，包括中英/等宽字形、真实 fallback 像素与磁盘缓存复用 |
| P1.4 | **已接通 AES-CTR、RSA、ECDSA/ECDH** | [AES 浏览器 29 项](validation-belle/webcrypto/README.md)、[ARM32 计数器 35 项](validation-belle/aes-ctr-counter/README.md)、[RSA 浏览器 33 项](validation-belle/rsa/browser-c11/README.md)、[EC 浏览器 30 项](validation-belle/ec/browser-c11/README.md) | 标准向量、生成、导入/导出、错误拒绝、并发及 DedicatedWorker 调用已验证；CryptoKey 结构化克隆和持久包装仍需独立系统密钥方案 |
| P1.5 | HTTP 401 原生 UI 已入候选，完整交互待验收；IAP proxy 有外部候选 | CAknTextQueryDialog、官方 CommDB proxy reader、curl 凭据机制 | Basic/Digest 输入/取消/三次重试/导航退出；代理切换/排除/并发取消仍需平台验收。407 尚无 UI，明确失败，不复用站点401凭据 |
| P1.6 | **原生 prompt、纯文本系统剪贴板键盘与菜单路径已验证** | CClipboard 原生适配 31 项；[C15 真实键盘 9 组](validation-belle/clipboard/browser-c15-keyboard/README.md)、[原生 FEP 五组](validation-belle/fep-ui/browser-c15/README.md) | Ctrl+A/C/X/V、选区、多行 Unicode、contenteditable、DOM paste 拒绝、密码/readonly 保护通过，原 clipboard/config 恢复。FEP真实Escape保留文本及选区，提交/清空生效，程序focus与readonly不误开FEP。[C17 Options菜单九组](validation-belle/clipboard/browser-c17-menu/README.md)通过；host密码键盘遮罩、富文本与 Async Clipboard 权限另列 |
| P1.7 | 内存压力与 online/offline 后端已入候选；整体验收未完成 | RChangeNotifier、EChangesFreeMemory、HAL、实际 RConnection/ConnMon | online 页面19项通过但EndTask后host -11，整轮FAIL。C15真实压力已及时READY；worker提交46,137,344字节、实际critical通知/释放/模式检查和worker退出0，浏览器三次VERIFIED后host -11，[整轮仍FAIL](validation-belle/memory-pressure/browser-c15-failed/README.md)。未满足40s保留及80MiB JSCell门槛；系统剩余RAM不等于进程可分配容量 |
| P1.8 | IPv6/HTTP2 已有独立外部候选，尚未合入 | nghttp2 1.68.0、curl/SDK ARM 编译及 host IPv6/H2 检查通过；EKA/OpenC ABI 缺口另有候选 | 先修实际 EKA/SDK 前提，再验 IPv6-only、AAAA/IPv4 回退、HTTPS/WSS、H2 多路复用及预算。host OpenSSL 结果不代替 guest mbedTLS，实际固件 backend.dll 行为仍待确认 |
| P1.9 | CookieStore 已有外部 provider 候选，尚未合入；无 HTTP 磁盘缓存 | document CookieStore、Cookie DB/SQLite/变更观察候选通过 host/ARM 检查；现有内存缓存可用 | 继续完整链接及真实 DOM 读写/删除/change/expiry；无 SW manager/partitioned。磁盘 GET 缓存需独立实现 ETag/304/Vary/no-store/认证隔离/中断写 |
| P2.1 | Web Audio 关闭，没有 Symbian 输出后端 | CMdaAudioOutputStream/DevSound PCM 输出 | 有界音频缓冲、重采样、FFT、文件解码和线程生命周期；OfflineAudioContext 数值、实际频谱、暂停/恢复、系统音频中断 |
| P2.2 | Fullscreen、Media Session 及视频帧读取缺失 | AVKON 全屏、MMF Utility2/GetFrameL、按控制器查询播放速率 | 用户手势、导航/软键退出、原生视频定位；异步帧缓存后再接 drawImage(video)/WebGL 视频纹理；不能假定所有音频支持变速 |
| P2.3 | 地理位置、方向/运动、振动等设备桥缺失 | LBS/RPositioner、Sensor Server、HWRM；按机型查询 | 权限和原生 capability、订阅取消、前后台/导航生命周期、错误/缺设备路径；真机数据与耗电验证 |
| P2.4 | SharedWorker、Cache API、StorageManager 缺真实 provider | 单进程线程/文件/SQLite 可用；目前禁用或 dummy | 先补独立 provider、配额/驱逐、来源隔离与重启，再暴露 API；不等同 Service Worker 已支持 |
| P2.5 | 系统证书库同步、客户端证书、通知/分享缺适配 | UnifiedCertStore、AVKON/平台服务有接口 | 保留完整链与主机名校验；原生权限、用户选择、撤销与生命周期；不得用忽略 TLS 错误代替适配 |
| P3.1 | Service Worker 缺整条服务链 | 系统能执行后台线程和保存文件，但 Legacy 没有现成完整实现 | 注册数据库、更新、生命周期、fetch 拦截、CacheStorage、配额/终止；需独立架构工作 |
| P3.2 | MSE、MediaStream、WebRTC、WebCodecs 尚未接通 | MMF/DevVideo/ECam 仅提供部分底层能力 | 需要新分段解码/采集/时钟/网络后端和权限体系；先逐项探测 API/控制器，不承诺靠开关即可支持 |

## 代码依据

- 页面配置：`Source/WebKitLegacy/symbian/WebViewImpl.cpp`；默认值还需对照
  当前构建的 `DerivedSources/WebCore/Settings.cpp`，不能只看 CMake 开关。
- 文件选择、prompt、全屏、方向：`SymbianChromeClient.h/.cpp`；下载/认证：
  `SymbianFrameLoaderClient.h/.cpp`；原生输入：`Tools/Symbian/browser/WebContainer.cpp`。
- 网络 Cookie：`Source/WebCore/platform/network/curl/{CookieUtil,CookieJarDB,NetworkStorageSessionCurl}.cpp`；
  请求上下文：`ResourceHandleCurl.cpp`；IAP：`Tools/Symbian/browser/NetworkConnection.cpp`。
- 字体/网络依赖：`Tools/Symbian/scripts/build-deps.sh`、`Source/cmake/OptionsSymbian.cmake`；
  字体配置还要对照安装后的原始日志，不能仅检查库链接成功。
- 加密：`Source/WebCore/crypto/mbedtls/`；媒体：
  `Source/WebKitLegacy/symbian/MediaPlayerPrivateSymbian.*` 与 `Tools/Symbian/browser/MediaPlayers.cpp`。

## 不应作为简单补全承诺的项目

WebGL 2 需要 GLES 3 能力，WebGPU 需要符合要求的现代图形后端，不能把 Belle 的
GLES2 当作实现基础。FTL 和当前上游 WebAssembly 对 ARM11/JSVALUE32_64 的限制
涉及编译器后端，不是 Symbian API 缺口。平台 DRM 不等于存在可用的 Widevine 等
网页 CDM；现代视频编码的高效播放也不能从 EKA 宿主 FFmpeg 的支持推断到手机。
这些项目保持明确限制，另行研究后才能进入实现队列。
