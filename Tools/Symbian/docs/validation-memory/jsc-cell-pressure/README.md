# JSCell integration pressure

Run [`jsc-cell-pressure.js`](../../../tests/jsc-cell-pressure.js) with the normal
JSC runner, default options, `TIMEOUT=180`, and expected marker
`PASS: jsc-cell-pressure`. The script requires shell heap-statistics/full-GC
APIs and has a 150-second deadline. It retains small linked objects, without an
ArrayBuffer payload or giant root array.

The recorded Symbian EKA run passed in 13.0 seconds with 1,179,648 live linked
objects in each round. Measured object-space bytes after full GC were
75,582,544 and 75,583,792; after releasing the corresponding roots and collecting
they fell to 86,336 and 86,288. Both complete chains and their fields were
verified. Guest and host exited zero without panic. `result.json` identifies the
image, fixture and exact compressed guest-log bytes.

This shell contains the initial growth allocator before the later small-stack
reset fix. Its main-thread success does not supersede that failed native
concurrency run. The final native probe separately passes on unchanged 8 KiB
pthread stacks; a final rebuilt shell should be recorded separately.

Default JIT options do not prove a particular function reached DFG. This is a
JSCell allocation/GC integration test, not a JIT tier assertion or a reproduction
of the original web page. JIT executable reservations and SDK mimalloc's
independent address budget are not counted as these live object bytes.
