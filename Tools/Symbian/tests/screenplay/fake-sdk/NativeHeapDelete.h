#pragma once
#include "fake_symbian.h"
#define SYMBIAN_BROWSER_NATIVE_DELETE static void operator delete(void* p) noexcept { User::Free(p); } static void operator delete(void* p, TLeave) noexcept { User::Free(p); }
