/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <DownloadFile.h>

namespace WebKitSymbian {

// Ordinary C++ allocation, matching DownloadFile's virtual deleting destructor;
// this adapter deliberately does not inherit CBase's native allocator.
DownloadFile* createNativeDownloadFile();

} // namespace WebKitSymbian
