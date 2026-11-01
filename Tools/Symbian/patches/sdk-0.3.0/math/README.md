# SDK math member rebuild

The complete SDK runtime/math_c.cc supplies the real C99 atan2l symbol. It
forwards to imported double atan2 only after ARM long-double equivalence
assertions. The declaration is C++-only to preserve official C macro aliases.

Apply symbian-native/0007 first through the checked complete patch stack.
Run scripts/rebuild-sdk-math.py with --sdk, --source and an external --output.
It builds actual Symbian Runtime/Streams/NativeAtomics64 CMake targets, checks
ARM flags, and preserves every other archive member. Staging is the default;
--install explicitly replaces three ARMv6 math members and include/config/math.h.
Run after allocator reconstruction and before consumers. Unknown math inputs
are refused. The original allocator guard must not be relaxed over a math-
modified archive. The helper adds no artifact cache. The workflow caches only its external work
directory, always runs the helper again, and saves the original SDK prefix before
all mutations. A cached staged archive is reconstructed from the current prefix,
not copied into the SDK blindly.

ARM compilation/linking and host type-adapted numerical forwarding tests do
not establish native libm/audio or physical-device compatibility. Existing
C-mode ldexpl/scalbn alias problems are not fixed here.
