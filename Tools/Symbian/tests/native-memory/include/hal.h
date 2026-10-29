#pragma once
#include <e32std.h>
namespace HALData { constexpr TInt EMemoryRAM = 0; }
struct HAL {
    enum { EDisplayXPixels, EDisplayYPixels };
    static TInt Get(TInt, TInt&) { return -5; }
};
