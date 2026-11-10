// Copyright (C) 2026 The symbian-webkit authors. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include <e32std.h>
class TSurfaceId {
public:
    std::array<TUint32,4> words{};
    bool IsNull() const { return words == std::array<TUint32,4>{}; }
};
