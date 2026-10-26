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

// Deliberately free of config.h / WTF headers: see SymbianSystem.h.
#include "SymbianSystem.h"

#include <e32math.h>
#include <e32std.h>
#include <hal.h>
#include <pthread.h>
#include <string.h>

namespace WTF {
namespace SymbianSystem {

int openCurrentThread()
{
    RThread thread;
    if (thread.Open(RThread().Id(), EOwnerProcess) != KErrNone)
        return 0;
    return thread.Handle();
}

static RThread threadFromHandle(int handle)
{
    RThread thread;
    thread.SetHandle(handle);
    return thread;
}

void closeThread(int handle)
{
    if (handle)
        threadFromHandle(handle).Close();
}

void suspendThread(int handle)
{
    threadFromHandle(handle).Suspend();
}

void resumeThread(int handle)
{
    threadFromHandle(handle).Resume();
}

size_t threadContext(int handle, uintptr_t* words, size_t capacity)
{
    TPtr8 context(reinterpret_cast<TUint8*>(words), 0, capacity * sizeof(uintptr_t));
    threadFromHandle(handle).Context(context);
    return context.Length() / sizeof(uintptr_t);
}

bool currentThreadStack(void*& origin, void*& bound)
{
    TThreadStackInfo info;
    if (RThread().StackInfo(info) != KErrNone)
        return false;
    origin = reinterpret_cast<void*>(info.iBase);
    bound = reinterpret_cast<void*>(info.iLimit);
    return true;
}

int64_t currentThreadCPUTimeMicroseconds()
{
    TTimeIntervalMicroSeconds time(0);
    if (RThread().GetCpuTime(time) != KErrNone)
        return 0;
    return time.Int64();
}

size_t ramSize()
{
    TInt bytes = 0;
    if (HAL::Get(HALData::EMemoryRAM, bytes) != KErrNone || bytes <= 0)
        return 64 * 1024 * 1024;
    return static_cast<size_t>(bytes);
}

// The kernel picks the address of a chunk, so the chunk handle of every
// reservation is kept in a small table keyed by base address.
struct Reservation {
    TUint8* base;
    TInt handle;
    bool executable;
};
static const int maxReservations = 256;
static Reservation reservations[maxReservations];
static pthread_mutex_t reservationLock = PTHREAD_MUTEX_INITIALIZER;

static RChunk chunkContaining(const void* address, TInt& offset, bool& executable)
{
    RChunk chunk;
    const TUint8* p = static_cast<const TUint8*>(address);
    pthread_mutex_lock(&reservationLock);
    for (int i = 0; i < maxReservations; ++i) {
        if (!reservations[i].handle)
            continue;
        RChunk candidate;
        candidate.SetHandle(reservations[i].handle);
        if (p >= reservations[i].base && p < reservations[i].base + candidate.MaxSize()) {
            chunk = candidate;
            offset = p - reservations[i].base;
            executable = reservations[i].executable;
            break;
        }
    }
    pthread_mutex_unlock(&reservationLock);
    return chunk;
}

void* reserve(size_t bytes, bool executable)
{
    RChunk chunk;
    TInt size = static_cast<TInt>(bytes);
    TInt error = executable
        ? chunk.CreateLocalCode(size, size, EOwnerProcess)
        : chunk.CreateDisconnectedLocal(0, 0, size, EOwnerProcess);
    if (error != KErrNone)
        return nullptr;
    pthread_mutex_lock(&reservationLock);
    for (int i = 0; i < maxReservations; ++i) {
        if (reservations[i].handle)
            continue;
        reservations[i].base = chunk.Base();
        reservations[i].handle = chunk.Handle();
        reservations[i].executable = executable;
        pthread_mutex_unlock(&reservationLock);
        if (executable)
            Mem::FillZ(chunk.Base(), size);
        return chunk.Base();
    }
    pthread_mutex_unlock(&reservationLock);
    chunk.Close();
    return nullptr;
}

void commit(void* address, size_t bytes)
{
    TInt offset = 0;
    bool executable = false;
    RChunk chunk = chunkContaining(address, offset, executable);
    // Code chunks are committed in full by reserve().
    if (!chunk.Handle() || executable)
        return;
    if (chunk.Commit(offset, static_cast<TInt>(bytes)) == KErrNone)
        Mem::FillZ(static_cast<TUint8*>(address), static_cast<TInt>(bytes));
}

void decommit(void* address, size_t bytes)
{
    TInt offset = 0;
    bool executable = false;
    RChunk chunk = chunkContaining(address, offset, executable);
    if (chunk.Handle() && !executable)
        chunk.Decommit(offset, static_cast<TInt>(bytes));
}

void release(void* base)
{
    pthread_mutex_lock(&reservationLock);
    for (int i = 0; i < maxReservations; ++i) {
        if (reservations[i].handle && reservations[i].base == base) {
            RChunk chunk;
            chunk.SetHandle(reservations[i].handle);
            chunk.Close();
            reservations[i].handle = 0;
            break;
        }
    }
    pthread_mutex_unlock(&reservationLock);
}

void flushInstructionCache(void* address, size_t bytes)
{
    User::IMB_Range(address, static_cast<TUint8*>(address) + bytes);
}

void randomBytes(unsigned char* buffer, size_t length)
{
    while (length) {
        TUint32 word = Math::Random();
        size_t count = length < sizeof(word) ? length : sizeof(word);
        memcpy(buffer, &word, count);
        buffer += count;
        length -= count;
    }
}

} // namespace SymbianSystem
} // namespace WTF
