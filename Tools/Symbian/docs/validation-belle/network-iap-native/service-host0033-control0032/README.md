# Normal IAPService API: host0033 with host0032 control

Two controlled native canary runs completed normally with the same canary and isolation sources. The host0032 adapter differs only in the pinned host path and SHA. This archive records normal RConnection Start, Progress, GetIntSetting Id/Network/Service, a second local read, and owned-handle Close.

| Host | Whole scoped run | Native capability | Start / Id / Network / Service rc | Stable snapshot |
| --- | --- | --- | --- | --- |
| host0032 control | NORMAL_API_SNAPSHOT_COMPLETE | UNAVAILABLE | 0 / 0 / 0 / -1 | false |
| host0033 getter fix | NORMAL_API_SNAPSHOT_COMPLETE | AVAILABLE | 0 / 0 / 0 / 0 | true |

UNAVAILABLE in the control is an ordinary completed capability result. Both runs have host exit 0, guard/audit true, zero denied mutations and unsupported async calls, no fatal/forced cleanup, observed host stop, full drive/configuration restoration, unchanged original ROM/J2ME bytes and unchanged pinned inputs. The public native records contain fixed identities, return codes, counts and booleans; actual IAP/Network/Service values were never formatted. Host identities and the 84,716-byte native EXE identity are explicit in identities.json. No binary bytes are included.

runs/ contains only whitelisted result fields, and records/ contains the strict validated public native records. identities.json retains only known public asset hashes; Root's separate post-verification of fifteen public pins and seventy-two WIP files is identified as a Root attestation, not a fresh original-file check by the archive checker. No configuration, catalog/device values, ROM/J2ME contents or hashes, inventories, journals, logs, sandbox specs, traces or user data are archived.

getter/ preserves the narrow source patch and its exact before/after public source identities. The new getter reads the active connection's IAPService integer from its existing record; it does not guess an IAP/service, create records or change proxy policy. The 37-assertion invented host adapter and compile/relink metadata are historical preparation evidence. The two native records provide the distinct actual runtime evidence. This is not native CommDB, proxy routing, name-clone, DNS, phone or installer validation. Close does not forcibly stop a shared interface, and double reads are not an atomic transaction.

prior-failures.json preserves Root01–06 and Root08–13 as FAIL, including pre-guest space failures. Root08's separately valid native record never overrides its outer FAIL. Root13's public-literal source references are diagnostic candidates, not a call stack or proof of an exact executing source line. The v12/v13 post-launch source-index limitation is recorded in peer-review.md; that diagnostic index is absent from the accepted v14 core.

harness/ contains exact source bytes and the two required preservation helpers; copy-provenance.json binds their original public paths and hashes. It is reproduction source, not a ready payload or an invitation to execute a preserved runtime. A future Root-controlled reproduction must separately materialize the pinned host/EXE and helpers, use fresh public/private outputs and the preserved containment/restore contract. The adapter's default is prepare-only; native execution remains explicit. Do not run the preservation helper as a substitute adapter.

Safe archive-only verification:

```sh
python3 -B verify_public_archive.py
sha256sum -c SHA256SUMS
```

The checker reads only this archive, imports only the pinned public validator, and launches no subprocess, guest, socket or network. A checker PASS verifies archived records, scope and hashes; it is not a newly executed native PASS. SHA256SUMS covers every other file, and its SHA-256 is the archive root digest. No runtime was repeated to prepare this archive.
