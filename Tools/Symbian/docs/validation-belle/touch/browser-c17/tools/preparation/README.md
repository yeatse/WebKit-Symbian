# C17 touch7 回归：只准备，尚未运行

原 run-touch-c13.py 未修改。克隆仅更新 C17 browser、host277、FEP aad 身份、
新输出目录，并记录 FEP SHA。原11条assert除身份常量外经AST比较保持，
七个case/输入驱动/页面断言/坐标映射/期限/正常runner流程不变。
差异见 driver-identity-only.diff，证明见 assertion-provenance.json。

C17 browser SHA：
0d91d91ea1e016b7725097bac0768a9aa2fdf9710b765795769d2da07608a1c8
EKA host SHA：
277db2b3e44c69d4b860b2f76cf194178e820556671ad32f4891fcae2cb4c2ff
FEP DLL SHA：
aad1a4c6a4a52c70d3ac430d45dce671a8ff664071c921146bc65f754a612f6b

## root 执行

已有Xvfb/Pulse和8783 touch fixture server必须继续运行；本工具不启动服务。
本组命令尚未执行，root独占空闲runtime后用异步session执行：

    . /workspace/eka-validation/env.sh
    . /workspace/symbian-media-work/env.sh
    python3 -B /workspace/symbian-touch-c17-regression-prepared/run-preserved.py \
      --execute \
      --output /workspace/symbian-belle-work/p1-validation/touch-c17-preservation-20261101 \
      > /workspace/symbian-belle-work/p1-validation/touch-c17-driver-20261101.log 2>&1

省略 --execute 只校验输入并返回PREPARED_NOT_EXECUTED；不创建run输出、
不动guest/config，不连接display或启动进程。
准备仅做语法解析、assert AST比较与实际文件SHA核对。
独立只读审查未发现锁继承/恢复/未执行误PASS阻塞。

Suite新输出：
/workspace/symbian-belle-work/p1-validation/touch-sequences-c17-regression-20261101
外层保存恢复结果：
/workspace/symbian-belle-work/p1-validation/touch-c17-preservation-20261101/result.json

目录已存在时拒绝覆盖；重试另准备新输出名克隆，不删除或改判旧结果。
C17相邻UI资源242 bytes已单独pin，不能借旧192-byte资源。

## 保存恢复与边界

外层独占jsc/browser双锁，用SavedFiles私下保存/恢复整个CDataWebKit profile、
Csysbrowser、reg/loc/ui/ICU/fontconfig。EKA config和bindings原目录按inode rename，
运行时使用副本，最终恢复原文件/目录。settings-restoration.json逐项核对。
原profile内容不读取/发布；touch不需要clipboard，不进行clipboard备份/修改。

原driver未改FD调用；外层只在runpy期间给Popen追加已持有锁FD继承，并把
EKA2L1_BROWSER_LOCK_FD交给正常runner。相同open-file-description不重复争锁。
该包装不修改UI事件、脚本、断言、退出结果或日志；完成后还原Popen/环境。

仅当七例input/launcher全0、suite PASS、全部恢复、无活动guest且input SHA未改，
外层才可PASS。活guest时保留私有备份和journal，不替换运行中文件；root正常
关闭后再按记录恢复。settings.private未恢复时不可复制进公开证据。

保留原driver超时/失败逻辑；touch7不证明Save后菜单恢复、物理多指、独立原生
焦点取消或真机通过。新候选没有继承旧C13的PASS。
