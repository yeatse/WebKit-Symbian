# WebKit 2.52.6 Symbian 交接（续接状态）

## 2026-11-01 续接更新：覆盖下面的暂停快照

用户已说“继续”，当前在推进 Belle P1。下文 10 月 30 日内容保留作历史依据，
其中“未推送”“目录不存在”“C11 最后构建”已过时，不能作为当前状态。

- 开发树仍为 `/workspace/WebKit-Symbian-graphics`、`belle-fp2-capabilities`；
  主树目标仍为 `2.52.6-symbian`。暂停前 df7274/7201187 和交接提交已推送。
  CI41/42 实际 SIS 已下载并核实版本、payload、CRC、CA、ICU；CI42 源
  `46234c2b063`、包 0.3.42。见 `validation-release-0.3.42/ci/`。
  CI42 浏览器尚未安装/运行，也不包含本地 P1 候选。
- 本轮接入下载、touch/pointer、原生系统剪贴板、401 UI、内存压力和连接状态。
  又修复 Ctrl 键、FEP 点击准入/Cancel、精确滚动、错误 fake hover、单调时钟、
  chunked 长度、100ms 进度合并、重名重试和 popup 清理栈合同。
  接入或编译成功不等于所有路径验收通过。
- C15/C16/C17 不可覆盖的镜像和 source/build identities 在
  `/workspace/symbian-belle-work/candidateN/`。C17 browser 为 58,795,408B，
  SHA `0d91d91ea1e016b7725097bac0768a9aa2fdf9710b765795769d2da07608a1c8`；
  UI RSC 242B，SHA `251cfbede521cc5d0a3e36400cf108b12b6bb8eb7956e33a6781d8ef4bf5bc3d`。
  必须配对资源，不能继续部署 C16 的 192B RSC。
- 当前 EKA host SHA `277db2b3e44c69d4b860b2f76cf194178e820556671ad32f4891fcae2cb4c2ff`，
  包含 0027 Ctrl/Alt 与 0028 FEP Cancel。新 native FEP DLL 为 99,272B，
  SHA `aad1a4c6a4a52c70d3ac430d45dce671a8ff664071c921146bc65f754a612f6b`，已实际构建部署。
  后续每次构建 EKA 会覆盖 native patch，须恢复此 DLL 和原有四个 MMF DLL/map。
  FEP 原件在 `/workspace/symbian-fep-sdk-build-work/artifacts/`；链接器 map 报告不能当 ordinal override。
- SDK/EKA 自身仍不推送，导出补丁随 WebKit 保存。新增 0027/0028、SDK native
  0006 和可移植 FEP builder/官方头文件 overlay 在 `Tools/Symbian/`。
  通用 builder 仅 prepare/assembly 检查；具体 FEP 候选完整构建与 ABI 检查已过。

本地浏览器已通过：C13/C17 单指 touch 七组；C15 键盘剪贴板九组；C15 FEP 五组
（真实 Escape 保留值/选区、提交、清空、程序 focus 和 readonly 不误开）；
C15 八次定时回调在 453ms 内完成。下载分别通过取消保存、POST 恰好一次、GET、
gzip、chunked、重名保护后取消。每份证据绑定不同镜像，不能拼成某张镜像全测通过。
详见 [Belle 清单](Belle-FP2-capabilities.md) 和对应 `validation-belle/` 目录。

最新 C19：native Save 返回后由仍 attached 的 manager 通知 shell 恢复自有 CBA、命令、布局和绘制；
browser 58,796,116B，SHA `6b6e8e0a430ff9c1ce0be935161d4b7e61804444e2947cb0218e74cb2e5420ae`，RSC同C17。
传输中取消已实际通过：正进度8192字节→真实 Options→Cancel download→terminal1/error0/cleanup0，
清理、标准runner退出0和原数据恢复均通过。原C15/C16/C17失败不改判。
C17 Options 九组剪贴板也已整轮通过（12次真实菜单点击，未用Ctrl或End猜选）；
旧End校准仍FAIL。见各证据目录。

仍待完成：HTTP Basic/Digest 16组、多指/独立原生失焦和真机。
已确认旧host FEP未传secret标志、使用明文QPlainTextEdit；0029遮罩修复候选准备中，
必须匹配重建host与native FEP并验收后再输入auth测试凭据。不得归档凭据、Authorization、
原始剪贴板或private config。

