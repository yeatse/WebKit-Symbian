# CI44 E32 converter 构建失败

2026-11-01；[run41277714685](https://github.com/yeatse/WebKit-Symbian/actions/runs/41277714685) / job124763161105，精确源提交207c9995d511a510bef15a966f27100de1cf7455，completed/failure。

`E32 converter`步骤执行`build-symbian-native.sh`时，git apply拒绝e32.cc、exports.cc、imports.cc、tools/main.cc及两个测试文件的既有hunks，退出1。实际失败片段、完整step状态及该提交的构建脚本已保留。缓存已命中，脚本采用逐补丁reverse-check后再apply；缓存中后续补丁改变早期上下文是一种待验证解释，未凭日志宣称已定位或修复。

JSC/WebCore/browser/SIS步骤均未执行，没有sis artifact，未下载任何二进制，也没有0.3.44核包PASS。六个source payload输入和认证255上限源码合同独立核对通过；它们不代替实际RSC242B/fb8…或实际安装包验证。

未改共享源、SDK/EKA、workflow或cache；未rerun、安装或运行guest/display。原完整GitHub job日志仅保留外部路径/SHA/字节见verification.json，归档只含公开失败片段。
