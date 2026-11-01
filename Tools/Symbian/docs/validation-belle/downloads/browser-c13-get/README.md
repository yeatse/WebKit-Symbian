# C13 普通 GET 下载实际验收

2026-11-01（Asia/Shanghai），root 用真实 XTest 和原生保存对话框完成 GET；
本目录只归档已经结束的运行，没有重新启动 guest/display 或改写原始记录。

- Run token：dl-g-6caef608f056；fixture URL 及动态 localhost 端口见 state/ready。
- C13 browser SHA256：fe75408d621a0c2cbe1f8882374d5289a1d0b43df8d0a1b6d407ac67e398cd08。
- EKA host0027 SHA256：b2ce947b4781d7253658552e157c6b1b342868effdfbe7bbae3f8c2993cd2fd8。
- Root 工具 session 58032 退出 0；新 runner/validator 都退出 0，严格校验 PASS。
- 附件恰好一次 GET；payload.bin 实际落盘 524288 bytes，SHA256 为
  33bc8aab40703678c3ebe94d2dd8f2afff285dd901f9234e841e4679f8204fd5。
- Root 逐步查看原生 UI 图片后提交输入，最后确认实际 Saved 路径。
  完整截图、输入/命令、原始 guest log、fixture 事件、前后清单与实际测试 bytes 均保留。
- config 与原 profile/资源/专用目录恢复成功；恢复结果逐项记录在 state。
  没有复制原 profile、原 config、测试前日志或私有备份内容。

这是 C13 + host0027 的 GET 结果，不继承到 CI41，也不替代旧 C11 的 POST/
取消保存验收。gzip、chunked、传输取消、同名拒绝仍须各自独立验收。

## 原始记录与工具身份

get-c13-02/ 保留本轮全部采证文件；完整 host/runner/driver .log 用确定性 gzip
压缩，raw guest log.txt 与 JSON/图片/实际 payload 保持原字节。
archive-manifest.json 同时记录原始及归档 SHA256，压缩没有裁剪日志。
SHA256SUMS 覆盖本目录所有其他文件。

tools/pinned-inputs/ 的每个文件都核对本轮 state/ready/输入 ledger 中的 SHA。
browser 和 host 体积较大，只记录精确身份，不放入 Git；小资源和工具保留。
candidate13/ 是该候选的源码/构建身份记录。

tools/get02-driver/run-case.py 是 get02 成功后、后续错误清理修复前保存的源码。
其唯一提前改动为 root 加 output mkdir 的 parents=True。provenance.json 同时
记录修改前 preparation hash 和 get02 实跑版本 hash。previous-preparation-
manifest/host-tests 是早先 7 项 host 检查的历史记录，不把它们冒称新版本实跑。

get-c13-01 的原 stdout 单行失败保存在 preflight-failure/：
输出父目录不存在导致 setup 失败，随后 restore/save 的 state.json.tmp 错误
掩盖了最初错误。Root 确认当时未创建该输出目录、未启动 guest、未改 guest 文件。
本目录保留失败，不把它归为生产下载功能失败，也不将它改成通过。

此后 observer 的 setup/cleanup 异常保留语义另有修复和 host 检查；
该新版本没有借用 get02 的运行结果，二者来源必须分开。
