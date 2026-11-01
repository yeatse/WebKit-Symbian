# C15 浏览器单调时钟与定时器 PASS

2026-11-01（Asia/Shanghai）。使用普通 `performance.now()` 和八次串行请求的 40ms
setTimeout，页面与独立 validator 均 PASS，标准 launcher 正常 Qt close 后退出 0。
原始 result.json、validator 输出和完整新日志原样保留，本次归档没有重新运行浏览器。

- run：`81ebcf24de174593a7128ba2c853d073`。
- C15 browser.exe：58,793,784 bytes，SHA256 `473679d5c12d7dfeb057955ebd2ec0291cdd5db4654b6a3a2c2dba7c31fb995e`。
- host：`277db2b3e44c69d4b860b2f76cf194178e820556671ad32f4891fcae2cb4c2ff`。
- 已部署 FEP：`aad1a4c6a4a52c70d3ac430d45dce671a8ff664071c921146bc65f754a612f6b`。

64 个同步采样及 timer 采样均非负、有限、非递减；八次 timer 间隔全部正向且小于 750ms，
依次约为 110、62、47、47、47、47、47、46ms，总计 453ms。原断言只要求至少六次低于750ms、
八次均有正向进展、整段低于6s，本次实际八次都达到。重复同步采样值允许正常计时精度钳制；
这不是每次调用必然递增、硬实时精度或真机计时校准证明。

`clock-c15/` 保留完整 fresh guest log、gzip host/runner log、原始结果/validator 输出及实际最终截图。
归档代理已使用 view_image 确认最终页面显示本轮 DONE PASS。标准 runner 正常关闭证明不等于
官方 EndTask 生命周期证明。本轮没有调用 clipboard/profile preservation wrapper，因此不声称对应恢复验收。

`fixture/` 的全部七个文件都匹配运行结果中的 fixture_hashes；无原生 test hook 或 precision bypass。
`candidate15/` 来自不可变 C15 目录，五个产物身份和72个源码快照文件均已核对；没有复制程序镜像。
`runtime/fep-deployment.json` 是原始部署记录，native_UI_verified=false，不能从本页推导 FEP Cancel UI 通过。
C15 的其他下载/媒体/内存能力也不能由时钟页面 PASS 推导。

`provenance.json` 记录原件与 gzip 对应的 bytes/SHA256；`SHA256SUMS` 覆盖全部保留文件。
未复制 previous-log、用户配置/profile、原始剪贴板或凭据；没有修改共享源码、构建或 guest。
