# RSA WebCore integration checks

Eight production translation units pass the actual Symbian SDK ARM compiler
after the RSA key type, registry, CMake sources and PSS feature gate are wired.
These checks use the real source headers: no key-container overlay and no
command-line `HAVE_RSA_PSS` override. The public JWK path also includes the
missing `dq` check and rejects unsupported multiprime `oth` input.

`result.json` records each translation unit, source hashes and log hashes.
`base-command.json` preserves the compiler arguments with portable path
placeholders. Substitute `$CHECKOUT`, `$WEBKIT_BUILD`, `$SDK` and `$DEPS_WORK`,
set `SYMBIAN_SDK_PREFIX` to the SDK directory, and run the command once per
listed source, replacing `$SOURCE` with its path under `Source/WebCore`.
The build directory must contain the normal generated WebCore/JSC/WTF headers.
Source include paths precede copied headers so the check uses the integrated
Platform.h and CryptoKeyRSA.h. WebCore's source directory precedes WTF's to
select the correct `config.h`.

These are compilation checks, not a successful browser link or API execution.
The independent native probe has passed [171 checks](../native-attributes/README.md).
The subsequent [33-case browser run](../browser-c11/README.md) passed on the
fully linked candidate 11; it is separate from these compilation checks.
