# host0034: read-only original backing and normal connection query

Root's actual host0034 run completed as NORMAL_API_SNAPSHOT_COMPLETE with an explicit RO original backing-file bind and no working ROM copy or writable ROM bind. The strict native record is AVAILABLE: Start/Id/Network/Service rc are all 0, the double read is stable, owned Close returned, and no timeout occurred. Host exit is 0; guard/audit pass, denied mutations and unsupported async are 0, no fatal/forced cleanup occurred, the host stopped, drives/configuration were restored, and original ROM/J2ME bytes and input pins remain unchanged. Only those fixed booleans are preserved, without original paths, contents or hashes.

Host0034 is 65,488,744 bytes, SHA-256 `b87e3961f7321bd0e8ce5cb3adc1ce608f624ecfbf3b75af434801addacfa773`. identities.json pins the exact Root harness sources and shared native canary. The harness itself is referenced by public source identity; its private runtime state is not copied. native-public-record.txt contains only the validated fixed statuses, counts and booleans. validate.py is the pinned public parser.

The narrow POSIX patch opens exactly existing-file/size-zero/private/RW backing with O_RDONLY and no O_CREAT. Writable private pages do not require file-write permission. Other open modes and Windows code are unchanged; this patch does not establish shared-map support or fix all mapping APIs.

cow/ includes the small public canary source, production patch and fixed file-API records. The real complete source TUs mapped only a task-owned public 0444 fixture: the original source retains its ordinary FAIL, while the patch permits a fixed private COW write and normal release with original task bytes/mode unchanged. Other-mode cases are synthetic open-flag checks. No file content or address is printed. That phase did not access a ROM/database or execute a guest. Compilation/link metadata are historical source evidence; the separate Root native record supplies the actual later controlled host execution.

The external canonical COW evidence has 18 verified entries and SHA256SUMS root `a68666701fe93157683c16f09ede9a9709ae241b691d0e8bbc3d3d71b28e19f6`. Only selected source and fixed statuses/hashes are copied; large link inputs, logs, objects and binaries remain outside this archive.

The preceding [host0033/control0032 archive](../service-host0033-control0032/README.md) remains unchanged: 26 files, root `8b6acf2bb423ba9c4ecdc433ee1b37b3f6dfeac20326b97fd43555ebc2edea72`. Its before/after API comparison and prior FAIL references retain their original meaning.

Scope is ordinary public COW backing-file behavior plus the fixed normal connection query. Name-clone, DNS, Native CommDB, proxy routing, phone and installer behavior remain unverified. Close does not force-stop shared interfaces and double reads are not atomic transactions. No runtime or file canary was repeated for this addendum.

Safe archive-only checks:

```sh
python3 -B verify_public_archive.py
sha256sum -c SHA256SUMS
```

The checker reads only this archive and imports only its pinned public parser. It compiles/launches nothing and uses no IPC/network. Its PASS checks archived records and hashes, not a new runtime result. SHA256SUMS covers every other file; its SHA-256 is the addendum root digest.
