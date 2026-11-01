/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <wtf/ExportMacros.h>

namespace WTF {

// Called by the embedding before it removes the native active scheduler/run
// loop bridge. Cancels native requests and pending main-thread relief work.
WTF_EXPORT_PRIVATE void stopMemoryPressureMonitoring();

} // namespace WTF
