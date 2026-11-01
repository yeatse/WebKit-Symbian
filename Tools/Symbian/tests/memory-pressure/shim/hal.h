#pragma once
namespace NativeTest { inline int halError = 0, freeRAM = 100; }
namespace HALData { enum { EMemoryRAMFree }; }
struct HAL { static int Get(int, int& value) { value = NativeTest::freeRAM; return NativeTest::halError; } };
