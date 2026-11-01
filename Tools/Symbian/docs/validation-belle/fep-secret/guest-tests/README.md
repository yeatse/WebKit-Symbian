# Guest-only A6 secret-mode request candidate

This candidate is based on the current EKA 0028 FEP source. It adds
EHUIOpenGlobalTextViewWithSecret(func_id, initial_text, max_length, TBool secret,
TRequestStatus* status) at HLE opcode A6. The existing A0 declaration, symbol,
argument order and implementation stub remain unchanged for older clients.
No host bridge or Qt code is changed by this guest-only patch.

CHostBridgedImeFEP::OpenDialogInputL captures the focused request's
TCoeInputCapabilities::SupportsSecretText() and normalizes its nonzero value.
The official getter can return 0x100, so it is not compared with ETrue(1).
CHostDialogIme::Open sends that value as argument four and request status as
argument five. Initial text, maximum length, KRequestPending/SetActive and
0028's accepted/cancelled completion semantics are retained.

The actual FEP C++ translation unit compiles to an ARMv6 object with the real
native-leaves SDK flags and exact official fepitfr.h overlay; the actual updated
Dispatch assembly also compiles, containing both A0 and A6 symbols. Only local
include-path/function metadata were added to the generated assembly for its
standalone object, as in the already established native FEP build.

The host fixture extracts the real Open, RunL, OpenDialogInputL,
CommitChangedTextL, CancelChangedTextL and CancelDialogL methods. It extends the
0028 ten-case matrix across raw secret values 0/256 and maximum lengths
64/1048576, for 40 cases. Submit/empty Submit, Cancel, selection preservation,
Unicode/multiline fixture text, duplicate completion, guest cancellation and
open failure pass under ASan/UBSan. It records the actual fifth status argument
and confirms the request was pending before dispatch. A negative comparison
control using == ETrue fails at secret 256 as intended.

This proves source/compile and bounded method behavior only: no full DLL link,
guest/display run, password masking UI or physical-device acceptance occurred.
The parent agent owns the new host A6 handler, Qt secret editor and combined
0029 patch. Original EKA GPL-3.0 source/license comments remain intact;
secret-api-evidence.json records the official EPL header/API sources and hashes.
No credentials, private config, clipboard or user data were read.
