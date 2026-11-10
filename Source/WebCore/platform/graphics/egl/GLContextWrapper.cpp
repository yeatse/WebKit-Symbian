/*
 * Copyright (C) 2024 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#include "config.h"
#include "GLContextWrapper.h"

#include "GLContext.h"
#include "GraphicsContextGLTextureMapperANGLE.h"
#if OS(SYMBIAN)
#include <wtf/NeverDestroyed.h>
#include <wtf/ThreadSpecific.h>
#endif

namespace WebCore {

static GLContextWrapper*& currentContextStorage()
{
#if OS(SYMBIAN)
    // E32 has no ELF TLS block / __aeabi_read_tp. Preserve per-thread current
    // context tracking through the same pthread-backed TLS used by WTF.
    static NeverDestroyed<ThreadSpecific<GLContextWrapper*>> storage;
    return *storage.get();
#else
    static thread_local constinit GLContextWrapper* context = nullptr;
    return context;
#endif
}

GLContextWrapper::~GLContextWrapper()
{
    if (currentContextStorage() == this)
        currentContextStorage() = nullptr;
}

bool GLContextWrapper::isCurrent() const
{
    return currentContextStorage() == this;
}

GLContextWrapper* GLContextWrapper::currentContext()
{
    return currentContextStorage();
}

void GLContextWrapper::didMakeContextCurrent()
{
    currentContextStorage() = this;
}

void GLContextWrapper::didUnmakeContextCurrent()
{
    currentContextStorage() = nullptr;
}

} // namespace WebCore
