# EKA keyboard host checks before integrated browser acceptance

Patch [0027](../../../patches/eka2l1/0027-Translate-Ctrl-Alt-and-refresh-repeated-key-codes.patch)
adds Ctrl/Alt state and ASCII control-letter translation to the existing WSERV
keyboard translator. Modifier keys remain raw down/up transitions, without a
character event or repeat timer. Held letters are translated using the current
modifier state on each repeat; the timer identity used by key-up cancellation
is unchanged. The three changed files replay from their post-0026 bytes to the
exact original candidate hashes; [report.json](report.json) records that identity.

The ordinary default phone profile does not bind Ctrl, Alt or A-Z. Consequently,
a real host Ctrl+V input cannot reach the guest merely because XTest emitted it.
The separate [webkit-editor.yml](webkit-editor.yml) is an **opt-in ASCII profile**:
it preserves every original default binding and appends 28 Ctrl/Alt/A-Z sources.
It does not modify the default profile or add an unconfigured-key fallback to
Qt/WSERV. Qt's existing display callback releases held keys on focus loss and
window deactivation; the candidate does not change menus, shortcuts or FEP.

## Results

- [host-tests.log](host-tests.log): **232 assertions in seven test cases PASS**.
  The project's actual Catch2 tests include the original three Shift cases and
  four new groups for both Ctrl/Alt sides, aggregate state, A-Z control codes,
  special Backspace/Tab/Enter scans, and changing modifiers on a held letter.
  The executable uses the actual candidate header and actual `common.cpp`
  scan-code mapping, rather than a copied translation algorithm.
- `common.cpp`, `io.cpp` and the modified `window.cpp` each compile successfully
  with the existing EKA build's actual flags and the candidate overlay header.
  [commands.json](commands.json) preserves the three commands. The original
  `*-compile.log` files are empty because those successful compilations emitted
  no output; object byte counts/hashes and exit codes are in report.json. No
  object, full executable or reconstructed compiler output is included here.
- [profile-check.log](profile-check.log): PASS using the project's real
  yaml-cpp archive, actual `map_key_to_inputcode`, and candidate translator.
  It parses both YAML profiles, checks preservation of all old mappings, rejects
  duplicate sources, checks exactly 28 additions and drives Ctrl/V/Alt transitions
  through the configured mapping. Source: [profile-check.cpp](profile-check.cpp).
- The mail-format patch was replayed with `git am` in an isolated three-file
  repository initialized from the pre-0027 bytes. All resulting file hashes
  match the original unified candidate. No shared checkout was changed by this
  evidence-generation step.

These checks were run while preparing the external candidate. They **do not
establish an integrated EKA build, guest clipboard operation, browser shortcut
behavior or physical-phone acceptance**. Root integration/build/runtime work,
if performed later, must keep its own image identities and results. The earlier
C11 clipboard browser run remains failed and is not superseded by host tests.

## Profile scope and use

Qt key constants and SDK scan/modifier identities are recorded in
[profile-evidence.json](profile-evidence.json). The profile maps Qt Control/Alt
to guest left-Control/left-Alt because the existing host callback reports Qt's
key value without a left/right side. The translator itself handles both guest
sides. ASCII letter scans remain A-Z: unshifted letters produce a-z, Shift gives
A-Z, and Ctrl without Alt produces codes 1-26. Ctrl+H/I/M retain the letter scan
so they remain distinguishable from the physical Backspace/Tab/Enter scans.

Install this YAML as a separate `bindings/webkit-editor.yml` and opt in through
EKA's existing `--keybindprofile webkit-editor` option. A runtime wrapper must
preserve and restore any original configuration; it must not replace the default
profile. File hashes in this directory bind the exact prepared profile.

This profile is not a CapsLock, AltGr, Unicode, input-method or complete host
keyboard-layout implementation. It does not translate shifted punctuation or
number rows. Normal FEP text retains its separate existing platform path.

The original working directory is `/workspace/symbian-eka-keyboard-work`;
absolute paths inside raw command/evidence records refer to that preparation
workspace. `SHA256SUMS` covers the portable evidence files without changing raw
log bytes. No guest profile database, clipboard content or credentials is included.
