// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu_info_util/SystemInfo.h"

namespace angle
{
bool GetSystemInfo(SystemInfo *info)
{
    // Symbian has no portable PCI/GPU enumeration API. RendererGL falls back
    // to querying the current driver's vendor and renderer strings.
    *info = SystemInfo();
    return false;
}
}  // namespace angle
