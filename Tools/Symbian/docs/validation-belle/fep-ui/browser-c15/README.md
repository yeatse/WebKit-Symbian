# C15 原生 FEP 输入 UI：五例通过

C15 browser 473679d5…、host 277db2b3…、新原生 FEP aad1a4c6…；
真实 XTest 控制同一 PID Qt/原生窗口，未注入 DOM 或修改编辑器状态。网页只使用固定测试文本，
由真实按钮/字段 click handler 设置并检查选区。

实际可编辑字段点击打开 400×300 FEP；程序化 focus 按钮、readonly 字段不打开。
Escape 取消保留七个 UTF-16 code units 的固定 Unicode 文本及 [1,4] 选区；
普通 Submit 写入 native text 并定位末尾；空 Submit 清空字段并定位0。
五个 fresh run 的 native input/page/standard runner 全部通过，正常 host 关闭。
root 已查看打开/结束截图；原 pending metadata 保留，acceptance.json 记录后续实际审阅。

HTML及输入脚本、完整新日志、截图、动作和产物身份均保留，previous 日志被排除。
不宣称密码遮罩、FEP等待时导航/EndTask、真机/其他键盘布局通过。