内存与网络失败：C15 及时 READY，worker 提交 46,137,344B，实际 critical、释放/
模式检查、worker 退出0；浏览器三次 VERIFIED 后 host -11，未通过40s保留，
cellBytes 37,910,912 也未达到80MiB门槛，整轮 FAIL。旧 C13 READY timeout、
online C11 页面19项通过但 EndTask 后 host -11 均仍 FAIL。不要重放旧崩溃、加诊断
shim 或检查 dump；继续普通平台合同修复和新的正常功能验收。这些测试不替代已推送
的多 region 分配器和旧80MiB原生验收，也没有新的真机 chatgpt.com 通过证据。

续接顺序：host FEP密码遮罩与401 UI，以及下载其余异常验收；
随后审查合入外部 CookieStore，再处理 IPv6/H2 的 EKA/OpenC 前提和 IAP proxy。
候选目录分别为 `/workspace/symbian-cookie-store-work`、`symbian-ipv6-http2-work`、
`symbian-iap-proxy-work`，尚未进入共享实现，禁止 bulk 覆盖 WebViewImpl 新 touch/FEP hunk。
之后继续清单 P2/P3，不能宣称所有 Belle 能力已经补全。

运行环境仍用 `/workspace/eka-validation/env.sh` 和 `symbian-media-work/env.sh`；
运行 ICU 始终为 `/workspace/symbian-idna-work/icu` 的 `808fc9bd…` 数据。
本轮已重启 Xvfb :95、Pulse 和 loopback fixtures；新会话检查实际进程/锁，不抄 session ID。
4CPU/16GiB/32GiB，剩约2GiB。不要 reset/clean 或 git add -A。
10月30日 tar/bundle 是旧快照，不包含本节新改动。

---

更新：2026-10-30，Asia/Shanghai。用户最新指令是“先到此暂停，更新交接文档，方便新会话接手”。本次只做状态核对和保存，没有继续实现、构建或功能测试，也没有推送。

**新会话先读本文，再读对应功能的证据。此前上传的交接说明是已经丢失的旧环境记录；其中“没有 commit/push”“C_LOOP only”“未接 GPU/媒体”等描述已经不代表现在。`Handoff.md` 是 2.32 历史记录，`Port-2.52.md` 是初次重建记录，不能据此回退当前实现。**

## 1. 用户目标与当前暂停范围

用户要求依次推进：

1. 参考 2.32 分支，为 2.52.6 接 GitHub Actions，推送自动产出安装包。
2. ARM11 接 Baseline JIT 和 DFG。
3. hardware compositing、WebGL、media；尽量用 Symbian^3 新增音视频 API，必要时修 SDK/模拟器。
4. 分析真机日志，包括系统仍有 200+ MB RAM 时的内存不足。
5. 盘点 Nokia Belle FP2 能支持但端口尚未支持的 WebKit 能力，按优先级补全。

前几项已有实现和模拟器证据；Belle 第一批已推送，第二批正在本地验收。**不能把所有目标写成完成，也没有新的真机验收结果。** 新会话收到继续指令后，从第 8 节的断点接着做，不要重新移植已有功能。

用中文交流。WebKit 目标仓库 `https://github.com/yeatse/WebKit-Symbian`，分支 `2.52.6-symbian`，当前没有 PR。SDK/EKA 的改动以导出补丁保存在 WebKit，SDK/EKA 仓库自身尚未 commit/push；不要把旧版 Handoff 的分支和提交约定套过来。

## 2. Git 状态：最重要的续接信息

| 位置 | 分支/HEAD | 状态 |
| --- | --- | --- |
| `/workspace/WebKit-Symbian` | `2.52.6-symbian`，`0b2858e230f038a51821ed197127480fa43fbea1` | 主工作树干净；此提交已推送，CI41 的源提交也精确匹配它 |
| `/workspace/WebKit-Symbian-graphics` | `belle-fp2-capabilities` | **当前开发树**，包含下述两笔本地实现提交和大量未提交/未跟踪的第二批文件 |
| `/workspace/WebKit-Symbian-arm11` | `arm11-jit-dfg`，`0514c15e427` | 历史工作树，不是当前入口 |
| `/workspace/WebKit-Symbian-belle-quota` | `belle-fp2-quota`，`0a8465ba573` | 历史配额工作树，不是当前入口 |

暂停前最后两笔实现提交：

- `df7274dac90`：EKA 0025/0026、原生剪贴板文件契约、实际 SIS 安装/注册长度证据。
- `7201187f295a5df4e29636bc4452574a0ce34620`：RSA/EC 后端、完整构建、真实浏览器验收，以及 runner 的完成后观察时间选项。

这两笔**没有推送**。本次交接仅另作一个文档提交；其最终 HEAD 见交接快照的 `state.json`。实现基线仍为 `7201187f295`，不要把文档提交误认为新功能构建。

