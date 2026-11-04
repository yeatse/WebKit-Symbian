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
401 原生 UI 的 [C21/host0029 十二个普通 Basic/Digest 场景](validation-belle/http-auth/browser-c21-host0029/README.md)已通过；四个退出场景仍未验收。online 与真实内存压力整轮仍为 FAIL。
CookieStore 已合入本地 WIP 并完整链接 C21，真实 HTTPS 页面15项 DOM 功能通过；
但官方 EndTask 后 guest KERN-EXEC3，整轮仍 FAIL，尚未提交或上线。
IPv6/HTTP2 已完成本地 C25 受控模拟器正向验收：真实 UDP DNS 的 AAAA-only HTTPS/WSS、
双栈 IPv6 与 IPv6 不可用时的 IPv4 回退通过；[IPv6 hostname H2](validation-belle/network-native-resolver/c25-host0032-aaaa-ipv6-h2-pass/README.md)
在一个真实 ::1 TLS 连接完成12流704512B，峰值4、设置上限8、窗口256KiB和8次window update。
当前 C25 的 IPv4 H2/H1 回归也通过。Root 在私有 user/net/mount namespace 中运行 DNS/TLS/EKA，
未读取或修改宿主 resolver；不是 hosts/CURL_RESOLVE/shim。IAP proxy 仍在独立候选准备。
[原生记录/默认协议](validation-belle/network-native-resolver/native-records-pass/README.md)修复分界明确：
数字 IPv6 Open C 正常，但 hostname转换不符合同；owned curl 改用有界 native RHostResolver，
EKA0032按类型选择 KUndefinedProtocol 默认 TCP/UDP。未应用旧 capacity 参考补丁。
真机 IAP/ISP DNS/延迟、Scope/DNS64、导航取消及预算耗尽仍需独立验收。
这些本地候选和部分验收不等于第二批已全部完成，也不替代真机验证。

Classic WebAudio现默认ON。除本地C28外，[C29已独立运行CI50/0.3.50实际浏览器载荷](validation-belle/webaudio/browser-c29-ci50-host0032/README.md)：原strict离线检查、四次真实输入、997Hz双声道PCM及暂停/恢复/关闭通过，正常退出与所选范围恢复通过。三SIS另经[实际验包](validation-release-0.3.50/ci/README.md)；未执行安装器或真机测试。Worklet、媒体元素PCM、真实资源中断和无缝pause仍有明确限制。

IAP原生前置查询已有[host0032/0033真实对照](validation-belle/network-iap-native/service-host0033-control0032/README.md)：修订后Start/Id/Network/Service全部成功，审计与恢复通过；[host0034](validation-belle/network-iap-native/host0034-readonly-rom/README.md)还通过只读原ROM启动。此结果不代表代理或CommDB读取通过。所选IAP的DNS需显式Name/clone绑定；三参数原生resolver不会因OpenC setdefaultif自动继承连接。修订正在外部重建和验收，尚未合入发布源。

ScreenPlay 软件呈现已有本地构建及模拟器验收（2026-11-04），见
[SCREENPLAY.md](SCREENPLAY.md)：host0038 两种尺寸/四次提交、Available、Finish、
清理及审计恢复通过。C33/测量用 host0039 的强制 ScreenPlay 页面 9 项通过、WebGL
1 项跳过；14 个屏幕纯色区域共 7200 像素、差异 0，提交标记 1、bitmap 回退 0，
正常关闭、审计、恢复与私有目录清理全部通过。v8 清理失败记录仍为 FAIL。
C33 含 IAP/CookieStore/多文件上传 WIP、UI 资源 383 字节，不能当作纯 CI 包。
默认 `auto` 仍依次尝试 EGL、ScreenPlay、bitmap；其实际切换、可信输入、MMF
退场、原生浏览器窗口缩放、真机及性能未因此通过。默认 auto 目前另有真实 TU
编译通过，未运行。Finish 整次调用在 stock 满队列下的硬时限缺口已有 host 反例。
浏览器依赖 Surface Manager，SUS1.1.3 客户端在本地；窗口初始不透明，首次 MMF
display 前退场再启 alpha 并永久禁用本窗口 ScreenPlay。不宣称零拷贝或真机支持。

