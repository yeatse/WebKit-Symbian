# CI 41 实际安装包核验

[Run 40964125695](https://github.com/yeatse/WebKit-Symbian/actions/runs/40964125695)
在 `2.52.6-symbian` 的提交 `0b2858e230f038a51821ed197127480fa43fbea1` 上成功。
2026-11-01（Asia/Shanghai）通过 GitHub connector 下载了
[sis artifact 12939729959](https://github.com/yeatse/WebKit-Symbian/actions/runs/40964125695/artifacts/12939729959)。
ZIP 为 33,626,643 字节，SHA256 与 GitHub digest 精确匹配；没有下载 ELF artifact。

三个 unsigned SIS 均为 **0.3.41**，其 SHA256 均匹配 artifact 内的 SHA256SUMS。
包 UID/header CRC、controller/data section CRC、13 个安装目标、每个 payload
的解压长度和 SHA-1、E32 UID3 与 VFPv2 标记全部通过。
四个 JS、test.html、fonts.conf 与 CI 源提交的文件逐字节一致，CA 与固定 release bundle 一致。
ICU 数据 SHA256 为 `808fc9bd…`；此次从实际 SIS 提取的数据重新通过 8 组 host IDNA
检查，其中包含隔离 stubdata 的实际数据检查及缺失 UTS46 负对照。

| SIS | bytes | SHA256 |
| --- | ---: | --- |
| browser.sis | 21,962,484 | `4374f17ca542e3c5163acfe4ece7a310061f86a5aa63dd1018c339ad82e5bf2b` |
| jsc.sis | 6,894,492 | `70e9407d1eeb9ba26536bb8f2f7b6ff3040ba49e0a2dab1a390a5a9770dbcc6e` |
| symbian-webkit-icu.sis | 4,768,936 | `5a2ad94da2911a9d586466dac58f40dd61af14c43c73a6613f44ed77a3e36692` |

CI browser.exe 为 58,579,008 字节，SHA256
`e1de3f72baa498ca9b61c30caf53626343196aa36290687653c4a8520ed19ac5`；
CI jsc.exe 为 18,830,172 字节，SHA256
`d4d07049690307b598df4f72a80e091d30032f566fd111fd42d9b9285a290e00`。
它们与 local C10 / C11 产物不同。本目录只证明下载、包内容和 host 数据检查；
**没有安装或运行此 CI41 包，没有真机验收**。local C10 的三包安装结果不能归给这里。

原始 ZIP、SIS、解压 payload 位于 `/workspace/symbian-ci41-validation/`，
不放进 Git。所有报告、核验器及其 SHA256 保存在此目录；ELF 仅核对 metadata。
重新核验时，将 ZIP 四个成员置于 `ARTIFACT_DIR/sis/`，执行：

```sh
python3 Tools/Symbian/docs/validation-release-0.3.41/ci/verify-payloads.py ARTIFACT_DIR
```

外部运行核验器时可用 `--tools-root /path/to/WebKit/Tools/Symbian`。
完整 IDNA 检查使用仓库 `Tools/Symbian/tests/icu-idna/run.py` 和实际解压数据。
