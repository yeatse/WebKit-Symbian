/* Copyright (C) 2026 The symbian-webkit authors. All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "config.h"
#include "PlatformDisplaySymbian.h"
#include "GLContext.h"
#include "SymbianEGLBindings.h"

namespace WebCore {
std::unique_ptr<PlatformDisplaySymbian> PlatformDisplaySymbian::create()
{
    auto display = GLDisplay::create(eglGetDisplay(EGL_DEFAULT_DISPLAY));
    if (!display) {
        WTFLogAlways("Symbian compositor: EGL initialization failed: %s", GLContext::lastErrorString());
        return nullptr;
    }
    return std::unique_ptr<PlatformDisplaySymbian>(new PlatformDisplaySymbian(display.releaseNonNull()));
}
PlatformDisplaySymbian::PlatformDisplaySymbian(Ref<GLDisplay>&& display)
    : PlatformDisplay(WTF::move(display))
{
}
}