| 顺序 | 能力和现状 | Belle/现有代码基础 | 实施与验收要求 |
| --- | --- | --- | --- |
| P0.1 | **已接通** localStorage 和 BroadcastChannel | 真实 namespace/registry；[20 项运行检查](validation-belle/storage/README.md) | 已验证跨进程重启、localStorage 配额、来源隔离、同来源消息、关闭后不投递及 Worker 通信 |
| P0.2 | **已接通 SameSite** 解析、落盘、迁移和发送筛选 | [两进程 28 项验收](validation-belle/cookie-samesite/README.md)；另修 curl 地址族截断及 SDK IP SAN/SNI | Strict/Lax/None、Secure、跨站导航/子资源/POST/重定向/WSS、嵌套来源、跨协议与重启均通过；旧库降级再升级保留原 24 条记录。HTTPS/WSS 原有 6 项回归通过 |
| P0.3 | **已接通** sessionStorage、Worker 栈修复及 IDB 配额 | [17 项后端检查](validation-belle/backends/README.md)；[三次启动 16 项配额验收](validation-belle/quota-browser/README.md) | 完整 ClientOrigin 32 MiB、全局 128 MiB、持久写准入 16 MiB 磁盘余量及 Blob 回滚；20 MiB Blob 重启后逐字节读回，配额重建、删除及空间复用通过。磁盘余量为准入检查，不是物理预留 |
| P1.1 | **已接通原生单文件上传**；下载已接入本地候选 | [实际 UI 8 项](validation-belle/ui/README.md)；C11–C16 原生保存及实际落盘验收 | 上传取消/Unicode/实际字节/frame 生命周期已验证；多选目前仅单文件。下载取消保存、POST 恰好一次、GET、gzip、[chunked](validation-belle/downloads/browser-c15-chunked/README.md)、[重名拒绝后取消](validation-belle/downloads/browser-c16-existing-pass/README.md)通过；原文件内容/inode/mtime不变。[C19传输中取消](validation-belle/downloads/browser-c19-cancel-transfer/README.md)通过，8192字节后真实菜单取消、清理和正常退出通过；其余异常路径待验收 |
| P1.2 | **本地 C13/C17 单指 touch/pointer 七组通过**；原生 tap 另有 C11 证据 | Window Server 指针事件、HAL；按原生查询报告点数（本次模拟器为 8），不以报告值冒充实际多指 | preventDefault、普通/pointer 拖动、touch-action:none、capture、导航取消均通过；修复精确滚动和错误桌面 hover 事件。实际多指、独立原生失焦取消及真机仍待验收，原生 pinch 未实现 |
| P1.3 | **已打包字体配置并接通 WOFF2** | [字体运行证据](validation-belle-fonts/README.md)；固定版本 Brotli/WOFF2，修复 fontconfig 盘符路径 | WOFF2 7 项通过；系统字体 7 项分别在两次启动通过，包括中英/等宽字形、真实 fallback 像素与磁盘缓存复用 |
| P1.4 | **已接通 AES-CTR、RSA、ECDSA/ECDH** | [AES 浏览器 29 项](validation-belle/webcrypto/README.md)、[ARM32 计数器 35 项](validation-belle/aes-ctr-counter/README.md)、[RSA 浏览器 33 项](validation-belle/rsa/browser-c11/README.md)、[EC 浏览器 30 项](validation-belle/ec/browser-c11/README.md) | 标准向量、生成、导入/导出、错误拒绝、并发及 DedicatedWorker 调用已验证；CryptoKey 结构化克隆和持久包装仍需独立系统密钥方案 |
| P1.5 | **原生 HTTP 401 的12个普通场景通过**；4个退出场景待验收；IAP proxy 有外部候选 | [C21/host0029 Basic/Digest 12项](validation-belle/http-auth/browser-c21-host0029/README.md)、CAknTextQueryDialog、CommDB proxy reader、curl | 成功输入、用户名/密码取消、三次错误后停止、用户名/密码待定时导航均通过；本轮heading和每个密码遮罩实例审图，可信父页面输入、普通关闭及恢复通过。官方EndTask的4个退出场景未验收；代理切换/排除/并发取消仍待平台验收，407无UI且明确失败，不复用站点401凭据 |
| P1.6 | **原生 prompt、纯文本系统剪贴板键盘与菜单路径已验证** | CClipboard 原生适配 31 项；[C15 真实键盘 9 组](validation-belle/clipboard/browser-c15-keyboard/README.md)、[原生 FEP 五组](validation-belle/fep-ui/browser-c15/README.md) | Ctrl+A/C/X/V、选区、多行 Unicode、contenteditable、DOM paste 拒绝、密码/readonly 保护通过，原 clipboard/config 恢复。FEP真实Escape保留文本及选区，提交/清空生效，程序focus与readonly不误开FEP。[C17 Options菜单九组](validation-belle/clipboard/browser-c17-menu/README.md)通过；[host密码FEP三例](validation-belle/fep-secret/browser-c19-host0029/README.md)通过（逐字遮罩、取消/提交/清空）；富文本与 Async Clipboard 权限另列 |
| P1.7 | 内存压力与 online/offline 后端已入候选；整体验收未完成 | RChangeNotifier、EChangesFreeMemory、HAL、实际 RConnection/ConnMon | online 页面19项通过但EndTask后host -11，整轮FAIL。C15真实压力已及时READY；worker提交46,137,344字节、实际critical通知/释放/模式检查和worker退出0，浏览器三次VERIFIED后host -11，[整轮仍FAIL](validation-belle/memory-pressure/browser-c15-failed/README.md)。未满足40s保留及80MiB JSCell门槛；系统剩余RAM不等于进程可分配容量 |
| P1.8 | **IPv6/HTTP2 已接通并完成受控模拟器正向验收** | curl7.88.1/nghttp2 1.68.0/SDK mbedTLS ALPN；owned native DNS桥，EKA0030/0032 | [C25真实AAAA/双栈回退/HTTPS/WSS](validation-belle/network-native-resolver/c25-host0032-dns-pass/README.md)、[真实IPv6 hostname H2](validation-belle/network-native-resolver/c25-host0032-aaaa-ipv6-h2-pass/README.md)及IPv4 H2/H1通过；12流704512B、实际峰值4/限制8、窗口256KiB/流控通过。真机IAP/ISP DNS/Scope/DNS64、导航取消及预算耗尽未验收；UDP开句柄不等于IPv6 UDP数据路径通过 |
| P1.9 | CookieStore 已合入本地 C21 WIP；未提交或上线；无 HTTP 磁盘缓存 | document provider/Curl SQLite/变更观察已通过 host/ARM、完整链接及 HTTPS页面15项DOM功能 | 官方 EndTask 后 guest KERN-EXEC3，整轮仍FAIL；先解决有明确依据的生命周期问题再验收退出。无 SW manager/partitioned；磁盘GET缓存另需 ETag/304/Vary/no-store/认证隔离/中断写 |
| P1.10 | **ScreenPlay 不透明软件呈现本地实现；模拟器原生 API 与强制路径显示已验** | SDK0010 正式头/冻结导入、私有 SUS1.1.3；双 buffer/direct Cairo/逐 buffer damage/cache/Available；[实现与边界](SCREENPLAY.md) | host0038 两尺寸/四提交及清理通过；C33/host0039 v9 页面9过1跳过、14区7200像素差异0、SP1/回退0、审计恢复正常关闭与清理通过。原WIP与UI383非纯CI；默认EGL/MMF/输入/原生窗口缩放/真机/性能待验；Finish stock队列硬时限缺口保留 |
| P2.1 | **经典 WebAudio 默认开启；C28 受控模拟器验收通过** | CMdaAudioOutputStream、真实 FFT/HRTF、WAV 解码；SDK0007/0009 数学运行库 | [离线数值与实际 PCM](validation-belle/webaudio/browser-c28-host0032/README.md)：128帧延迟、声道、振荡器/FFT、WAV、Biquad；真实点击后997Hz双声道及暂停/恢复/关闭通过。音频要求用户手势；Worklet隐藏、MMF媒体元素PCM源明确NotSupported。真机、系统资源中断及无缝pause未验收；EKA采用明确不支持pause后的Stop/reopen/re-prime路径 |
| P2.2 | Fullscreen、Media Session 及视频帧读取缺失 | AVKON 全屏、MMF Utility2/GetFrameL、按控制器查询播放速率 | 用户手势、导航/软键退出、原生视频定位；异步帧缓存后再接 drawImage(video)/WebGL 视频纹理；不能假定所有音频支持变速 |
| P2.3 | 地理位置、方向/运动、振动等设备桥缺失 | LBS/RPositioner、Sensor Server、HWRM；按机型查询 | 权限和原生 capability、订阅取消、前后台/导航生命周期、错误/缺设备路径；真机数据与耗电验证 |
| P2.4 | SharedWorker、Cache API、StorageManager 缺真实 provider | 单进程线程/文件/SQLite 可用；目前禁用或 dummy | 先补独立 provider、配额/驱逐、来源隔离与重启，再暴露 API；不等同 Service Worker 已支持 |
| P2.5 | 系统证书库同步、客户端证书、通知/分享缺适配 | UnifiedCertStore、AVKON/平台服务有接口 | 保留完整链与主机名校验；原生权限、用户选择、撤销与生命周期；不得用忽略 TLS 错误代替适配 |
| P3.1 | Service Worker 缺整条服务链 | 系统能执行后台线程和保存文件，但 Legacy 没有现成完整实现 | 注册数据库、更新、生命周期、fetch 拦截、CacheStorage、配额/终止；需独立架构工作 |
| P3.2 | MSE、MediaStream、WebRTC、WebCodecs 尚未接通 | MMF/DevVideo/ECam 仅提供部分底层能力 | 需要新分段解码/采集/时钟/网络后端和权限体系；先逐项探测 API/控制器，不承诺靠开关即可支持 |

补充文件接口验收：UTF-16路径打开、metadata及存在性已通过[实际WTF三文件检查](validation-belle/file-utf16/wtf-owner/README.md)，含非BMP文件名与空文件。此结果不代表多选GUI/FormData已通过；旧窄字符8/9对照FAIL保留。


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
