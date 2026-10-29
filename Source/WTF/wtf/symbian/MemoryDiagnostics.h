// Private to FastMalloc.cpp and SymbianSystem.cpp. Keep allocation diagnostics
// out of the platform interface included throughout WTF and JavaScriptCore.
#pragma once

#include <stddef.h>

namespace WTF::SymbianSystem {

// Fixed-stack-buffer failure logging. Pool sizes are bytes; zero means this
// is a direct chunk operation. A negative system-free value means unavailable.
void reportMemoryFailure(const char* reason, size_t bytes, size_t alignment = 0,
    size_t poolReserved = 0, size_t poolUsed = 0, size_t poolLargestFree = 0, int nativeError = 0);

}
