# Belle FP2 IPv6 and HTTP/2

2026-10-29 外部候选：IPv6 和 HTTP/2 的依赖构建与 curl 接入、主机正向功能验收、
ARMv6 依赖编译、CurlContext 单对象编译和 SDK ELF 链接通过。以下原始结果属于该轮候选。
2026-11-01：适配当前 903ab1 与 CookieStore 工作树；完整构建和实际运行尚未完成。
本轮未运行 Belle 真机或模拟器网络，因此不把这些结果写成 Belle 联网运行通过。

IPv6 使用 Open C 声明，CMake 检测包含其所需的前置头文件。AF_UNSPEC
在 curl 的现有解析线程中分成明确的 IPv6、IPv4 查询，再将两族结果交给
Happy Eyeballs 连接流程。两次查询顺序执行；本验收没有覆盖运营商 DNS
延迟、DNS64、真实 AAAA 服务或设备 IAP。仅一族可用时保留该族结果。

HTTP/2 使用固定 SHA256 的 nghttp2 1.68.0 静态库，HTTPS 通过 TLS ALPN
协商，未协商 h2 时保留 HTTP/1.1。WSS 的 connection-only 路径仍使用
HTTP/1.1 Upgrade。旧依赖缓存缺少 nghttp2 或版本标记时，顶层构建会重建
nghttp2 和 curl；首次构建不会在下载 curl 前执行补丁。

| 限额 | 值 |
| --- | --- |
| 空闲连接缓存 / 总连接数 / 每主机连接数 | 8 / 8 / 4 |
| 每连接 HTTP/2 同时请求流 | 8 |
| HTTP/2 流与连接接收窗口 | 256 KiB |
| nghttp2 session 内部分配 | 每连接 512 KiB，包含分配元数据 |
| curl HTTP/2 响应头 / trailer 缓冲 | 每流 64 KiB / 16 KiB |
| 本端 HPACK 动态表 / 发出头块 | 4 KiB / 64 KiB |

nghttp2 分配限额不包含 curl 缓冲和 mbedTLS 分配；接收窗口是流控额度，
不是预分配内存。环境变量可下调连接池上限，不允许将其改成无限。

主机验收使用同版本 curl/nghttp2，临时源副本只替换三个文件中的平台
条件名以启用生产逻辑，TLS 后端为 OpenSSL 3.5.7。监听器仅在本机回环地址。

| 正向验收 | 结果 |
| --- | --- |
| IPv6-only / IPv4-only / localhost 两族解析与结构长度 | 通过 |
| IPv6 字面地址和 localhost HTTPS，证书验证开启 | 通过 |
| 固定双地址映射的 IPv6 到 IPv4 连接回退 | 通过 |
| 不支持 h2 的 TLS 服务回退 HTTP/1.1 | 通过 |
| IPv6 WSS Upgrade 和掩码文本回显 | 通过 |
| 12 个 HTTP/2 响应复用一个 IPv6 TLS 连接 | 通过；并发峰值 8 |
| 512 KiB 单响应、总计 704512 字节，窗口补充 | 通过 |
| 内部分配增长、缩小、限额、归还 | 通过 |

完整结果见 [host-report.json](host-report.json)、[host-tests.log](host-tests.log)
和 [budget.log](budget.log)。ARM 构建开启 IPv6、异步解析、nghttp2 和
mbedTLS；SDK 链接保留了 `Curl_getaddrinfo_ex`、
`nghttp2_session_client_new3`、`mbedtls_ssl_conf_alpn_protocols`，
见 [arm-build.json](arm-build.json)。补丁重复应用不改变文件，见
[patch-idempotence.json](patch-idempotence.json)。

可复用命令和测试源位于
[`tests/network-protocols`](../../../tests/network-protocols/README.md)。
待完成：集成后的完整浏览器构建和 Belle 实际 IPv6/AAAA、HTTPS/WSS、
mbedTLS ALPN、IAP 切换及设备内存行为验收。

前提复核：EKA KAfInet 必须返回 A/AAAA，并让 TCP 按实际 endpoint 选择地址族；
UDP 不能只改 lazy family，否则会丢广播选项和初始 write-ready。Open C 参考源的
sockaddr 容量修复属于 backend.dll 实现，SDK libc.dso 仅是导入 proxy；
实际 Belle backend.dll 与参考源的对应仍未建立，不把编译或代理导出算成运行修复。
