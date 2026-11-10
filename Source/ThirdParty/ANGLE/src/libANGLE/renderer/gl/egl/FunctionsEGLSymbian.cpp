// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libANGLE/renderer/gl/egl/FunctionsEGLSymbian.h"
#include "libANGLE/renderer/gl/egl/functionsegl_typedefs.h"

#include <algorithm>
#include <cstring>
#include <e32std.h>

namespace rx
{
namespace
{
struct NativeEntryPoint
{
    const char *name;
    TInt ordinal;
};

#include "libANGLE/renderer/gl/egl/SymbianNativeExports.inc"

template <size_t count>
TLibraryFunction findExport(const RLibrary &library,
                            const NativeEntryPoint (&entries)[count],
                            const char *name)
{
    auto entry = std::lower_bound(std::begin(entries), std::end(entries), name,
                                 [](const NativeEntryPoint &entry, const char *key) {
                                     return std::strcmp(entry.name, key) < 0;
                                 });
    if (entry == std::end(entries) || std::strcmp(entry->name, name))
        return nullptr;
    return library.Lookup(entry->ordinal);
}
}  // namespace

struct FunctionsEGLSymbian::Libraries
{
    RLibrary egl;
    RLibrary gles;
    PFNEGLGETPROCADDRESSPROC getExtension = nullptr;

    ~Libraries()
    {
        gles.Close();
        egl.Close();
    }
};

FunctionsEGLSymbian::FunctionsEGLSymbian() : mLibraries(std::make_unique<Libraries>()) {}
FunctionsEGLSymbian::~FunctionsEGLSymbian() = default;

egl::Error FunctionsEGLSymbian::initialize(EGLAttrib platformType,
                                          EGLNativeDisplayType nativeDisplay)
{
    // Native Symbian EGL has 32-bit integer handles. ANGLE's private EGL ABI
    // uses pointer handles; the values have identical ARM softfp representations.
    static_assert(sizeof(EGLDisplay) == sizeof(TInt));
    static_assert(sizeof(EGLConfig) == sizeof(TInt));
    static_assert(sizeof(EGLContext) == sizeof(TInt));
    static_assert(sizeof(EGLSurface) == sizeof(TInt));

    if (platformType)
        return egl::Error(EGL_BAD_ATTRIBUTE, "Symbian requires the native default EGL platform");
    TInt error = mLibraries->egl.Load(_L("libegl.dll"));
    if (error != KErrNone)
        return egl::Error(EGL_NOT_INITIALIZED, "Cannot load libegl.dll: " + std::to_string(error));
    error = mLibraries->gles.Load(_L("libglesv2.dll"));
    if (error != KErrNone)
        return egl::Error(EGL_NOT_INITIALIZED, "Cannot load libglesv2.dll: " + std::to_string(error));
    mLibraries->getExtension = reinterpret_cast<PFNEGLGETPROCADDRESSPROC>(
        findExport(mLibraries->egl, kNativeEGLExports, "eglGetProcAddress"));
    if (!mLibraries->getExtension)
        return egl::Error(EGL_NOT_INITIALIZED, "Native eglGetProcAddress is unavailable");
    return FunctionsEGL::initialize(0, nativeDisplay);
}

void *FunctionsEGLSymbian::getProcAddress(const char *name) const
{
    if (!name)
        return nullptr;
    // Core functions have fixed Symbian ordinals. eglGetProcAddress is only
    // required to return extension functions on real firmware.
    if (auto function = findExport(mLibraries->egl, kNativeEGLExports, name))
        return reinterpret_cast<void *>(function);
    if (auto function = findExport(mLibraries->gles, kNativeGLES2Exports, name))
        return reinterpret_cast<void *>(function);
    return mLibraries->getExtension
               ? reinterpret_cast<void *>(mLibraries->getExtension(name))
               : nullptr;
}
}  // namespace rx
