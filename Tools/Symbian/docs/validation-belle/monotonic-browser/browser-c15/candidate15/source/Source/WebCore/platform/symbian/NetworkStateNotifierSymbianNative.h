/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

namespace WebCore::SymbianNetworkState {

// All operations and callbacks belong to the creating active-scheduler thread.
// Native descriptors and leave exceptions never cross this boundary. Creation
// starts an asynchronous snapshot; state() returns false until one is known.
using ChangeCallback = void (*)(void*);
void* createObserver(ChangeCallback, void* context);
void destroyObserver(void*);
bool state(void*, bool& online);

} // namespace WebCore::SymbianNetworkState
