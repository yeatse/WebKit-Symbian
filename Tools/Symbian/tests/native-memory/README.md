# Native memory bridge fault injection

Run `./run.sh` with a host C++20 compiler (`CXX` may select one). The test compiles
`Source/WTF/wtf/symbian/SymbianSystem.cpp` itself against small fake EKA2 and
hardware entropy interfaces. It covers alignment and guard bounds, preserving
already committed pages, rollback when a later page commit fails, decommit
idempotence and zeroing, the reservation limit and slot reuse, and full-output
clearing after failed or short entropy reads.

The fixture uses host `mmap` for its fake chunks. It validates the bridge's
bookkeeping and error handling, not EKA2's kernel semantics, thread APIs, true
entropy quality, or JSC's physical-memory accounting. Guest RAB/GSAB and native
runtime tests remain necessary.
