# Browser native class allocation

The SDK's `CBase::operator new` and `operator new(..., TLeave)` allocate with `User::AllocZ` and `User::AllocZL`. Its `CBase` declaration has no class-specific `operator delete`. With the portable C++ runtime linked into the browser, compiler-generated deleting destructors for browser-owned subclasses consequently referenced global sized delete (`_ZdlPvj`), whose allocator is not the Symbian user heap.

The browser's private `NativeHeapDelete.h` supplies class-local unsized delete and the matching `TLeave` construction-failure delete, both calling `User::Free`. It is applied to `CUrlBar`, `CUrlSuggestions`, `CBrowserAppUi`, `CBrowserDocument`, `CBrowserApplication`, `CWebContainer`, `CRunLoopDriver`, its `CWorkTimer`, `CPageList`, and `CNetworkConnection`. No base classes, object layouts, global operators, WebCore allocator, or imported DLL destructors are changed.

The recorded check compiled five actual ARM objects using the configured browser commands, then inspected ARM relocation sections. All ten deleting destructors call `User::Free` and contain no global delete reference; their allocation sites still call `User::AllocZ` or `User::AllocZL`. Constructor-failure cleanup in the local native factories also resolves to `User::Free`. `summary.json` records class-by-class symbols and source/header/object hashes. This is compiler/object evidence, not a guest lifetime or allocation-failure execution test.

Reproduce after setting the SDK environment and configuring the browser with compilation commands:

```sh
python3 Tools/Symbian/tests/check-browser-native-heap.py \
    --compile-commands /path/to/build/browser/compile_commands.json \
    --output /path/to/new-native-heap-check
```

The checker compiles only independent objects in the new output directory and uses host `readelf`; it does not invoke Ninja, link the browser, or run EKA. It retains compiler output and complete relocations. Imported native objects continue to be deleted through their DLL virtual destructors. Direct construction of the external non-leaving `CAsyncCallBack` in the input self-test still emits a global-delete exceptional cleanup edge; that imported non-L constructor is outside this local-subclass fix. Normal callback destruction remains a DLL virtual call.
