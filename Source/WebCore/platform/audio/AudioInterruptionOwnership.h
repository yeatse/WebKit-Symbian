/* Copyright (C) 2026 The symbian-webkit authors. SPDX-License-Identifier: BSD-2-Clause */
#pragma once
#include <cstdint>
namespace WebCore {
// An owned end can only mark its matching entry; draining is still LIFO.
// Duplicate end requests can restrict, but never grant, resume permission.
struct AudioInterruptionOwnership {
    uint64_t token { 0 };
    bool endPending { false };
    uint8_t endFlags { 0 };
    bool requestEnd(uint64_t owner, uint8_t flags)
    {
        if (!owner || owner != token)
            return false;
        markEnd(flags);
        return true;
    }
    bool requestLegacyEnd(uint8_t flags)
    {
        if (token || endPending)
            return false;
        markEnd(flags);
        return true;
    }
private:
    void markEnd(uint8_t flags)
    {
        endFlags = endPending ? endFlags & flags : flags;
        endPending = true;
    }
};
} // namespace WebCore
