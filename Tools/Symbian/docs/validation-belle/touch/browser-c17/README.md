# C17 touch7 真实回归：PASS

2026-11-01（Asia/Shanghai），root 使用已准备的私有保存恢复包装完成七例。
Root 工具 session 38834 退出0；suite 和 preservation 结果均为 PASS。

| case | actual input / runner |
| --- | --- |
| prevent-start | 0 / 0 |
| prevent-move | 0 / 0 |
| pan | 0 / 0 |
| pointer-pan | 0 / 0 |
| none | 0 / 0 |
| capture | 0 / 0 |
| navigation | 0 / 0 |

Browser 为 C17：0d91d91ea1e016b7725097bac0768a9aa2fdf9710b765795769d2da07608a1c8。
本轮 host：277db2b3e44c69d4b860b2f76cf194178e820556671ad32f4891fcae2cb4c2ff。
FEP DLL：aad1a4c6a4a52c70d3ac430d45dce671a8ff664071c921146bc65f754a612f6b。

每例有新的UUID、真实XTest输入、fresh READY/最终PASS、正常runner退出。
整个profile、部署资源、config和bindings均按原身份恢复；恢复错误为空。
恢复前 guest 已结束；clipboard未备份或修改。原/还原部署镜像可能是另一个候选，
它属于原状态恢复，不是本次运行C17身份的替代。具体before/after见preservation。

## 断言与原始证据

原 run-touch-c13.py 未改。克隆仅更新C17/host/FEP身份、新输出目录及FEP元数据，
原11条assert经AST比较保持，七例输入流程、页面断言和坐标映射均保留。
完整原fixture、原输入脚本、runner、helper及wrapper/preparation身份已归档。

suite/ 保留全部本轮输入ledger、before/after图片、case raw guest log与结果；
完整 .log 仅确定性gzip，不裁剪。preservation/保留实际恢复与配置身份结果。
Previous日志及原profile/config/bindings私有内容不复制。
archive-manifest记录源/归档长度与SHA，gzip解压后逐字节核对；
SHA256SUMS覆盖其他全部文件，归档未改原run。

input-identities记录全部15个运行pin及wrapper源码身份。小工具/资源/FEP DLL
复制精确匹配内容，大browser/host/ICU只保留运行时before/after验证的身份。
若host路径此后被0029等构建覆盖，只记录当前差异，不把新host套用为旧结果。

ui-identities记录每例PID、原记录的window geometry和全部实际图片SHA。
原drive-touch没有记录XID或capture-time图像SHA；本目录不补造它们。
图片SHA为归档时对保留原字节的校验，same-PID/focus保护由所保存helper执行。
FEP仅有运行DLL身份pin，这些触摸用例不证明原生文字编辑/FEP交互通过。

本轮只证明C17七个单触点序列；不替代C19下载、Save后菜单恢复、物理多指、
独立原生焦点取消、真机或CI41验收。本目录只做已结束运行的只读归档，
没有启动guest/display或修改共享源码。
