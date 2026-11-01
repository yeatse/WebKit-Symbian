/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <stdint.h>

namespace WTF::SymbianSystem {

// Native SDK declarations stay in the implementation. All operations, including
// destruction, belong to the main thread with its active scheduler installed.
using MemoryPressureNotification = void (*)(void* context, unsigned changes, int error);
constexpr unsigned memoryChangeFree = 0x20;
constexpr unsigned memoryChangeLow = 0x80;
void* createMemoryPressureMonitor(MemoryPressureNotification, void* context, int& error);
void destroyMemoryPressureMonitor(void*);
bool systemFreeMemory(uint64_t& bytes);

} // namespace WTF::SymbianSystem
