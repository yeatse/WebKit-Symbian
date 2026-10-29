# 交接：WebKit for Symbian^3 与 EKA2L1

写于 2026-10-29，给下一个会话。新会话以 [yeatse/EKA2L1](https://github.com/yeatse/EKA2L1) 为主仓库，再用 add_repo 挂上 [yeatse/WebKit-Symbian](https://github.com/yeatse/WebKit-Symbian)。两个仓库名字不同，可以同时挂。**不要再挂上游 EKA2L1/EKA2L1**：它和 fork 同名，挂了之后 fork 就加不进来、推不上去，上一个会话就卡在这里。

构建方法、目录结构和移植笔记见 [Development.md](Development.md)。这里只记那份文档里没有的东西：当前进度、本地环境怎么搭、怎么测、怎么查崩溃，以及约定。

## 约定（用户定的，继续遵守）

* 用中文回复。
* WebKit：直接推到 `2.32.4-symbian`，不开 PR（用户明确要求时除外）。
* EKA2L1：在 yeatse/EKA2L1 上开发。之前“永远不推 EKA2L1”的规定是针对上游的，现在作废。工作分支是 `webkit-symbian`，不用 `master`。
* 提交信息里不写模型 ID。结尾加会话给出的 `Co-Authored-By` / `Claude-Session` 两行。
* 没有确认之前，不要推 `symbian-v0.3.0` 标签。

## 当前状态

手机（N8/X7）上 HTTPS 正常。m.youtube.com 首页、YouTube 搜索结果、chatgpt.com、html5test.com 都能打开和滚动，用户确认过。最近几个提交：

| 提交 | 内容 |
| --- | --- |
| `83051a52` | 关掉 accelerated compositing。移植没有合成器，`GraphicsLayerSymbian` 是空壳；用 transform、`will-change` 的页面会建出没人接收的图层树，拆的时候在 `GraphicsLayer::removeFromParent` 里出现 KERN-EXEC 3 |
| `2fb286a7` | 页对齐分配（JSC 的 16 KB MarkedBlock）改用一块独立的 64 MB disconnected chunk（`FastMalloc.cpp` 的 `AlignedPool`）。SDK 的 mimalloc 在复杂页面上，提交量才 16 MB 左右时 `mi_malloc_aligned(16384, 16384)` 就返回空，导致 `RELEASE_ASSERT` 触发 USER-EXEC 3。调小 `mi_option_arena_reserve` 没用：SDK 启动时就保留了 48 MB |
| `8807f545`, `9c03b2fb`, `79fb433f` | 线程结束方式写进日志；curl 里初始化 PSA crypto；TLS 握手跟踪 |

## EKA2L1 补丁

原来的 `0002`–`0004`（esock 两种 `RSocket::Send` 参数布局、16 位描述符不再被当成 handle、`Math::Random(TDes8&)`）已经提交到 yeatse/EKA2L1 的 `webkit-symbian` 分支（`b2221fe`、`bac9ece`、`5bbc043`），并从这里删掉了。`0003` 影响面大（services 里有 81 处调用），只用浏览器验证过。

`Tools/Symbian/patches/eka2l1/` 里还剩两个，基于 `webkit-symbian` 分支 `5bbc043`，按顺序 `git am` 能干净应用（已验证）：

| 补丁 | 处理 |
| --- | --- |
| `0001` `--installdevice` | 命令行安装固件。没提交给上游；推不推到 fork 由用户决定，本地搭环境要用 |
| `0005` kernel：*-EXEC panic 时转储寄存器和栈 | **只在本地用**，不推。见“查崩溃” |

```sh
git checkout -b local origin/webkit-symbian
git am /path/to/WebKit-Symbian/Tools/Symbian/patches/eka2l1/000{1,5}-*.patch
```

## 本地环境（容器重建后要重新搭）

上一个会话的布局如下，新会话照着搭，路径可以不同：

* **SDK**：symbian-platform 0.3.0，放在 `/home/user/dev/symbian-sdk`，`export SYMBIAN_SDK_PREFIX=...`。沙箱代理会拦 GitHub release 下载（403），所以当时是从 hpnkv/symbian-platform v0.3.0 源码自己构建的。CI（`.github/workflows/symbian.yml`）能下载 release 包。
  * 只改 EKA2L1、不改 WebKit 的话，可以不搭 SDK：直接用 CI 产出的 `sis.zip` artifact（GitHub Actions，`browser.sis` 等）。
* **固件**：Nokia X7-00（RM-707）。ROM 包 `x7.7z` 是用户给的 Google Drive 链接（文件 id `1K3HCexRHgqatV7NqPtU5N8fr1KNGsCDs`），解压得到 `X7.rom` 和 `X7.RPKG`，然后：
  ```sh
  xvfb-run -a ./eka2l1_qt --installdevice X7.rom X7.RPKG     # 需要补丁 0001
  ```
* **EKA2L1**：在 `build/` 下用 cmake 构建，可执行文件是 `build/bin/eka2l1_qt`。数据目录是 `~/.local/share/EKA2L1/data`（`drives/c` 对应 C 盘）。构建时 lupdate 会改 `src/emu/qt/translations/*.ts`，这些改动不要提交。
* **WebKit**：`Tools/Symbian/scripts/build-webcore.sh && build-browser.sh && package-sis.sh`，产物在 `WebKitBuild/Symbian/sis/`。build-webcore 要跑十来分钟。

## 在模拟器里测试浏览器

1. 安装 sis（EKA2L1 报 "Installation done" 后就可以杀掉进程）：
   ```sh
   DISPLAY=:96 ./eka2l1_qt --device RM-707 --install browser.sis
   ```
2. **只在模拟器里做**：沙箱的出口网关会重签 TLS 证书，签发者是 "... Egress Gateway CA"。要把它的 CA 追加进模拟器的证书文件，否则会报 `BADCERT_NOT_TRUSTED`。不要改仓库，也不要改 sis：
   ```sh
   P=~/.local/share/EKA2L1/data/drives/c/resource/webkit/cacert.pem
   grep -q sandbox-egress "$P" || { echo "# sandbox egress CA (emulator testing only)" >> "$P"; cat /root/.ccr/ca-bundle.crt >> "$P"; }
   ```
   重装 sis 后要再加一次。
3. 运行（sis 装到 E 盘）。命令行参数前要留一个空格：
   ```sh
   Xvfb :95 -screen 0 1280x900x24 &
   DISPLAY=:95 ./eka2l1_qt --device RM-707 --run 'E:\sys\bin\browser.exe' " curl-verbose https://m.youtube.com" > eka2l1-run.log 2>&1 &
   sleep 200; DISPLAY=:95 import -window root shot.png
   ```
4. 看结果：
   * 浏览器日志在 `drives/c/Data/WebKit/log.txt`（上一次的是 `log.old.txt`）。带 `curl-verbose` 时，每 2 秒记一次 UI 线程状态、线程列表（含结束方式）和 mimalloc 用量。
   * curl 跟踪在 `curl.txt`。
   * 模拟器日志里搜 `Access violation`、`panicked`、`terminated`、`Unimplemented`。

用户在手机上测试时，会贴出带 `curl-verbose` 的 log.txt 和 curl.txt。

## 查崩溃

* CI 每次构建除了 `sis` 还会上传 `elf` artifact（`browser.elf`，保留 30 天）。用 CI 的包（包括用户手机上装的）查崩溃时，用同一次构建的 elf 来符号化。
* **KERN-EXEC 3（访问违例）**：EKA2L1 日志里会打出 `pc`、`lr` 和寄存器。browser.exe 的加载基址在 `browser.exe (UID3=0xE1A5C002) runtime code: 0x...` 这一行。ELF 地址 = 运行地址 − 基址 + 0x8000：
  ```sh
  llvm-symbolizer -e WebKitBuild/Symbian/build/browser/browser.elf -f -C -i 0x<ELF 地址>
  llvm-objdump -d -C --start-address=... --stop-address=... browser.elf
  ```
* **USER-EXEC 3**：通常是 `RELEASE_ASSERT` 或 `CRASH()` 走到 `WTFCrashWithInfo`。日志里没有 PC，要打上补丁 `0005`，日志会给出 `PANICDUMP` 和 8 KB 的 `PANICSTACK`。
  * 把栈上落在 `.text` 范围内的字换算成 ELF 地址后符号化，就是调用链。
  * 落在 `.rodata` 里的通常是传给 `WTFCrashWithInfo` 的文件名和函数名字符串。
  * 两个段的范围用 `llvm-readelf -S browser.elf` 看。
* **分配失败**：上一次是在 `tryFastAlignedMalloc` 里临时加了 `RDebug::Printf`（会以 `[Emulated.Stdout]` 出现在模拟器日志里），定位后已经删掉。

## 待办

1. AVKON 风格的表单控件（`RenderTheme`）。
2. 手机上的速度：先测量，再考虑滚动 blit 和局部 `-O2`。
3. 在手机上验证：桌面页面（980 像素布局、双击缩放），以及多个 HTTPS 连接共享 CA 链后不再出现 `mbedTLS -0x2880`。
4. 远期：基于 GLES2 TextureMapper 的硬件合成。N8 有 OpenGL ES 2.0 和 EGL，但 GPU 内存和纹理上传都是瓶颈，GOOM 会杀进程，所以要单独评估。
