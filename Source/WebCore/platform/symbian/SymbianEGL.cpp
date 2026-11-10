/* Copyright (C) 2026 The symbian-webkit authors. All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "SymbianEGL.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifdef __ARM_PCS_VFP
#error "Symbian native EGL imports require base AAPCS (soft or softfp), not hard-float PCS"
#endif
static_assert(sizeof(void*) == 4);

static_assert(sizeof(EGLDisplay) == sizeof(uint32_t));
static_assert(sizeof(EGLContext) == sizeof(uint32_t));
static_assert(sizeof(EGLSurface) == sizeof(uint32_t));
static_assert(sizeof(EGLConfig) == sizeof(uint32_t));
static_assert(sizeof(EGLint) == sizeof(int));
static_assert(sizeof(intptr_t) == sizeof(EGLint));

namespace WebCore::SymbianEGL {
Handle getDisplay(int32_t nativeDisplay) { return eglGetDisplay(nativeDisplay); }
int initialize(Handle display, int* major, int* minor) { return eglInitialize(display, major, minor); }
int terminate(Handle display) { return eglTerminate(display); }
int bindAPI(uint32_t api) { return eglBindAPI(api); }
int chooseConfig(Handle display, const int* attributes, Handle* configs, int size, int* count)
{
    // EGLConfig is the signed type corresponding to Handle; accessing its
    // storage through the corresponding signed type is permitted by C++.
    return eglChooseConfig(display, attributes, reinterpret_cast<EGLConfig*>(configs), size, count);
}
int getConfigAttrib(Handle display, Handle config, int attribute, int* value) { return eglGetConfigAttrib(display, config, attribute, value); }
Handle createContext(Handle display, Handle config, Handle sharing, const int* attributes) { return eglCreateContext(display, config, sharing, attributes); }
Handle createWindowSurface(Handle display, Handle config, void* window, const int* attributes) { return eglCreateWindowSurface(display, config, window, attributes); }
Handle createPbufferSurface(Handle display, Handle config, const int* attributes) { return eglCreatePbufferSurface(display, config, attributes); }
int destroyContext(Handle display, Handle context) { return eglDestroyContext(display, context); }
int destroySurface(Handle display, Handle surface) { return eglDestroySurface(display, surface); }
int makeCurrent(Handle display, Handle draw, Handle read, Handle context) { return eglMakeCurrent(display, draw, read, context); }
int swapBuffers(Handle display, Handle surface) { return eglSwapBuffers(display, surface); }
int querySurface(Handle display, Handle surface, int attribute, int* value) { return eglQuerySurface(display, surface, attribute, value); }
Handle getCurrentContext() { return eglGetCurrentContext(); }
Handle getCurrentDisplay() { return eglGetCurrentDisplay(); }
Handle getCurrentSurface(int which) { return eglGetCurrentSurface(which); }
const char* queryString(Handle display, int name) { return eglQueryString(display, name); }
int getError() { return eglGetError(); }
Handle createImage(Handle display, Handle context, uint32_t target, Handle buffer, const intptr_t* attributes, bool core15)
{
    // Native EGL 1.4 KHR attributes and 32-bit EGL 1.5 attributes have identical
    // widths here, but their handle ABI remains the SDK's integer ABI.
    using Function = EGLImageKHR (EGLAPIENTRY *)(EGLDisplay, EGLContext, EGLenum, EGLClientBuffer, const EGLint*);
    auto function = reinterpret_cast<Function>(eglGetProcAddress(core15 ? "eglCreateImage" : "eglCreateImageKHR"));
    return function ? function(display, context, target, buffer, reinterpret_cast<const EGLint*>(attributes)) : EGL_NO_IMAGE_KHR;
}
int destroyImage(Handle display, Handle image, bool core15)
{
    using Function = EGLBoolean (EGLAPIENTRY *)(EGLDisplay, EGLImageKHR);
    auto function = reinterpret_cast<Function>(eglGetProcAddress(core15 ? "eglDestroyImage" : "eglDestroyImageKHR"));
    return function ? function(display, image) : EGL_FALSE;
}
}
