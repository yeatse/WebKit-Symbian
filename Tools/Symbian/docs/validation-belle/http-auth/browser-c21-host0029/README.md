# C21 / host0029 原生 HTTP 认证：12 个普通场景通过

2026-11-01，Basic 与 Digest 各自完成 success、retry、cancel-user、
cancel-password、navigate-user、navigate-password，合计 **12/12 普通场景 PASS**。
四个 `exit-user` / `exit-password` 场景没有执行或验收；**这不是 16/16 认证矩阵通过**。
已知官方 EndTask 退出问题仍独立保留，不用普通 Qt 窗口关闭代替该项。

本轮使用固定 C21 浏览器及 host0029 / secret FEP：

| 输入 | SHA256 |
| --- | --- |
| C21 browser.exe | `496bce2b06bd9a131939dd15b0cd414d4959fb3b297c640e2ae65f85adb8663f` |
| host0029 | `07af513045bf26e200f7d711c682e219ee4c7cecc9e30f2fa3b08732fc1012e3` |
| native secret FEP | `fd6092f2ad61d39ccb47475229c367f5494fe68a8bd2bd8d0e04dc1bafcdc7ae` |

没有用后续 C22 网络候选或 host0030 的结果替换这些输入。验收范围是受控
loopback HTTP 的真实 AVKON 用户界面及 Basic/Digest 行为，不包含真机、TLS 站点、
代理 407、IAP 切换或跨站凭据缓存。

## 实际结果

| scheme | case | fresh run | 含凭据提交 / 接受 | 结果 |
| --- | --- | --- | --- | --- |
| Basic | success | a21bs501 | 1 / 1 | PASS |
| Basic | retry | a21br501 | 3 / 0 | PASS |
| Basic | cancel-user | a21bu501 | 0 / 0 | PASS |
| Basic | cancel-password | a21bp501 | 0 / 0 | PASS |
| Basic | navigate-user | a21bn501 | 0 / 0 | PASS |
| Basic | navigate-password | a21bm501 | 0 / 0 | PASS |
| Digest | success | a21ds501 | 1 / 1 | PASS |
| Digest | retry | a21dr501 | 3 / 0 | PASS |
| Digest | cancel-user | a21du501 | 0 / 0 | PASS |
| Digest | cancel-password | a21dp501 | 0 / 0 | PASS |
| Digest | navigate-user | a21dn501 | 0 / 0 | PASS |
| Digest | navigate-password | a21dm501 | 0 / 0 | PASS |

两个 retry 用例各提交恰好三次错误密码，收到最终 401；安静观察期间没有第四次
请求或输入框。Digest 的协议 challenge 请求数不等于密码提交次数。取消和导航
用例没有发送凭据；导航完成后父页面可继续接受真实输入。

每次 fresh run 都先由 Root 查看本轮空用户名原生 query，确认 `HTTP sign in`、
`http://localhost:8794` 与 Username 标签；再将仍为空的同一个 FEP 窗口恢复原位。
12 张 heading 图与本轮 PID、窗口、geometry、图 SHA 和人工批准文件逐项绑定。
密码场景另有 12 个实例的公开 `maskcheck` 九圆点审图：retry 各三次，success、
cancel-password、navigate-password 各一次。公开探针清空后才开始私有输入，
没有凭据阶段截图，也没有从上一轮借用人工观察标志。

完成原 HTTP 条件及五秒安静观察后，驱动点击真实父页面按钮；同一 run 的
`e.isTrusted=true`、top-parent 回调和 FEP absent 共同证明 query 已结束且页面可操作。
随后才向本轮 host PID 发送普通 Qt WM close。全部 12 次 input0、host0、正常关闭、
资源及原 config/profile 恢复通过；forced cleanup=false，无目标线程 panic。
所有 `permitted_background_shutdown` 均为空，本轮没有使用任何 Domino 豁免。

## 原始判定与校验器修订

每轮均保留以下原件，未覆盖或重标：

- `validation-initial.json`：原始 **INCOMPLETE**，HTTP 功能条件已通过，但尚无 Root
  对本轮原生 heading、遮罩及关闭观察的显式确认。
- `strict-validation-initial.json` 与 `strict-validation-rootreview.json`：未改的原
  校验器均为 **FAIL**，唯一失败原因是 `interaction must explicitly avoid secret screenshots`。
- `validation-rootreview.json`：在明确的本轮 Root 观察和公开图合同核对后，独立修订
  校验器给出的 **PASS**。

原校验器的 SHA256 为
`cfec60b977abc7c3d78209272237577d19defc24a14a8df1c202166e45d5da63`，未改。
独立修订只替换两条旧谓词：笼统的“任何截图即失败”改为逐文件、逐实例、私有输入前的
公开 heading / mask 图合同；笼统 panic 匹配改为保留完整日志的目标/未知 panic 判定，
限定后台退出类别仍须有完整成功条件和实际退出证据。WebKit panic、未知 panic、
USER/KERN、access violation、host fault 始终失败。本轮不存在需豁免的后台 panic。

原有身份、HTTP 响应、恰好一次成功提交、三次拒绝上限、取消/导航不得发凭据、
隐私、安静观察、UI 和生命周期条件均保留。另增加真实可信父页面输入的关闭证明，
不再把原有定时轮询响应当作真实用户交互。

## 观察工具的先前失败

`previous-calibration-failures.json` 保留两个公开校准失败的路径、原始 SHA 及摘要。
`a21bs301` 的 heading 已批准，但 Escape 后空用户名 FEP 未重新打开，保持 FAIL。
`a21bs401` 移开空 FEP 后立即截图得到旧窗口残影，Root 未批准；原 FAIL 未变。
后拍的纯公开空 query 图确认 Qt 重绘需要时间。v5 只在 capture 前增加一秒等待，
随后重新核对同 PID、窗口和 geometry；本轮 12 个 fresh run 实际确认该方式可用。
没有修改或重跑旧失败，也没有因此猜测 SDK 或 host 修复。

`harness/preexecution-freeze.json` 是运行前冻结记录，其 `runtime_executed:false`
保留历史原值；实际运行结果以本目录的 12 个 run 及 `manifest.json` 为准。
v4 的 host 检查数量不是 v5 的新运行证据；v5 本轮实际公开图合同另做了只读复核。

## 归档范围与复核

原始 guest/emulator 日志只保留绝对路径、大小及 SHA，未复制或读取其文本。
原 observer 包含原配置恢复元数据，本归档仅取白名单布尔值及运行身份到
`run-summaries.json`；没有原 profile/config 内容或逐项库存。私有 fixture 凭据、
Authorization header/trace、私钥和二进制未打开或归档。

`source-evidence-identities.json` 将已归档公开文件与外部原件绑定。
公开图、gate、approval、interaction、Root observation 及两种校验输出均保留。
源 harness 包含原校验器供审阅；重新执行完整校验需要当前运行的私有 fixture 和完整日志，
它们有意不在公开归档。这里的审计只读验证公开 proof 身份、图 SHA、顺序、关闭记录
和源冻结，不能据此从公开归档重新生成 HTTP 整体验收结果。

```sh
python3 -B audit-public-evidence.py
sha256sum -c SHA256SUMS
(cd harness && sha256sum -c SHA256SUMS)
```

上述只读复核不启动 guest、display、fixture server 或输入，也不读取凭据或原配置。
