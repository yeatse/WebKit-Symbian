/* Copyright (C) 2026 The symbian-webkit authors. All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include "SymbianEGL.h"
#include <EGL/egl.h>
#include <wtf/Vector.h>

namespace WebCore::SymbianEGLBindings {
static_assert(sizeof(void*) == sizeof(SymbianEGL::Handle));
inline SymbianEGL::Handle nativeHandle(void* handle) { return static_cast<SymbianEGL::Handle>(reinterpret_cast<uintptr_t>(handle)); }
template<typename T> inline T publicHandle(SymbianEGL::Handle handle) { return reinterpret_cast<T>(static_cast<uintptr_t>(handle)); }
inline EGLDisplay eglGetDisplay(EGLNativeDisplayType native) { return publicHandle<EGLDisplay>(SymbianEGL::getDisplay(native)); }
inline EGLBoolean eglInitialize(EGLDisplay d, EGLint* major, EGLint* minor) { return SymbianEGL::initialize(nativeHandle(d), major, minor); }
inline EGLBoolean eglTerminate(EGLDisplay d) { return SymbianEGL::terminate(nativeHandle(d)); }
inline EGLBoolean eglBindAPI(EGLenum api) { return SymbianEGL::bindAPI(api); }
inline EGLBoolean eglChooseConfig(EGLDisplay d, const EGLint* attributes, EGLConfig* configs, EGLint size, EGLint* count)
{
    if (!configs || size <= 0)
        return SymbianEGL::chooseConfig(nativeHandle(d), attributes, nullptr, size, count);
    Vector<SymbianEGL::Handle> nativeConfigs(size);
    auto result = SymbianEGL::chooseConfig(nativeHandle(d), attributes, nativeConfigs.mutableSpan().data(), size, count);
    if (result && count) {
        for (int i = 0; i < size && i < *count; ++i)
            configs[i] = publicHandle<EGLConfig>(nativeConfigs[i]);
    }
    return result;
}
inline EGLBoolean eglGetConfigAttrib(EGLDisplay d, EGLConfig c, EGLint attribute, EGLint* value) { return SymbianEGL::getConfigAttrib(nativeHandle(d), nativeHandle(c), attribute, value); }
inline EGLContext eglCreateContext(EGLDisplay d, EGLConfig c, EGLContext sharing, const EGLint* attributes) { return publicHandle<EGLContext>(SymbianEGL::createContext(nativeHandle(d), nativeHandle(c), nativeHandle(sharing), attributes)); }
inline EGLSurface eglCreateWindowSurface(EGLDisplay d, EGLConfig c, EGLNativeWindowType window, const EGLint* attributes) { return publicHandle<EGLSurface>(SymbianEGL::createWindowSurface(nativeHandle(d), nativeHandle(c), window, attributes)); }
inline EGLSurface eglCreatePbufferSurface(EGLDisplay d, EGLConfig c, const EGLint* attributes) { return publicHandle<EGLSurface>(SymbianEGL::createPbufferSurface(nativeHandle(d), nativeHandle(c), attributes)); }
inline EGLBoolean eglDestroyContext(EGLDisplay d, EGLContext c) { return SymbianEGL::destroyContext(nativeHandle(d), nativeHandle(c)); }
inline EGLBoolean eglDestroySurface(EGLDisplay d, EGLSurface s) { return SymbianEGL::destroySurface(nativeHandle(d), nativeHandle(s)); }
inline EGLBoolean eglMakeCurrent(EGLDisplay d, EGLSurface draw, EGLSurface read, EGLContext c) { return SymbianEGL::makeCurrent(nativeHandle(d), nativeHandle(draw), nativeHandle(read), nativeHandle(c)); }
inline EGLBoolean eglSwapBuffers(EGLDisplay d, EGLSurface s) { return SymbianEGL::swapBuffers(nativeHandle(d), nativeHandle(s)); }
inline EGLBoolean eglQuerySurface(EGLDisplay d, EGLSurface s, EGLint attribute, EGLint* value) { return SymbianEGL::querySurface(nativeHandle(d), nativeHandle(s), attribute, value); }
inline EGLContext eglGetCurrentContext() { return publicHandle<EGLContext>(SymbianEGL::getCurrentContext()); }
inline EGLDisplay eglGetCurrentDisplay() { return publicHandle<EGLDisplay>(SymbianEGL::getCurrentDisplay()); }
inline EGLSurface eglGetCurrentSurface(EGLint which) { return publicHandle<EGLSurface>(SymbianEGL::getCurrentSurface(which)); }
inline const char* eglQueryString(EGLDisplay d, EGLint name) { return SymbianEGL::queryString(nativeHandle(d), name); }
inline EGLint eglGetError() { return SymbianEGL::getError(); }
}

// Namespace declarations avoid preprocessor rewrites leaking into ANGLE calls.
namespace WebCore {
using SymbianEGLBindings::eglGetDisplay;
using SymbianEGLBindings::eglInitialize;
using SymbianEGLBindings::eglTerminate;
using SymbianEGLBindings::eglBindAPI;
using SymbianEGLBindings::eglChooseConfig;
using SymbianEGLBindings::eglGetConfigAttrib;
using SymbianEGLBindings::eglCreateContext;
using SymbianEGLBindings::eglCreateWindowSurface;
using SymbianEGLBindings::eglCreatePbufferSurface;
using SymbianEGLBindings::eglDestroyContext;
using SymbianEGLBindings::eglDestroySurface;
using SymbianEGLBindings::eglMakeCurrent;
using SymbianEGLBindings::eglSwapBuffers;
using SymbianEGLBindings::eglQuerySurface;
using SymbianEGLBindings::eglGetCurrentContext;
using SymbianEGLBindings::eglGetCurrentDisplay;
using SymbianEGLBindings::eglGetCurrentSurface;
using SymbianEGLBindings::eglQueryString;
using SymbianEGLBindings::eglGetError;
}