暂停前实现 WIP 有 42 个已跟踪文件修改、247 个未跟踪文件，约 0.9 MB 的未跟踪内容；包含下载、touch、401 UI、剪贴板、内存压力和网络状态代码/测试/证据。**不要 reset/clean，不要 git add -A，不要仅保存 tracked diff 就认为备份完整。** 文档更新增加的文件另算。

最近确实写入共享源码、但不在 C11 二进制中的变化：触摸滚动修复已 `git apply`；不是只有候选补丁。它修改 `ScrollingEffectsController.cpp` 和 `WebViewImpl::scrollAt`，还没构建 C12。触摸/剪贴板 fixture 另有等待 load + 两次 rAF 的修正，避免初始 980px viewport 导致误点。

快照位置：`/workspace/symbian-handoff-20261030/`，打包为 `/workspace/WebKit-2.52.6-Symbian-暂停快照.tar.gz`。其中包含本地提交 bundle、WIP tracked diff、所有 WIP 文件副本/哈希、完整外部候选 patch 和未完成键盘草稿；不包含 SDK、固件、整套运行日志、构建二进制或测试凭据。本文另存 `/workspace/WebKit-2.52.6-Symbian-交接.md`，可直接给新会话。

本地没有 `origin/2.52.6-symbian` remote-tracking ref，不要因 `rev-parse` 失败误判远端分支不存在。需要时用插件或 `git ls-remote` 核对。

## 3. CI 与安装包：区分三个来源

### CI41 已全绿，但 artifacts 还没下载核验

- Run：<https://github.com/yeatse/WebKit-Symbian/actions/runs/40964125695>
- Job：`123764140026`，`completed / success`。
- Source：`0b2858e230f038a51821ed197127480fa43fbea1`。
- JSC、WebCore、browser、SDK 修复检查、SIS packages、Upload SIS packages 全通过。
- 预期包版本 0.3.41；仍须下载实际 SIS 后核实版本和 payload。

GitHub 返回的 artifact 元数据：

| artifact | ID | ZIP bytes | GitHub digest |
| --- | ---: | ---: | --- |
| sis | 12939729959 | 33,626,643 | `96cb72cfaa7f1634643311e2344e3c08282a074e59b77a5a54098f8347741e8f` |
| elf | 12940483515 | 34,898,556 | `7350eb9f148913d40f42ab020af76183ae4b53d3a91277b2c46f56b6f3f87b54` |

`/workspace/symbian-ci41-validation` **不存在**。暂停前虽派出了核验任务，但没有交付；不要写成已校验 ZIP、已装 CI41 或已运行 CI41。上一条“避免推送取消运行中 CI41”的等待条件已解除，但本次按用户要求暂停，未推送。

### 本地 C10 的 0.3.41 三个 SIS 已实际安装通过

证据：`/workspace/symbian-belle-work/release-0.3.41/install-after-0026`，仓库归档 `validation-belle/package-0.3.41/installation-after-0026/`。

- 3 次正常 host 退出；13 个 payload 的实际内容/哈希、13 个注册长度全部正确。
- 5 个 `!:` 路径确实安装到 E；8 个 C 固定文件和 5 个 C shadow 保持不变。
- 旧 host 的 registry length 失败记录完整保留；修正来自 EKA0026。
- **这是 local C10 SIS，不是 CI41 artifacts，也不是 C11。**

| local C10 SIS | SHA256 |
| --- | --- |
| browser.sis | `15ea260aadeb1b8b75d9f47db26c78ad4a82d89035bd21ab935dfd9d71787710` |
| jsc.sis | `b32d1d49befbf977c591bb25a830688cd04f3676553690432e24adcd6d3265ad` |
| symbian-webkit-icu.sis | `5a2ad94da2911a9d586466dac58f40dd61af14c43c73a6613f44ed77a3e36692` |

CI40 的下载、payload、实际安装及 smoke 证据已在 `validation-release-0.3.40/`，可作为核验流程参考，不必重做。

## 4. 已完成能力及真机问题的准确结论

当前清单：`Tools/Symbian/docs/Belle-FP2-capabilities.md`。其中第二批段落比若干表格行新；表格里“下载被忽略”“无 DOM touch”“认证没有 UI”等还是开发前状态，续接时应按实际验收更新，不要据此重复实现。

已有证据包括：

