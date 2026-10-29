# SIS registry uncompressed length

Formal installation of the three local 0.3.41 SIS files on host `3c4e27c4…`
produced all 13 correct payload files, normal WM shutdown and host exit 0.
Registry UID/version, destinations, SID, SHA-1 and controller bytes matched,
but **all 13 registry file lengths were wrong**. The installation report keeps
payload PASS separate from registry metadata FAIL; this is not full acceptance.
The original results are in
`/workspace/symbian-belle-work/release-0.3.41/install-run-4/result.json`.

`register_file_description()` assigned the compressed `sis_file_des::len`
to `file_description::uncompressed_length`. Patch 0026 changes that one value
to `sis_file_des::uncompressed_len`, without changing extraction or file bytes.
For example, ICU was registered as 4,768,109 bytes instead of 10,261,824 bytes.
The browser was registered as 21,854,176 instead of 58,640,092 bytes.

The pinned Nokia sources in `official/` independently establish the contract:
`CSISFileDescription::SetLengths()` stores compressed and uncompressed values
separately; `CSisRegistryFileDescription::ConstructL()` takes
`aFileDescription.UncompressedLength()` and serializes its full 64-bit value.
The saved files' Git blob hashes match the two recorded upstream blobs exactly.

The host regression compiles the actual registration function extracted from
the old/new production source, with the actual EKA C++ descriptor/registry types.
The executable SID read is a named stub because this test covers metadata, not
file I/O. It checks lengths, file index, operation/options, MIME, hash, SID,
target and drive mask. Sixteen ordinary cases cover the 13 real package
entries, an empty file, an uncompressed file and a 64-bit logical length.
The old source fails 14 length cases; the fixed source passes all 16 cases.
The complete changed production TU also passes syntax checking with the actual
configured compiler flags. `portable-result.json` records an independent rerun
of the repository test runner, not just the first scratch test.

Reproduce from the repository root (no emulator or engine build):

```sh
python3 Tools/Symbian/tests/eka-sis-metadata/run-host.py \
  --eka-root /workspace/EKA2L1-validation \
  --work /workspace/symbian-installer-metadata-work/recheck
```

Source-only application was authorized and checked against the original SHA:
`d79d15fb16c8275aa53111da8e242535440d4cb02bd3007f14cc8e383177576e`.
The new source SHA is
`e78dac1c04f244a8b145456cc0ddc38f2c9c07e323ad15b7b9a4adf062f9610c`.
Patch SHA is `6d33ae74ffaacc13b8cf6029b5f62c039b489b4a815c0f15a5a8eb936f2f4033`.
The field regression itself did not run a host binary or guest. Subsequent
[formal reinstallation](../package-0.3.41/installation-after-0026/README.md)
on rebuilt host `0d271449...` passed all 13 payload and expanded-length checks,
with three normal host exits and the existing C-drive files preserved.
