2026-11-01 独立只读审查：Root v14 有限 IAP canary

结论：未见阻塞本次固定普通 API 受控验收的剩余静态问题。审查对象为 /workspace/symbian-iap-root-harness-20261101-v14；只读源码与公开布尔结果，没有运行 guest、namespace、socket、测试或原生程序，没有读取原配置、profile、私有日志、数据库内容或 raw trace，也没有修改 Root 副本或共享仓库。

隔离与清理：原树/备份保持 RO，RW 例外来自独立 C/D/E、配置和 DATA 元数据视图、私有 ROM、明确的 J2ME SQLite DB/sidecar 副本、临时目录和 font cache。其余静态子目录 RO；cwd 与 Qt BASE 都有真实空 patch。HOME、Pulse cookie、缓存、/tmp、/run 使用私有路径，未放宽原配置父目录 RW。Root 外侧双锁由独立 session keeper 保持，启动 FD 类检查拒绝原 regular/directory FD 继承。追踪器以自有进程组做有限等待/失败清理；收集器终止、活体 PID 关联、guard 与审计失败不能变成成功。只有 stopped 且全局 no-host 才恢复；状态不明保留原件和锁，forced cleanup 为 FAIL。

隐私与判定：write/映射/xattr 缓冲区采用 raw 参数形式，路径片段仅在匿名 pipe 内解析，公开仅固定角色、调用计数、状态与布尔值。没有 raw trace 输出文件。native-public-record 先经严格白名单验证才写入公开输出；实际 IAP/Network/Service 值不输出。denied mutation、unsupported async、意外 peer、fatal、collector/guard 失败均保持失败。v12/v13 的公开 literal 索引只是诊断候选引用，存在 post-Popen 扫描不计入目标等待的限制；v14 已撤去该路径。

公开证据：Root14 result.json SHA256 ac540f294799bca980f32d4d9ff6483629a7966ffba6ca71192b4ce09f904c15，状态 NORMAL_API_SNAPSHOT_COMPLETE。snapshot 为 AVAILABLE，Start/Id/Network/Service rc 全 0、stable_snapshot/owned_handle_closed 为 true、timed_out 为 false；外层 host exit 0、guard/audit true、denied/async 0、无 fatal/force、host stopped、驱动与配置已恢复、ROM/J2ME 原件 bytes 与输入 pins 不变。成功私有工作区删除由 Root 报告，本审查不读取该私有目录。Root08/10/13 的外层 FAIL 保持原判，Root08 的单独有效 native 记录不等于整轮 PASS。Root 另报告 15 pins 与 72 WIP 不变，本审查没有重复原目录核验。

结论范围：证明这个固定 native canary 在受控 host0033 上正常 Start、GetIntSetting、Progress、双读比较及自有句柄 Close，并满足该轮外层健康/恢复要求。Close 不是强停共享接口；双读不是原子事务。routing_verified、storage_scope_verified_by_record 和 commdb_scope_known 仍为 false；Native CommDB、代理路由、SIS/手机与任意客户端依赖纯度均未由本次结果验证。OS containment 是已授权的隔离方案，不另增 closed-native-source gate。

冻结源码 SHA256：

- adapter.py: c57e68ff0b8eea527ec65eeaaf4629491c46ed34524a84d8133d87e07f9bcd59
- os_sandbox.py: 6240ac0be26b6574392425e96224fd940c9d9592af6d9f7334a379f0ad3960f0
- sandbox_guard.py: 01597566923823fda2c8b11c14aedf72c57df83aa308f0ebf5897d5f3d8f1d88
- validate.py: 667fa28ff1ce68efcf131ba410b7497755b74bf2480361e77a36862a6b7d3eba
- canary.cpp: 3d52a60725fd9f71fa59640fcdd13a50abb3d20f6fd0115cfd727d68cd0bfdfe
- native EXE 的 Root/adapter pin: 84716B / 3f52a5ca2f5241bbb719f740cbbdefe68e2865fcc447675bfe900b2d917a5895
