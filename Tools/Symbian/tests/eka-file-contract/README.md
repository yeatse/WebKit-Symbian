# EKA file-sharing and read-only contracts

The fixture exercises production sharing state against the public Symbian
`f32file.h` table, including multiple readers and promotion/demotion. It also
mounts a real isolated host directory through EKA's actual physical VFS:
read-only attributes must survive VFS reconstruction, denied writes must
preserve the sentinel, and clearing the bit must permit writing again.
An open file can be renamed without reopening or truncating it: its cursor,
contents and read-only attribute must survive; a failed rename must preserve
the original path and cursor. The fixture currently performs 115 checks.

Use the normal configured EKA host compiler environment:

```sh
python3 Tools/Symbian/tests/eka-file-contract/run.py \
    --eka-source /path/to/EKA2L1 --eka-build /path/to/EKA2L1/build \
    --output /new/private-test-directory
```

During review, `--overlay /private/candidate` selects changed source files and
headers there, with configured source paths for unchanged includes. The script
reads `ninja -t commands`; it does not build the shared graph or launch EKA.
Objects, dependency files and fixture files stay in the private output directory.

All three complete modified TUs are compiled. Only the physical VFS and actual
sharing/attribute helpers execute. This does not prove IPC decoding,
same-process `RFile` calls, duplicate-handle teardown or clipboard behavior;
these require the separate native guest probe. It does not claim support for
hidden/system/archive changes or modification-time updates. Unsupported changes
must return errors instead of false success.

The physical rename test covers ordinary host-backed paths. Android content
URI rename is not implemented by this change; it fails without a copy/delete
fallback. The fixture does not establish Windows open-file rename behavior.
