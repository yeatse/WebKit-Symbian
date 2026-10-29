# ARM32 AES-CTR counter helper

QEMU ARM1136, `size_t == 4`, 35 cases using extracted production methods.

| Implementation | Outcome |
| --- | --- |
| Baseline `d46c6c5a84b` | Four expected failures: lengths 32/33/64/128 with remaining capacity 2^32 return zero |
| Updated saturation check | 35/35 PASS; those capacities saturate to `UINT32_MAX` |

The original guest output is preserved in `before.log` and `after.log`;
`result.json` associates each with full source/header/extracted-method hashes.
The [reproduction script](../../../tests/aes-ctr-counter/README.md) reads the
methods from the checked-out source and the fixed baseline, builds only a small
probe, and supplies expected values from Python integers.

This evidence covers counter capacity and nonce preservation across partial
counter widths. It does not establish that the browser AES-CTR encryption or
decryption path passes standard vectors; that requires separate runtime tests.
