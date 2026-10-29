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

#include <limits.h>
#include <stdlib.h>
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

void lowerCurrentThreadPriority()
{
    RThread().SetPriority(EPriorityLess);
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

struct Reservation {
    TUint8* base;
    size_t size;
    TInt handle;
    size_t offset;
    unsigned char* committed;
    bool executable;
};
static constexpr size_t memoryPageSize = 4096;
static constexpr int maxReservations = 256;
static Reservation reservations[maxReservations];
static pthread_mutex_t reservationLock = PTHREAD_MUTEX_INITIALIZER;

// Caller holds reservationLock through each kernel operation and its accounting.
static Reservation* reservationContaining(const void* address, size_t bytes)
{
    uintptr_t p = reinterpret_cast<uintptr_t>(address);
    if (p % memoryPageSize || bytes % memoryPageSize)
        return nullptr;
    for (auto& reservation : reservations) {
        uintptr_t base = reinterpret_cast<uintptr_t>(reservation.base);
        if (reservation.handle && p >= base && p - base <= reservation.size && bytes <= reservation.size - (p - base))
            return &reservation;
    }
    return nullptr;
}

bool displaySize(int& width, int& height)
{
    TInt x = 0;
    TInt y = 0;
    if (HAL::Get(HAL::EDisplayXPixels, x) != KErrNone || HAL::Get(HAL::EDisplayYPixels, y) != KErrNone)
        return false;
    width = x;
    height = y;
    return true;
}

void* reserve(size_t bytes, bool executable, size_t alignment, unsigned guardPages)
{
    // Local code chunks are contiguous and fully committed. Do not claim that
    // their padding is protected: guarded reservations remain data-only.
    if ((executable && guardPages) || !bytes || bytes % memoryPageSize || alignment < memoryPageSize || (alignment & (alignment - 1)))
        return nullptr;
    if (bytes > INT_MAX || alignment > INT_MAX || guardPages > static_cast<size_t>(INT_MAX) / (2 * memoryPageSize))
        return nullptr;
    size_t guards = guardPages * memoryPageSize;
    size_t extra = alignment - memoryPageSize;
    if (extra > static_cast<size_t>(INT_MAX) - bytes || 2 * guards > static_cast<size_t>(INT_MAX) - bytes - extra)
        return nullptr;
    size_t total = bytes + extra + 2 * guards;
    auto* committed = static_cast<unsigned char*>(calloc(bytes / memoryPageSize, 1));
    if (!committed)
        return nullptr;
    RChunk chunk;
    TInt error = executable
        ? chunk.CreateLocalCode(static_cast<TInt>(total), static_cast<TInt>(total), EOwnerProcess)
        : chunk.CreateDisconnectedLocal(0, 0, static_cast<TInt>(total), EOwnerProcess);
    if (error != KErrNone) {
        free(committed);
        return nullptr;
    }
    uintptr_t chunkBase = reinterpret_cast<uintptr_t>(chunk.Base());
    uintptr_t base = (chunkBase + guards + alignment - 1) & ~(alignment - 1);
    pthread_mutex_lock(&reservationLock);
    for (auto& reservation : reservations) {
        if (reservation.handle)
            continue;
        reservation = { reinterpret_cast<TUint8*>(base), bytes, chunk.Handle(), base - chunkBase, committed, executable };
        pthread_mutex_unlock(&reservationLock);
        return reservation.base;
    }
    pthread_mutex_unlock(&reservationLock);
    chunk.Close();
    free(committed);
    return nullptr;
}

bool commit(void* address, size_t bytes, bool executable)
{
    if (!bytes)
        return true;
    pthread_mutex_lock(&reservationLock);
    auto* reservation = reservationContaining(address, bytes);
    if (!reservation || reservation->executable != executable) {
        pthread_mutex_unlock(&reservationLock);
        return false;
    }
    RChunk chunk;
    chunk.SetHandle(reservation->handle);
    size_t first = (reinterpret_cast<uintptr_t>(address) - reinterpret_cast<uintptr_t>(reservation->base)) / memoryPageSize;
    size_t end = first + bytes / memoryPageSize;
    bool success = true;
    for (size_t page = first; page < end; ++page) {
        if (reservation->committed[page])
            continue;
        if (!reservation->executable && chunk.Commit(reservation->offset + page * memoryPageSize, memoryPageSize) != KErrNone) {
            success = false;
            break;
        }
        Mem::FillZ(reservation->base + page * memoryPageSize, memoryPageSize);
        reservation->committed[page] = 2;
    }
    for (size_t page = first; page < end; ++page) {
        if (reservation->committed[page] != 2)
            continue;
        if (!success)
            chunk.Decommit(reservation->offset + page * memoryPageSize, memoryPageSize);
        reservation->committed[page] = success ? 1 : 0;
    }
    pthread_mutex_unlock(&reservationLock);
    return success;
}

bool decommit(void* address, size_t bytes, bool allowExecutable)
{
    if (!bytes)
        return true;
    pthread_mutex_lock(&reservationLock);
    auto* reservation = reservationContaining(address, bytes);
    if (!reservation || (reservation->executable && !allowExecutable)) {
        pthread_mutex_unlock(&reservationLock);
        return false;
    }
    RChunk chunk;
    chunk.SetHandle(reservation->handle);
    size_t first = (reinterpret_cast<uintptr_t>(address) - reinterpret_cast<uintptr_t>(reservation->base)) / memoryPageSize;
    size_t end = first + bytes / memoryPageSize;
    bool success = true;
    for (size_t page = first; page < end; ++page) {
        if (!reservation->committed[page])
            continue;
        if (!reservation->executable && chunk.Decommit(reservation->offset + page * memoryPageSize, memoryPageSize) != KErrNone) {
            success = false;
            break;
        }
        reservation->committed[page] = 0;
    }
    pthread_mutex_unlock(&reservationLock);
    return success;
}

void release(void* base)
{
    pthread_mutex_lock(&reservationLock);
    for (auto& reservation : reservations) {
        if (reservation.handle && reservation.base == base) {
            RChunk chunk;
            chunk.SetHandle(reservation.handle);
            chunk.Close();
            free(reservation.committed);
            reservation = { };
            break;
        }
    }
    pthread_mutex_unlock(&reservationLock);
}

void flushInstructionCache(void* address, size_t bytes)
{
    User::IMB_Range(address, static_cast<TUint8*>(address) + bytes);
}

size_t commandLine(char* buffer, size_t capacity)
{
    if (!capacity)
        return 0;
    size_t written = 0;
    HBufC* line = HBufC::New(User::CommandLineLength());
    if (line) {
        TPtr16 text = line->Des();
        User::CommandLine(text);
        for (TInt i = 0; i < text.Length(); ++i) {
            uint32_t c = text[i];
            if (c >= 0xD800 && c < 0xDC00 && i + 1 < text.Length() && text[i + 1] >= 0xDC00 && text[i + 1] < 0xE000)
                c = 0x10000 + ((c - 0xD800) << 10) + (text[++i] - 0xDC00);
            unsigned char bytes[4];
            size_t count;
            if (c < 0x80) {
                bytes[0] = c;
                count = 1;
            } else if (c < 0x800) {
                bytes[0] = 0xC0 | (c >> 6);
                bytes[1] = 0x80 | (c & 0x3F);
                count = 2;
            } else if (c < 0x10000) {
                bytes[0] = 0xE0 | (c >> 12);
                bytes[1] = 0x80 | ((c >> 6) & 0x3F);
                bytes[2] = 0x80 | (c & 0x3F);
                count = 3;
            } else {
                bytes[0] = 0xF0 | (c >> 18);
                bytes[1] = 0x80 | ((c >> 12) & 0x3F);
                bytes[2] = 0x80 | ((c >> 6) & 0x3F);
                bytes[3] = 0x80 | (c & 0x3F);
                count = 4;
            }
            if (written + count >= capacity)
                break;
            memcpy(buffer + written, bytes, count);
            written += count;
        }
        User::Free(line); // From the user heap; the global delete is mimalloc's.
    }
    buffer[written] = 0;
    return written;
}

extern "C" int mbedtls_hardware_poll(void*, unsigned char*, size_t, size_t*);

bool randomBytes(unsigned char* buffer, size_t length)
{
    size_t offset = 0;
    while (offset < length) {
        size_t count = length - offset;
        if (count > 1024)
            count = 1024;
        size_t written = 0;
        if (mbedtls_hardware_poll(nullptr, buffer + offset, count, &written) || written != count) {
            memset(buffer, 0, length);
            return false;
        }
        offset += count;
    }
    return true;
}

} // namespace SymbianSystem
} // namespace WTF
