# C17 普通Options菜单：root实际UI smoke通过

root实际在about:blank完成四步：指针[540,641]打开Options；Cancel[450,497]关闭并恢复软键；F2重新打开；Reload行[450,298]选择后正常返回页面。原始四段XTest记录和前后截图保留，launcher0，config/profile/resources恢复成功。

原始runner只定位为CALIBRATION_ONLY，result.json没有改写；本目录acceptance-summary.json的PASS只指上述root观察的菜单打开/关闭/选择smoke，不是新的自动页面用例或clipboard九例。about:blank的Copy/Select all来自地址栏焦点，不能用作网页编辑tail或End/Up导航校准。

C17与host/FEP/resource身份见原始result.json。未复制config.private.yml、private备份或previous原文件。正常Qt close不替代官方EndTask生命周期验收。
