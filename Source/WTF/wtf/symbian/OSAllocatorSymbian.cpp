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

// EKA2 has no anonymous mmap; each reservation is a local RChunk (see
// SymbianSystem.cpp). Guard pages are left uncommitted in data chunks, which
// faults on access just like PROT_NONE.

void* OSAllocator::reserveUncommitted(size_t bytes, Usage, bool, bool executable, bool, bool)
{
    void* result = SymbianSystem::reserve(bytes, executable);
    if (!result && !executable)
        CRASH();
    return result;
}

void* OSAllocator::reserveAndCommit(size_t bytes, Usage usage, bool writable, bool executable, bool jitCageEnabled, bool includesGuardPages)
{
    void* result = reserveUncommitted(bytes, usage, writable, executable, jitCageEnabled, includesGuardPages);
    if (!result)
        return nullptr;
    if (includesGuardPages)
        commit(static_cast<char*>(result) + pageSize(), bytes - 2 * pageSize(), writable, executable);
    else
        commit(result, bytes, writable, executable);
    return result;
}

void OSAllocator::commit(void* address, size_t bytes, bool, bool)
{
    SymbianSystem::commit(address, bytes);
}

void OSAllocator::decommit(void* address, size_t bytes)
{
    SymbianSystem::decommit(address, bytes);
}

void OSAllocator::hintMemoryNotNeededSoon(void*, size_t)
{
}

void OSAllocator::releaseDecommitted(void* address, size_t)
{
    SymbianSystem::release(address);
}

} // namespace WTF

#endif // OS(SYMBIAN)
