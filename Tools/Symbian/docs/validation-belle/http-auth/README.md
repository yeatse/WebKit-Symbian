# HTTP authentication bridge validation

This artifact records host ASan/UBSan contracts, five ARM syntax checks, native
ARM object generation and browser resource compilation. `native-symbols.json`
maps all 81 native API imports in AuthenticationUI.o to the browser-linked SDK
proxies, including CEikDialog::TryExitL in eikdlg.dll.

The AVKON resource layout and empty-entry behavior were checked against official
SymbianSource classicui commit dcea899751dfa099dcca7a5508cf32eab64afa7a. The
manifest records exact upstream file hashes. The SECRETED resource comes from
the same EIKON.RH already recorded by browser/resources/source-hashes.json.

No EKA2L1 or physical-device run was performed for this patch. Native dialog/FEP,
401 retry/redirect integration and active-dialog navigation/exit remain device
acceptance checks; see ../../../tests/http-auth/README.md for the checklist.
TLS verification is unchanged; interactive 407 proxy authentication remains
unimplemented. Authentication results use CredentialPersistence::None; the
Curl handler now respects it before writing credentialStorage.
