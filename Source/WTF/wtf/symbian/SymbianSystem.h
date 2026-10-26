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

#pragma once

#include <stddef.h>
#include <stdint.h>

// EKA2 services used by the WTF platform layer. The implementation is the
// only WTF file that includes Symbian headers (e32std.h clashes with WTF and
// libc++ names), so this interface uses plain C types.

namespace WTF {
namespace SymbianSystem {

// A process-owned RThread handle for the calling thread; 0 on failure.
int openCurrentThread();
void closeThread(int handle);

// RThread::Suspend / RThread::Resume.
void suspendThread(int handle);
void resumeThread(int handle);

// RThread::Context of a suspended thread: the user register set (r0-r15,
// cpsr, ...) as words. Returns the number of words written.
size_t threadContext(int handle, uintptr_t* words, size_t capacity);

// RThread::StackInfo of the calling thread: origin is the empty-stack
// address (highest), bound the lowest valid address.
bool currentThreadStack(void*& origin, void*& bound);

// RThread::GetCpuTime of the calling thread.
int64_t currentThreadCPUTimeMicroseconds();

// HAL EMemoryRAM.
size_t ramSize();

// Address space backed by a local RChunk. Data reservations use a
// disconnected chunk and commit page ranges on demand; executable
// reservations use a code chunk and are committed in full up front.
void* reserve(size_t, bool executable);
void commit(void*, size_t);
void decommit(void*, size_t);
void release(void*);

// User::IMB_Range.
void flushInstructionCache(void*, size_t);

// User::CommandLine as UTF-8 (truncated to capacity - 1, NUL-terminated).
// Returns the length written.
size_t commandLine(char*, size_t capacity);

// Math::Random, which Symbian^3 draws from the kernel's secure generator.
void randomBytes(unsigned char*, size_t);

} // namespace SymbianSystem
} // namespace WTF
