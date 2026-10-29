# Native aligned allocator pressure

These runs compile the actual production `AlignedPool` class and real Symbian
chunk bridge. The fixture and reproduction instructions are in
[`tests/aligned-pressure`](../../../tests/aligned-pressure/README.md). No JS VM
or executable code pool is present. The default 8 KiB pthread stack is unchanged.

| Image variant | Result | What it establishes |
| --- | --- | --- |
| `single-pool-negative` | FAIL, 0/2; guest 1, host 0 | The released `17cf20` single pool stops at 4,095 live 16 KiB blocks (67,092,480 bytes). The aligned base consumes the remaining edge of its 64 MiB reservation. Only the pool class comes from that release. |
| `growth-stack-failure` | FAIL, guest access violation; host killed | The first five growth/zeroing/content/fragmentation checks passed, then an 8 KiB pthread overflowed. The aggregate-reset expression produced a 34 KiB temporary in every allocation's frame. This is not a successful run. |
| `growth-final` | PASS, 7/7; guest 0, host 0, no panic | 80 MiB of simultaneously committed and fully written aligned blocks, retained patterns, hole reuse, zero-on-recommit, fragmentation-driven growth and four concurrent pthreads all pass. |

The stack fix resets only the two fields changed by reservation before the
first commitment succeeds. `stack-frames.json` records disassembly and compiler
stack-usage evidence. The production allocation function's static frame falls
from 34,880 to 40 bytes; its callees have additional frames. Fault-injection
tests separately cover rollback after failed initial commitment.

`manifest.json` identifies all images and the host, records exact guest-log
bytes/hashes, and preserves the strict runner's original results. Compressed
logs decompress to the original bytes. Full host logs remain external; their
hashes and sizes are recorded. No firmware or executable is stored here.

EKA reports the same 256 MiB free-RAM value throughout these runs. That reading
is not used as an allocation assertion or proof of physical-phone RAM usage.
All-byte writes and retained checks establish the tested memory was usable in
the guest. These tests demonstrate and remove the allocator's fixed-pool limit;
the older phone log alone does not uniquely identify its failure cause.