- ARM11 LLInt/Baseline/DFG；原生 EGL/GLES2 compositor；WebGL 1。见 `ARM11-JIT.md`、`GRAPHICS.md`。
- MMF 音视频、Symbian^3 API/SDK 修复、模拟器 OpenUrlL 等补齐；实际 PCM 频谱和视频像素/HTTP/TLS 证据。见 `MEDIA.md`。宿主 FFmpeg 支持不代表真机支持同样的编解码器。
- 第一批 Belle：localStorage/BroadcastChannel 20 项；sessionStorage/Worker/后端 17 项；SameSite 两进程 28 项，HTTPS/WSS 6 项回归；IDB 配额跨三次启动 16 项，20 MiB Blob 重启逐字节读回；原生单文件上传/prompt UI；WOFF2 7 项、系统字体两次各 7 项。
- AES 浏览器 29 项和 ARM32 CTR 35 项；第二批新增 RSA 浏览器 33/33、EC 30/30（下节）。

真机日志的两个问题已经分开处理：

1. ICU IDNA 数据缺失已修复构建/打包；运行数据应为下节的 `808fc9…`，不是旧的裁剪数据。见 `validation-idna/`。
2. `log-1.txt` 在 chatgpt.com 加载后触发 `LocalAllocator.cpp:150 / MemoryExhaustion`，即新 JSC marked block 分配失败。旧 aligned pool 只有一块最多 64 MiB reservation，是已证实的软件容量/碎片边界；新实现按需增加独立 region。最终原生压力验证保留 80 MiB，JSC 集成保留超过 72 MiB 活 JSCell，另修正 SDK pagemap 与小线程栈上的大临时对象。

**原日志没有足够计数，不能断定手机那一次具体是池满、碎片、物理 commit 或进程地址空间限制；系统空闲 200+ MB 不等于该进程能够成功分配。也没有用户新日志证明 chatgpt.com 真机已恢复。** 详见 `validation-memory/README.md` 及其 final-pagemap 证据。

## 5. C11 与 EKA 当前产物身份

### C11：最后一个完整构建、已验收部分功能的浏览器

不可覆盖的保存目录：`/workspace/symbian-belle-work/candidate11/`。

| 文件 | bytes / SHA256 |
| --- | --- |
| browser.exe | 58,792,332 / `20f58cef43d6fa4252546cb337940bfa609ba7ef71d0059ef1336bd126ef5288` |
| browser_ui.rsc | 192 / `43080e0ff913e6490045daf9a3a0cb362bad29dd4f4e5d3800bd6dc19afe2d48` |
| browser_reg.rsc | 97 / `b0a97bcbaa79c250f78611f8a0e976c44e085cdb19dbbfe83fa8fbaae78e8056` |
| browser_loc.rsc | 74 / `30724f35615879bf281c61e1a079497b0c938822d3fc25098b180ee275c58a00` |
| libraries.json | 59 / `d471e7e47cff37252dc9c741bdb82ce9cac4d4d68c48f504d544a48fb0cf5534` |

`source/`、`source-identities.json`、`tracked-source.patch`、`build-identities.json` 记录 C11 源码和构建身份。C11 包含下载/touch/401/clipboard/online/memory/RSA/EC；**不包含 CookieStore、IPv6/H2、IAP proxy、新 scroll 修复或键盘草稿。**

完整引擎构建日志：`/workspace/symbian-belle-work/p1-full-engine-build*.log`（最终 `resume3` 完成 262/262）。最终 browser 日志 `/workspace/symbian-belle-work/p1-build/browser-build.log`，链接完成 2/2。中断/最初错误日志都保留，不把它们当最终失败。

引擎 build：`/workspace/symbian-graphics-work/build/webcore`。
浏览器 build：`/workspace/symbian-graphics-work/build/browser-belle11`。
后续使用 **browser-belle12 / candidate12**，不能覆盖 C11 后再让旧验收指向新字节。

链接 ICU：`/workspace/symbian-work/icu`；运行/打包 ICU：`/workspace/symbian-idna-work/icu`。后者 `icudt70l.dat` 为 10,261,824 bytes，SHA `808fc9bd37589371288b409a8433cf3434ba792ca9d640109c5da41454895d0d`。不要把旧数据重新打包。

当前 libmbedcrypto：`/workspace/symbian-sdk/lib/armv6/mbedtls/lib/libmbedcrypto.a`，SHA `282b081f833b6f93a8a677919bf6466725ba9bd76b6c3f03df2d9c6cf3923627`。

### EKA：已构建到 0026

