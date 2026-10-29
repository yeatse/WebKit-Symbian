/* Copyright (C) 2026 The symbian-webkit authors. All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include <stdint.h>

// This boundary deliberately exposes neither SDK nor ANGLE EGL typedefs.
// Symbian's native ABI uses 32-bit integer handles, while WebCore/ANGLE use
// Khronos pointer handles. Only SymbianEGL.cpp includes the native EGL headers.
namespace WebCore::SymbianEGL {
using Handle = uint32_t;
Handle getDisplay(int32_t nativeDisplay);
int initialize(Handle, int*, int*);
int terminate(Handle);
int bindAPI(uint32_t);
int chooseConfig(Handle, const int*, Handle*, int, int*);
int getConfigAttrib(Handle, Handle, int, int*);
Handle createContext(Handle, Handle, Handle, const int*);
Handle createWindowSurface(Handle, Handle, void*, const int*);
Handle createPbufferSurface(Handle, Handle, const int*);
int destroyContext(Handle, Handle);
int destroySurface(Handle, Handle);
int makeCurrent(Handle, Handle, Handle, Handle);
int swapBuffers(Handle, Handle);
int querySurface(Handle, Handle, int, int*);
Handle getCurrentContext();
Handle getCurrentDisplay();
Handle getCurrentSurface(int);
const char* queryString(Handle, int);
int getError();
Handle createImage(Handle, Handle, uint32_t, Handle, const intptr_t*, bool core15);
int destroyImage(Handle, Handle, bool core15);
}
