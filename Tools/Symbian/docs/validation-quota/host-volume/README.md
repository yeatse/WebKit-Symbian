# Mapped-volume host verification

The actual `drives.cpp` passed syntax checking with the emulator build command.
The standalone helper passed 21 checks plus one real permission-denial case.
`patch-chain.json` records the 15 ordered patches applied to a temporary Git index
and verifies that the two resulting sources match the tested working files.
The emulator repository was not committed and no emulator binary was built or
executed for these checks.

The independent review corrected the inherited default-drive behavior to use
the RFs session path. It also requires the complete 552-byte EKA1 or 556-byte
EKA2 volume prefix while retaining compatibility with an omitted EKA2 tail.
The standalone checks do not execute the IPC context or assert native guest
results; that remains the separate native volume probe's job.

Run the reproducer documented in
[`host-volume.md`](../../../tests/native-volume/host-volume.md) against the patched
emulator source. ROM capacity describes the mapped host filesystem, not the
physical size of a phone ROM; ROM writable space is zero.
