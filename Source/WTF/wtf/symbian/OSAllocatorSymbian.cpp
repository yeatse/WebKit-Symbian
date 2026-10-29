/*
 * Copyright (C) 2026 The symbian-webkit authors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "OSAllocator.h"

#if OS(SYMBIAN)

#include <wtf/Assertions.h>
#include <wtf/PageBlock.h>
#include <wtf/symbian/SymbianSystem.h>

namespace WTF {

void* OSAllocator::tryReserveUncommitted(size_t bytes, Usage, void* address, bool writable, bool executable, bool, unsigned guards)
{
    if (address || (executable && !writable))
        return nullptr;
    return SymbianSystem::reserve(bytes, executable, pageSize(), guards);
}

void* OSAllocator::reserveUncommitted(size_t bytes, Usage usage, void* address, bool writable, bool executable, bool jitCageEnabled, unsigned guards)
{
    void* result = tryReserveUncommitted(bytes, usage, address, writable, executable, jitCageEnabled, guards);
    RELEASE_ASSERT(result);
    return result;
}

void* OSAllocator::tryReserveUncommittedAligned(size_t bytes, size_t alignment, Usage, void* address, bool writable, bool executable, bool, unsigned guards)
{
    if (address || (executable && !writable))
        return nullptr;
    return SymbianSystem::reserve(bytes, executable, alignment, guards);
}

void* OSAllocator::tryReserveAndCommit(size_t bytes, Usage usage, void* address, bool writable, bool executable, bool jitCageEnabled, unsigned guards)
{
    if (!writable)
        return nullptr;
    void* result = tryReserveUncommitted(bytes, usage, address, writable, executable, jitCageEnabled, guards);
    if (result && !SymbianSystem::commit(result, bytes, executable)) {
        SymbianSystem::release(result);
        return nullptr;
    }
    return result;
}

void* OSAllocator::reserveAndCommit(size_t bytes, Usage usage, void* address, bool writable, bool executable, bool jitCageEnabled, unsigned guards)
{
    void* result = tryReserveAndCommit(bytes, usage, address, writable, executable, jitCageEnabled, guards);
    RELEASE_ASSERT(result);
    return result;
}

void OSAllocator::commit(void* address, size_t bytes, bool writable, bool executable)
{
    RELEASE_ASSERT(writable);
    RELEASE_ASSERT(SymbianSystem::commit(address, bytes, executable));
}

void OSAllocator::decommit(void* address, size_t bytes)
{
    RELEASE_ASSERT(SymbianSystem::decommit(address, bytes, true));
}

void OSAllocator::hintMemoryNotNeededSoon(void*, size_t) { }

void OSAllocator::releaseDecommitted(void* address, size_t, unsigned)
{
    SymbianSystem::release(address);
}

bool OSAllocator::tryProtect(void* address, size_t bytes, bool readable, bool writable)
{
    if (readable && writable)
        return SymbianSystem::commit(address, bytes);
    if (!readable && !writable)
        return SymbianSystem::decommit(address, bytes);
    return false;
}

void OSAllocator::protect(void* address, size_t bytes, bool readable, bool writable)
{
    RELEASE_ASSERT(tryProtect(address, bytes, readable, writable));
}

} // namespace WTF
#endif // OS(SYMBIAN)
