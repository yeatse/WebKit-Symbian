# Clipboard adapter host boundaries

All 24 production-fragment checks passed. `result.json` binds the source,
extracted code, harness and log hashes. The extraction and invocation are in
[`clipboard-native-adapter`](../../../../../tests/clipboard-native-adapter/README.md).

These checks exercise Unicode/line normalization and admission before native
allocation, including invalid lengths/pointers, exact size limits and invalid
HAL drives. They do not establish native CClipboard/SCSU behavior, real heap
cleanup or browser paste/cut semantics. Those require the separate native and
browser fixtures.
