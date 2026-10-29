# ARM1136 QEMU validation

`tiers-result.json` records one ARMv6 ARM-state/VFPv2/softfp Linux JSC image running all 12 cases in LLInt, Baseline and DFG profiles. Each profile exited zero, emitted all expected markers and produced the same semantic digest. The DFG result includes actual optimized-body observations, compilation counts, OSR entry/exit and a RAB guard exit after the new view structure entered the profile. `elf-attributes.txt` verifies the image architecture and ABI.

`features-result.json` records one successful run each of `es2020.js`, `intl.js` and `resizable-arraybuffer.js` using the image's default settings. These are compatibility regressions; the separate tier matrix supplies execution-tier evidence.

`concurrent-result.json` records a separate successful run with default concurrent JIT and default tier thresholds. Both kernels compiled and actually executed DFG; optimized allocations survived GC and 32 retained roots survived an additional full collection. The outer process timeout was 60 seconds, in addition to the script's 50-second monotonic budget. The process exited zero.

The JSON command templates use `${WORK}` for the isolated build/tool directory and `${WEBKIT_SRC}` for the checkout. All `.log` files are unmodified guest output. Binary images, SDK files and toolchain packages are intentionally not stored here. QEMU validates the ARM11 CPU backend; Symbian platform behavior requires the separate EKA evidence.
