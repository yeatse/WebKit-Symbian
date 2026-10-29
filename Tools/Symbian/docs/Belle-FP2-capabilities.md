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

| 顺序 | 能力和现状 | Belle/现有代码基础 | 实施与验收要求 |
| --- | --- | --- | --- |
| P0.1 | **已接通** localStorage 和 BroadcastChannel | 真实 namespace/registry；[20 项运行检查](validation-belle/storage/README.md) | 已验证跨进程重启、localStorage 配额、来源隔离、同来源消息、关闭后不投递及 Worker 通信 |
| P0.2 | **已接通 SameSite** 解析、落盘、迁移和发送筛选 | [两进程 28 项验收](validation-belle/cookie-samesite/README.md)；另修 curl 地址族截断及 SDK IP SAN/SNI | Strict/Lax/None、Secure、跨站导航/子资源/POST/重定向/WSS、嵌套来源、跨协议与重启均通过；旧库降级再升级保留原 24 条记录。HTTPS/WSS 原有 6 项回归通过 |
| P0.3 | sessionStorage 已启用，Worker 栈已修；IDB 配额已实现，**运行验收中** | [17 项后端运行检查](validation-belle/backends/README.md)；[配额 host/ARM 检查](validation-quota/idb-host/README.md) | IDB 增加完整 ClientOrigin 32 MiB、全局 128 MiB、持久写准入 16 MiB 磁盘余量及 Blob 提交失败回滚；尚未完成 guest 配额验收 |
| P1.1 | **已接通原生单文件上传**；下载被忽略 | [实际 UI 8 项](validation-belle/ui/README.md)；原生文件选择与 RFs/RFile | 已验证取消、Unicode 路径、实际上传字节、frame 导航/脱离；多选输入目前可选单文件。继续下载目标、进度、取消、重定向、磁盘满及原子完成 |
| P1.2 | 实际仅 mouse/wheel，缺 DOM touch；maxTouchPoints 固定 0 | Window Server 指针事件、HAL、支持机型的高级指针 | touchstart/move/end/cancel、preventDefault、滚动、兼容 mouse 去重；按实际能力报告点数，多指需真机验收 |
| P1.3 | **已打包字体配置并接通 WOFF2** | [字体运行证据](validation-belle-fonts/README.md)；固定版本 Brotli/WOFF2，修复 fontconfig 盘符路径 | WOFF2 7 项通过；系统字体 7 项分别在两次启动通过，包括中英/等宽字形、真实 fallback 像素与磁盘缓存复用 |
| P1.4 | **已补 AES-CTR**；RSA/ECDSA/ECDH 仍缺 | [WebCrypto 29 项](validation-belle/webcrypto/README.md)及 [ARM32 计数器 35 项](validation-belle/aes-ctr-counter/README.md) | 继续 RSA/ECDSA/ECDH 标准向量、导入/导出及错误拒绝；密钥持久包装另需系统密钥方案 |
| P1.5 | HTTP 认证没有 UI；IAP 代理未接 | CAknMultiLineDataQueryDialog、ConnectionMethod/IAP 属性、curl 凭据机制 | 401 输入/取消/重试限制；407 独立代理凭据路径；IAP 切换、代理排除、TLS 验证和来源间凭据隔离 |
| P1.6 | **已接原生 prompt**；系统剪贴板未接 | CClipboard、原生文本流、CAknTextQueryDialog | prompt 默认值、空串和取消已实际 UI 验证；原生对话框中正常退出应用已通过。继续跨应用 UTF-16 纯文本复制粘贴；富文本与 Async Clipboard 权限另列 |
| P1.7 | 内存压力通知为空，online 状态恒 true | RChangeNotifier、EChangesFreeMemory、HAL、连接监视 API | 实际低内存通知及有界缓存回收；连接变化触发 online/offline。不能把系统剩余 RAM 当作进程可分配容量 |
| P1.8 | IPv6、HTTP/2 没有编入 curl | Open C IPv6/getaddrinfo 接口、nghttp2 可移植库 | IPv6-only、AAAA/IPv4 回退、HTTPS/WSS、HTTP/2 多路复用和资源限额；先核实 SDK/模拟器实际实现 |
| P1.9 | CookieStore 暴露但 curl DOM 方法为空；无 HTTP 磁盘缓存 | 现有 Cookie DB/SQLite/RFile；内存缓存已有 | CookieStore 读写/删除/权限/变更事件；小容量 GET 缓存的 ETag/304/Vary/no-store/认证隔离/中断写；不能直接搬 WebKit2 网络进程 |
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
