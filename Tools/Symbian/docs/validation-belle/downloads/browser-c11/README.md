# C11 下载：真实 POST 保存和原生保存取消

2026-11-01（Asia/Shanghai）由 root 独占 EKA，通过实际 XTest、原生 AVKON 保存 UI 和文件校验完成两例。
原始 `validation.json` 均 PASS；root 从实际工具会话确认正常 launcher/host 退出 0，并逐张观察原生 UI。
本目录只整理已有证据，没有重新运行功能，也没有改写原始日志、输入记录或校验结果。

| case | run | 结果 |
| --- | --- | --- |
| cancel-save | dl-cancel-cal-20261101-01 | 一次附件 GET；真实 native Cancel；terminal=1,error=0,cleanup=0；专用目标目录前后均为空 |
| post | dl-post-c11-20261101-01 | 恰好一次 POST `/post-attachment`，正文精确 `token=post-once`；原生选择目标并保存；terminal=0,error=0,cleanup=0；实际文件大小和逐字节内容正确 |

两个 run 使用 **C11 browser.exe**：
`20f58cef43d6fa4252546cb337940bfa609ba7ef71d0059ef1336bd126ef5288`，
以及 **EKA host through 0026**：
`0d2714494b322684872c0fc036c2f206b8514e54d021dc2ec1e3182af5855b98`。
这是旧 host 的普通下载结果，不能替代随后 keyboard host/C12 复验；也不是 CI41 包运行证据。
同版 UI resource 和构建身份保存在 `candidate11/` 元数据中，未存放二进制程序。

POST 文件保存在专用测试目录
`C:\Data\DownloadRuntime\dl-post-c11-20261101-01\payload.bin`，
524288 字节，SHA256 `33bc8aab40703678c3ebe94d2dd8f2afff285dd901f9234e841e4679f8204fd5`。
归档 `after-files/payload.bin.gz` 解压后是 `bytes(range(256)) * 2048`，完全由 fixture 生成；不包含用户文件。

每轮保留新的 guest log、gzip 压缩的完整 host/runner log、fixture ready/events、原始 validator
结果、before/after 目录清单、全部输入 ledger、校准/操作前后截图与计划。原生 FEP 只显示普通测试文件名，
没有认证、剪贴板或 profile 数据；之前的 `previous-log.txt` 明确排除。
`retained-files.json` 同时记录原始/归档路径、字节数、SHA256 和压缩方式；gzip 解压字节已核对原件。
`SHA256SUMS` 覆盖本目录全部文件（自身除外）。原始 validation 内绝对路径保留，避免改写原证据。

`tools/` 保留本轮校验器、fixture wrapper、快照和真实输入工具，`source/` 保留复用的仓库
fixture、XTest helper 与正常 runner；每个源文件都有哈希。原始 run 仍在
`/workspace/symbian-download-runtime-work/runs/`，没有修改已有失败记录。

GET、gzip、chunked、传输中取消、同名拒绝五例尚未在本批验收。七例整体不能标为完成。
