# Classic WebAudio 的 SDK/stream 前提候选

基线 WebKit `0885f13ce4252a96b77d4c8cac0579ba2b8ba618`。本准备阶段仅生成外部候选/检查，未改共享 WebKit/SDK/EKA、未新装 prefix、未部署 DLL 或运行 guest/audio/display，也未提交或推送。`OptionsSymbian.cmake` 的 WebAudio 默认 OFF 原样保留。以下不是完整 WebAudio 或真机验收。

## 精确改动

| 文件 | 行为 |
| --- | --- |
| `.github/workflows/symbian.yml` | allocator 后运行 math helper；仅缓存独立 math work，命中仍重跑；两个相关 key 覆盖 helper/manifest/0007，导出 math JSON/log；E32 步骤加整栈回归 |
| `Tools/Symbian/scripts/rebuild-sdk-math.py` | 原数学 helper 逐字导入：真实三 profile ARM object、完整 math member/ABI 检查，默认只 stage；workflow 才显式 `--install` |
| `Tools/Symbian/patches/sdk-0.3.0/math/{manifest.json,README.md}` | 固定 source/header/member/profile；说明顺序与 work cache |
| `Tools/Symbian/patches/symbian-native/0007-Provide-C99-atan2l-in-the-real-guest-runtime.patch` | 真实 C99 atan2l，ARM long-double 等价断言后转发 libm double atan2；保留官方 C macro aliases |
| `Tools/Symbian/patches/symbian-native/0008-Support-explicit-MMF-client-DLL-identities.patch` | 仅导出 converter/MMF UID2 白名单扩展及 tests；ordinary/ECom/EXE/API默认行为不改 |
| `Tools/Symbian/patches/eka2l1/0031-Report-unsupported-stream-pause-and-input-bitrate.patch` | 仅导出两个 stream body 修复；Pause 不再破坏 queue 却报成功，BitRateL 诚实 Leave NotSupported |
| `Tools/Symbian/tests/sdk-native-patch-stack/test_patch_stack.py` | 原11例保留并扩展至15例：原6幂等、prefix1..7升级、partial math/MMF和未知修改拒绝；不改 product helper 算法 |
| 本目录三个文档 | source pins、实际检查、现有 producer 证据与限制 |

原 native0001..0006 及 `apply-native-patches.py` 逐字不变；新增0007/0008由现有完整 prefix 矩阵识别，不能回退逐 patch reverse-check。`rebuild-sdk-runtime.py` 原 whole-archive allocator guard 也不改。

## 跨 run 缓存顺序

候选 workflow 行36 定义 prefix；106–110只恢复原SDK，124–129的 **Save the original SDK** 是唯一 prefix save，且在132的首次 SDK 修改之前。不存在 `actions/cache@...` 隐式 job-end prefix save。其余26项 cache path 扫描见 `validation-summary.json`；没有覆盖 prefix 或其祖先目录的晚保存。

192行allocator → 207行math work restore → 213行math helper → 219行仅保存math work → 281行JSC消费者。新runner的暖缓存恢复的仍是原始SDK，所以旧allocator不会碰到上一run的math postimage。原SDK cache miss时也先保存原始release，再按序修改。CI不引用Root本地install-backups。

math work可包含stage/archive，但每次helper都以 **当前prefix** 重新写stage、替换math member并逐字比对其余所有成员，不能直接拷贝cached stage到SDK。真实重复staging还把外部旧stage换成sentinel，确认它被正确重建；全程未install。原allocator从固定git revision读取源文件（原脚本336行），而不是读取已被native0007修改的owner worktree。

当前本地prefix已装math时，**不能再运行旧whole-archive allocator rebuild**。本检查在独立小prefix中验证其正确拒绝math postimage，未放宽guard或丢弃math。若未来要缓存修改后的完整prefix，需要另外定义完整allocator+math postimage合同；本候选没有这样做。SDK原始cache必须继续在所有修改之前保存。

## 已完成的有限检查

- 15项实际整栈host测试通过，包括旧6系列幂等与prefix6缓存升级；SDK base为`b105f752f84d1b5640052f64c4a703017810d7b8`，14个前后source pins和8 patch SHA在`source-identities.json`。
- 同一数学helper实际 ARM staging default/streams/native_atomic64 三 profile；math member一致，其余102/113/78成员保持。缓存work重复staging亦通过；默认`installed=false`，四个已安装math文件及SDK/EKA原owner/index共22个选定文件保持。
- YAML/步骤/所有cache路径结构检查通过；原allocator的隔离math-postimage拒绝日志保留。没有派发GitHub workflow或重跑整轮CI。
- Root既有math0007实际安装记录只作为已有证据引用；本任务没有再次安装。
- 0008 producer 已有65 GTest、13 CLI合同、ordinary/ECom/EXE旧新字节一致、55冻结exports、3 DLL/101 importslots及fresh full recipe逐字复现。这里只核对既有记录身份，不重跑并冒充新的native运行结果。

最终EKA0031合并Pause/BitRateL的SHA为`0da2d048e53290a752e29b37956b33a4f81c1363f5b5a3f11e27dd18598e2f12`。producer `inputs.json.patch_sha256`保留的是更早最小Pause导出；本候选直接固定最终combined patch，并对实际built-source snapshot反向apply核对两个pre/post image，没有改旧记录。

## 限制与后续

math修复不是空符号；但ARM编译、C++类型和host转发数值不证明实际libm/audio运行。既有C模式ldexpl/scalbn问题没有在此扩张修复。

SDK0008仅接受原MMF UID2 `0x10003b19` 的显式DLL类型，仍限experimental UID3/capability规则；不把已构建stream DLL自动装到runtime。EKA0031依旧没有true DSP pause、资源注册/取消/WillResumePlay/CustomInterface；输出暂停由client选择明确Stop/re-prime，不宣称无缝恢复。GNU `.elf.map` 不得冒充ordinal override。

Root后续需另行review/apply候选、做完整CI/image与SDK部署审核，再独立验证native AO/线程/descriptor/PCM/中断、EndTask和Belle FP2真机；本候选不打开WebAudio默认开关。