仓库 `/workspace/EKA2L1-validation`，基线 `5bbc0435cf9a3a3d0f08d20259d35f60255c3a8a`；本地改动未提交。当前 `build/bin/eka2l1_qt`：65,474,624 bytes，SHA `0d2714494b322684872c0fc036c2f206b8514e54d021dc2ec1e3182af5855b98`。

WebKit 中导出补丁目录 `Tools/Symbian/patches/eka2l1/`，最新：

- 0025：file sharing / read-only / rename / temp ownership 等普通文件契约；115 host 检查、实际 TU、补丁链检查。
- 0026：SIS registry 使用 `uncompressed_len`，不是压缩 `len`；16 字段检查及实际三包安装验收通过。

构建和身份：`/workspace/symbian-belle-work/host-0025-0026/`。

**每次 EKA host 构建后，都要恢复已经验证的原生 MMF DLL/map**，构建可能覆盖它们。来源 `/workspace/symbian-media-work/native-patches-20261029T151914Z`：

| 产物 | SHA256 |
| --- | --- |
| audio DLL | `a07674f2957410a00f0bcdbc997590feca50eb028147a1497934713d8aa65d49` |
| video DLL | `03fd46f1ce54a2b5863fe841e87bdc17b99fbd05ef99459f5a56b7dbd75209c3` |
| audio map | `5fadc737e5182ddd24230885af23717cd7537daf7427501ff09e43a9e40df5d3` |
| video map | `1d0f3e76bcbc25d65207b0cb5674bcbbc9c19e58bdf0959b2d3ac0f7f9824282` |

SDK `/workspace/symbian-sdk`，0.3.0 / Clang 23.1.2；源码 `/workspace/symbian-work/symbian-platform`，HEAD `b105f752f84d1b5640052f64c4a703017810d7b8`，本地有改动。修复导出在 `patches/sdk-0.3.0/`、`patches/symbian-native/` 等目录；没有推送 SDK 仓库。

## 6. 第二批验收：PASS、FAIL、未运行必须分清

### 已通过

- **RSA 33/33**：`/workspace/symbian-belle-work/p1-validation/rsa-browser-c11/result.json`；run token `e487ceced5a64403adba4e022bcbce36`。24 个 WPT 加 9 个集成检查，含 DedicatedWorker。
- **EC 30/30**：同目录 `ec-browser-c11/result.json`；token `db247b2968a74aa4aadcbc9ebf6e1b8f`。
- 两者都用实际 C11、正常 launcher/host 退出、严格 validator；仓库 `validation-belle/{rsa,ec}/browser-c11/` 已提交。host/native 检查与浏览器证据分开记录。CryptoKey 持久包装/结构化克隆仍未支持。
- **原生 clipboard adapter 31/31**：`/workspace/symbian-clipboard-work/adapter-runtime-host0026-run2`，guest/host 都 0，原 clipboard 文件不存在的状态已恢复；仓库 `validation-belle/clipboard-adapter/` 已提交。它不证明网页键盘粘贴通过。

### Touch：部分通过，pan 失败；修复已合入、尚未复验

- tap：`p1-validation/touch-tap-c11` PASS，单触点事件、兼容 mouse/click 各一次。
- 第一轮自动 suite 因初始 viewport 980 坐标计算错误而失败，原记录保留，不能当生产缺陷依据。
- 修正 fixture 等待 load + 两次 rAF 后，`p1-validation/touch-sequences-c11-stable-viewport/result.json`：prevent-start PASS、prevent-move PASS、**pan FAIL**；真实 viewport 360×532，取消事件正确但滚动区没移动。
- 根因候选：精确手指 delta 走了非 kinetic 的额外 smooth animation 分支。`/workspace/symbian-touch-scroll-fix-work/touch-scroll.patch` 已合入当前工作树：`scrollAt` 标记 precise，仅 Symbian 的 precise event 绕过额外平滑动画，离散滚轮/其他端口不变。
- 两个 ARM 语法检查和三种提取方法语义检查通过；**没有 C12 构建，没有浏览器证明问题已解决**。保留原 `scrollTop > initialScroll + 5` 断言复验。
- pointer-pan、touch-action:none、capture、导航取消还未完成；实际多指和真机能力仍待验收。原生 pinch 尚未实现。

### Clipboard：网页键盘第一例失败，不能用 native PASS 代替

