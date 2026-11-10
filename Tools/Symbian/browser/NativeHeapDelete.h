// Browser-owned CBase subclasses use the Symbian user heap, not the C++ runtime heap.
#pragma once

#include <e32base.h>

// CBase's inherited new/new(ELeave) use User::AllocZ/AllocZL, but its SDK
// declaration has no class delete. A locally emitted deleting destructor would
// otherwise select the portable runtime's global (possibly sized) delete.
// Keep this in each native class's public section: it does not change the class
// layout, imported DLL destructors, or WebCore's C++ allocator. The TLeave form
// also matches construction failure after new(ELeave).
#define SYMBIAN_BROWSER_NATIVE_DELETE \
    static void operator delete(TAny* aPointer) noexcept { User::Free(aPointer); } \
    static void operator delete(TAny* aPointer, TLeave) noexcept { User::Free(aPointer); }
