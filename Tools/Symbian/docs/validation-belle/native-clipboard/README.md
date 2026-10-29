# Native clipboard validation

Four independent emulator processes completed native Unicode and empty-string
write/read pairs; a preceding read-only inspection also passed. The fourteen
checks use actual CClipboard/CPlainText APIs, exact UTF-16 equality, persisted
store bytes, and normal guest/host exits. Chinese, an emoji surrogate pair, and
two newlines survived the process boundary.

The profile originally had no clipboard file. The test restored that absence
afterwards and removed only the new, empty parent directory. No prior clipboard
contents, profile databases, firmware, or executable binaries are archived here.
Each case contains the unmodified guest log and runner result; manifest.json
records source/image/host identity and hashes of the external host logs.

This establishes the native clipboard foundation only. The current WebCore
clipboard strategy still uses its process-local item map; system integration
and real-device behavior are not claimed.