- `p1-validation/clipboard-keyboard-c11-calibration` 是超时校准，没发送 Ctrl+V。
- 实际 run `p1-validation/clipboard-keyboard-c11-full`，token `clipboard-keyboard-c11-full-01`：真实 Start、XTest Ctrl+V 已发送，45 秒没有 paste，**FAIL**。普通 Qt close 停止剩余观察，runner 正确判失败；原 clipboard 已恢复。
- 已发现两层缺口：浏览器按 `iCode` 先做 VK 转换，Ctrl 字符 1..26 可能被丢；EKA 当前 keyboard 仅维护 Shift，default bindings 缺 Ctrl/A/C/V/X，无映射键不投递。宿主 XTest 成功不证明 guest 收到 Ctrl。
- `/workspace/symbian-keyboard-fix-work/{before,overlay}/` **只有两个 cpp 的草稿**，没有完整 patch、ARM/helper 检查或交付报告，不要整份覆盖有新 scroll 修改的 `WebViewImpl.cpp`。
- `/workspace/symbian-eka-keyboard-work` **不存在**；派出的 EKA 修复未交付。后续需正常实现修饰键/普通键投递，不能破坏模拟器快捷键/FEP。浏览器要区分 Ctrl+M 与 Enter 的 scan code，DOM key 保持字母，未处理 Ctrl 不应插入普通文本。
- 输入驱动 `/workspace/symbian-clipboard-browser-work/drive.py`；真实 9 case keyboard 和 menu 均待完成。root wrapper `p1-validation/run-clipboard-preserved.py` 保存/恢复剪贴板；不要用 DOM 脚本代替真实 paste。

### online/offline：页面 19 项通过，但整轮 FAIL

`/workspace/symbian-p1-system-runtime-work/online-c11-1/`，token `2cd5507048d64ffd87998c40b67c9cf0`。

实际连接 Stop/Start/Attach/Close 及 4 个 trusted online/offline 事件、页面 19/19 通过。随后 controller 通过官方 EndTask 正常请求退出，host 以 **-11** 结束，因此整体和严格 validator 都为 **FAIL**；没有强制清理冒充成功。原 profile、资源和文件状态恢复成功。

仓库 `validation-belle/network-state/browser-c11/` 是未提交归档；README 的哈希可能还没加入 manifest，最终整理时补齐。完整 host 原始日志保留在外部 run 目录，没有复制进归档，归档只记录其哈希/大小。

这部分不能标完成。先前额外崩溃诊断曾被自动审核以“可能涉及网络安全风险”中断，已向用户说明并停止；不要恢复旧诊断 agent、增加诊断 shim、重放旧崩溃或分析 dump。保留当前正常功能运行的失败记录，继续其余普通移植工作。

### HTTP Basic/Digest：只完成空用户名查询校准

工具 `/workspace/symbian-http-auth-runtime-work/{fixture.py,drive.py,validate.py,README.md}`；host fixture 6 项检查通过，不等于 guest 验收。

唯一实际浏览器 run：`runs/auth-basic-success-c11-01`。点击 Start challenge，看见正确 origin/HTTP sign-in/Username 原生查询；提交过空 FEP editor。**没有输入用户名或密码，没有发出 native Cancel（尝试时 host 已到正常观察期限关闭），没有认证成功证据。** `calibration.json` 明确是 `CALIBRATION_ONLY / INCOMPLETE`。

实测当时主窗口 `[1,25,900,700]`；Start 主窗口内 `[450,226]`；FEP `[241,185,400,300]`，edit `[80,40]`、Submit `[200,277,400,300]`；native OK/Cancel 主窗口内 `[364,331,540,331]`，editor `[450,278]`。新启动先核实尺寸/画面，不能盲用坐标。

完整 Basic/Digest × success、cancel-user/password、retry3、navigate-user/password、exit-user/password 共 16 轮都未验收。沿用真实 UI/本地临时凭据；每轮新目录/token，不复用超时校准为成功。

凭据在 `credentials.private.json`，不要打印、归档或提交；关闭 curl trace，`SCREENSHOT=null:` 丢弃 runner 自动截图，不保存密码/FEP阶段截图。最终遮罩输入、原生 query 关闭和退出检查仍需真实观察，不能由资源源码推断通过。

### Downloads 与 memory-pressure：工具准备好，浏览器验收未运行

