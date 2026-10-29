# AES-CTR counter bounds on ARM32

`reproduce.py` extracts `CounterBlockHelper` and its method bodies directly from
`CryptoAlgorithmAESCTR.h/.cpp`, builds the baseline and current versions using an
existing ARMv6 Linux JSC build's WTF dependencies, and executes both under QEMU
ARM1136. It does not recreate the counter algorithm in a test implementation.
Expected capacities and wrapped nonce bytes come from Python arbitrary-precision
integer arithmetic. A compile-time assertion requires 32-bit `size_t`.

```sh
. "$QEMU_WORK/env.sh"
python3 Tools/Symbian/tests/aes-ctr-counter/reproduce.py \
  --checkout "$CHECKOUT" --qemu-build "$QEMU_WORK/build" \
  --sysroot "$ARM11_TOOLS_ROOT" --qemu "$QEMU_ARM" --work "$WORK"
```

The fixed baseline is `d46c6c5a84b`. The 35 cases cover lengths 1, 7, 32, 33,
64 and 128, zero and maximum counters, values around a remaining capacity of
2^32, and preservation of all nonce bits outside the counter field. The old
implementation must fail exactly four cases: remaining capacity 2^32 incorrectly
becomes zero for lengths 32, 33, 64 and 128. The current version must pass all
35, returning `SIZE_MAX` when that distance cannot be represented.

The output records source/header/method/probe/log hashes and exact build/run
commands. Only this small program is compiled; it does not start an engine build
or emulator. This validates the counter helper, not the mbedTLS cipher routine,
browser API, encryption vectors, key import/export, or asynchronous dispatch.
