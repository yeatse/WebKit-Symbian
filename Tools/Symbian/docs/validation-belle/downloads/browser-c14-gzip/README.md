# C14 gzip 下载实际验收

2026-11-01（Asia/Shanghai），root 通过真实 XTest 和原生对话框完成本例；
本目录只归档已经结束的运行，没有启动 guest/display 或改写原始记录。

- Run：dl-z-d75249df5942，动态 localhost URL 见 state/ready。
- Browser：C14，SHA256 b222b858e58703c057c538a8ee65c642e7ceb0b20cdcad47ea523305c86da065。
- EKA：host0027，SHA256 b2ce947b4781d7253658552e157c6b1b342868effdfbe7bbae3f8c2993cd2fd8。
- Root 工具 session 96051 退出 0；runner/validator 退出 0，严格校验 PASS。
- 恰好一次 GET /gzip；线上压缩正文较短，落盘为解压后的 524288 bytes 固定 payload。
- Config、profile、资源和专用目录恢复通过，原始逐项恢复记录在 state。
- 实用 supervisor SHA256：15948267cf88385ffb072e90926e3543bb0fa6b8090f3d10810fec00069f7b80。
  Controller SHA256：0e4d0892169fb4366d7e54f98e8b03d6d647e850caf6f27aab779e4994a635b0。
  按本轮 state 的自 hash 核对实际工具，未使用旧 GET02 driver 代替。

gzip-c14-01/ 保留全部本轮 fixture/命令/输入/截图/前后清单/实际测试 bytes/
validator/state 和原始 guest log。完整 .log 确定性 gzip 压缩；archive-manifest
记录原始及归档 SHA256，解压后逐字节验证。SHA256SUMS 覆盖其他全部文件。
不复制 previous 日志、原 config/profile 或 private backup。

tools/pinned-inputs/ 每项都与本轮实际 state/ready/input ledger 的 SHA 核对；
browser/host 只记录身份，不放入 Git。c14/ 保存候选源码/构建身份。
本例不继承到 CI41 或其他候选，也不替代其他下载 case 的独立验收。