- 下载工具 `/workspace/symbian-download-runtime-work/README.md`：POST once、GET、gzip、chunked、取消保存、取消传输、同名拒绝 7 例。独立本机 fixture、真实 XTest、前后目录/字节校验器准备好；4 工具及 14 validator host 检查通过。尚无实际下载通过结果。先校准 native Save UI，用独立 `C:\Data\DownloadRuntime\TOKEN\`，POST 不重放，同名文件不得覆盖。
- 内存/在线驱动 `/workspace/symbian-p1-system-runtime-work/README.txt`、`run-memory.py`、`runtime_support.py`、`qt-launch-process.py`。已固定 C11 resource 身份，不能混 C10。memory 尚未执行。
- memory 须通过真实 Qt Emulation → Launch process 在**同一个 EKA 实例**启动 worker；`ui-config.example.json` 仍未校准，禁止改成 true 假装测过。真实 RChunk 压力、JSCell≥80 MiB、1,572,864 cells/8 MiB buffer、保留 40 秒及正常结束都要验收。
- worker：`/workspace/symbian-memory-pressure-work/browser-worker-build/build/e32/memory_pressure_probe.exe`，SHA `73e73b4bb778a0bd299c161a334c4f4fdf913908e7555026d6621318be9fe10f`；退出 helper `/workspace/symbian-ui-work/app-exit-build/e32/browser_exit_request.exe`，SHA `e668e662f15b563b71e5237b910db422f86fc265de7f6a88925b310776e7489d`。

## 7. 下一批外部候选：均未合入当前浏览器

| 候选 | 交付与检查 | 必须保留的边界 |
| --- | --- | --- |
| CookieStore | `/workspace/symbian-cookie-store-work/cookie-store.patch`，38 文件；6 ARM TU、真实 SQLite 方法、observer ASan/UBSan、4 fixture、重放通过 | 未完整链接/guest；document provider、get/getAll/set/delete/change、expiry 已候选实现；无 SW manager/partitioned，visibility baseline 限制见文档 |
| IPv6/HTTP2 | `/workspace/symbian-ipv6-http2-work/network.patch`，22 文件；SHA `9366905a13a7f9db94b3432ae1c29394b4102c0a04da9cf4c06bbe845587b3c8`；ARM curl/nghttp2/CurlContext 和 SDK ELF link、host IPv6/H2 loopback 通过 | host 使用 OpenSSL，不证明 guest mbedTLS；无 Belle 验收。nghttp2 1.68.0；连接 8/8/4、streams 8、window 256 KiB、header 64 KiB 等预算保持 |
| IAP proxy | `/workspace/symbian-iap-proxy-work/iap-proxy.patch`，19 文件；SHA `ea159d312d9cc662c5caedcb010cfbb3ab881311484bedb82edb27f8e8f36c77`；7 ARM TU、46 策略和 4 host curl、重放通过 | 未完整链接/guest。官方 CommDB proxy reader、2s poll、不可用时 fail closed、每 handle 不变配置、切换取消旧请求且不重放 POST；407 尚无 UI，不得复用站点401凭据 |

IPv6 前置阻塞：`/workspace/symbian-ipv6-http2-work/abi-audit/` 有 `eka-ipv6-prerequisite.patch` 和 `openc-ipv6-prerequisite.patch`。EKA 对官方共享 KAfInet family 提前锁死 IPv4/resolver 强制 AF_INET，需要正常功能修复；候选两个 EKA object 编译、9 host API/layout 检查通过，**没有合入/全量 host build/guest**。OpenC 参考源码存在地址容量 `sizeof(pointer)` 错误，但实际 Belle backend.dll 的实现未确认，不能把参考补丁当固件已修复。

合并时按当前文件 hunk 合并；CookieStore 会动 `WebViewImpl.cpp`，network/IAP 都动 curl 配置。不要整份 overlay 覆盖本轮下载/auth/touch 改动。各目录 manifest/before-after SHA 是审查依据。

## 8. 新会话继续顺序

1. 确认用户已经要求继续；读当前 Git/status 与快照。不要覆盖 WIP，保留 C11。
2. 下载/校验 CI41 实际 artifact（纯核验，不把 local C10 结果移花接木）。
3. 完成浏览器和 EKA 两层 keyboard 修复；scroll 修复已合入，不要重复 apply。最小改动各自检查后构建 C12，保持正常测试权限/输入语义。
4. C12 重跑 touch pan 和其余序列、clipboard 9 例 keyboard/menu；同一断言，不降低通过标准。
5. 完成原生 HTTP auth、downloads、memory 的真实浏览器验收；online 退出 FAIL 单独保留，不冒称所有 P1 已完成。
6. 精确整理并提交已完成的第二批代码/证据/能力清单，再按目标分支 fast-forward/push；不要把未验收写成通过，不要无差别 stage。
7. 依序审查合入 CookieStore、IPv6/H2（先补实际 EKA/SDK前提）、IAP proxy，构建并跑真正平台路径。
8. 再进入 P2：Web Audio、Fullscreen/MediaSession/视频帧读取、地理位置/传感器/振动、SharedWorker/Cache API/StorageManager、系统证书/客户端证书/通知分享。P3 的 Service Worker、MSE/MediaStream/WebRTC/WebCodecs 是完整后端工作。WebGL2/WebGPU/FTL/Wasm 不属于简单开关补全；Belle GLES2 和 ARM11 有实质限制。

## 9. 环境恢复与运行约束

暂停核对时没有活动 EKA、ninja、fixture、HTTP server、Xvfb 或 Pulse 进程；只有历史僵尸进程。早先 agents 和 exec session ID **都不能视为仍有效**。`/workspace/symbian-eka-keyboard-work`、`/workspace/symbian-ci41-validation` 均不存在。继续时重新启动服务/agent，不要等待旧 session。

资源：4 CPU、16 GiB RAM、32 GiB 磁盘，暂停时剩约 3.7 GiB。避免不必要的完整复制、debug artifact 和并行大构建。

恢复环境：

```sh
. /workspace/eka-validation/env.sh
. /workspace/symbian-media-work/env.sh
export EKA2L1=/workspace/EKA2L1-validation/build/bin/eka2l1_qt
export ICU_DIR=/workspace/symbian-idna-work/icu
```

前者设置 host 工具、XDG 隔离目录、DISPLAY=:95；后者设置隔离 Pulse 路径，不会自动启动服务。先检查 :95 的旧锁/PID，确认无活进程后再处理 stale lock，重启 Xvfb 1280×900 和已有 Pulse 配置。guest data `/workspace/eka-validation/xdg/data/EKA2L1/data/`，固件 RM-707。

常用旧端口（暂停时全未运行，重启前核实）：touch8783、auth8794、downloads8800、clipboard8802、RSA8805、EC8810。fixture 启动方式见各 README，不要让多次 run 复用同一 token/输出目录。

引擎构建环境 `. /workspace/host-build-env.sh`。C11 的 browser 构建变量供 C12 参考：

```sh
SYMBIAN_WEBKIT_WORK=/workspace/symbian-belle-work/p1-build \
SYMBIAN_SDK_PREFIX=/workspace/symbian-sdk \
BROWSER_BUILD=/workspace/symbian-graphics-work/build/browser-belle12 \
WEBCORE_BUILD=/workspace/symbian-graphics-work/build/webcore \
DEPS_DIR=/workspace/symbian-work/deps \
ICU_DIR=/workspace/symbian-work/icu \
SYMBIAN_NATIVE_BUILD=/workspace/symbian-work/symbian-native \
JOBS=3 bash /workspace/WebKit-Symbian-graphics/Tools/Symbian/scripts/build-browser.sh
```

先确认脚本/生成的 CMake cache 指向当前 feature 源树；这里不是让新会话无条件立即构建。若引擎源已变，需正常更新引擎库后再链接。thin archive 的 `.a` SHA 不代表所有外部 object 字节，最终 E32 身份才绑定链接结果。

运行须 root 独占 `.jsc/.browser` 锁。使用现有 `run-browser.sh`：`WAIT` 是硬期限；新选项 `EKA2L1_COMPLETION_GRACE=1..60` 只有配合**最终成功标记**才可用，禁止用 READY 提前结束。它仍要求正常 host 退出。官方 EndTask 生命周期测试使用对应 observer，不把 WM close 当作 guest 正常退出证明。

保留失败原始证据，不裁剪后重新命名 PASS；需要减体积用 gzip 和哈希。不要发布临时密码、Authorization trace、私钥、profile DB、原始剪贴板文件或内存 dump。认证 `SCREENSHOT=null:`；状态保存/恢复 wrapper 不应省略。

## 10. 快照恢复说明

同一工作区：直接读现有 feature 树和外部工作目录；快照用于校验/防丢，**不要再次 apply 到已有 WIP**。

新机器且只有远端：先取得远端基线 `0b2858e230f038a51821ed197127480fa43fbea1`，验证快照 `local-commits.bundle`，在新分支导入其中的 `belle-fp2-capabilities`。随后根据 `worktree-files.json` 将 `worktree/` 文件恢复到新 checkout；`tracked.patch` 是同一 WIP 的另一个表示，不能再重复应用。该快照没有删除项时不需要推断删除；以 manifest 为准。

外部候选在快照 `external/` 中；完整运行证据和二进制仍主要依赖当前 `/workspace`，如果环境不保留，恢复源码后应重新构建/运行，不能继承旧路径就声称新环境验收通过。固件与 SDK 也需另行准备。

快照 `state.json` / `SHA256SUMS` 给出实际提交和文件身份。文档和源码备份不改变前述 FAIL/未验收状态。
