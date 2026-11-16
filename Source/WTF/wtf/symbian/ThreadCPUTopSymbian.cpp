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

#include <e32std.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace WTF {
namespace SymbianSystem {

unsigned currentThreadIdentifier()
{
    return static_cast<unsigned>(RThread().Id().Id());
}

size_t threadCPUTop(char* out, size_t capacity)
{
    struct Sample {
        unsigned long long id;
        long long time;
    };
    struct Top {
        long long delta;
        unsigned id;
        char name[56];
    };
    static const int maximumThreads = 768;
    static const int topCount = 6;
    static Sample* previous;
    static Sample* current;
    static int previousCount;
    if (!previous) {
        previous = static_cast<Sample*>(malloc(sizeof(Sample) * maximumThreads));
        current = static_cast<Sample*>(malloc(sizeof(Sample) * maximumThreads));
    }
    if (!capacity || !previous || !current)
        return 0;

    Top top[topCount];
    memset(top, 0, sizeof(top));
    int count = 0;
    TFindThread find;
    TFullName name;
    while (count < maximumThreads && find.Next(name) == KErrNone) {
        RThread thread;
        if (thread.Open(find) != KErrNone)
            continue;
        TTimeIntervalMicroSeconds time(0);
        unsigned long long id = thread.Id().Id();
        TInt error = thread.GetCpuTime(time);
        thread.Close();
        if (error != KErrNone)
            continue;
        current[count].id = id;
        current[count].time = time.Int64();
        long long delta = -1;
        for (int i = 0; i < previousCount; ++i) {
            if (previous[i].id == id) {
                delta = time.Int64() - previous[i].time;
                break;
            }
        }
        ++count;
        if (delta <= 0)
            continue;
        int slot = topCount - 1;
        if (delta <= top[slot].delta)
            continue;
        while (slot > 0 && top[slot - 1].delta < delta) {
            top[slot] = top[slot - 1];
            --slot;
        }
        top[slot].delta = delta;
        top[slot].id = static_cast<unsigned>(id);
        int length = name.Length() < static_cast<int>(sizeof(top[slot].name)) - 1 ? name.Length() : static_cast<int>(sizeof(top[slot].name)) - 1;
        for (int i = 0; i < length; ++i)
            top[slot].name[i] = name[i] < 0x7f && name[i] > 0x20 ? static_cast<char>(name[i]) : '_';
        top[slot].name[length] = 0;
    }
    Sample* swap = previous;
    previous = current;
    current = swap;
    previousCount = count;

    size_t written = 0;
    out[0] = 0;
    for (int i = 0; i < topCount && top[i].delta > 0 && written < capacity; ++i) {
        int result = snprintf(out + written, capacity - written, "%s%s#%u=%dms", i ? " " : "", top[i].name, top[i].id, static_cast<int>(top[i].delta / 1000));
        if (result < 0)
            break;
        written += result;
    }
    return written < capacity ? written : capacity - 1;
}

} // namespace SymbianSystem
} // namespace WTF
