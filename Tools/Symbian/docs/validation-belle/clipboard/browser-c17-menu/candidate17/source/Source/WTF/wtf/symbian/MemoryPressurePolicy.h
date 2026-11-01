/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <stdint.h>

namespace WTF::SymbianSystem {

// System free RAM is a pressure signal, never a process allocation limit or RSS.
// No allocator calls this policy. Its owner samples on the main run loop.
class MemoryPressurePolicy {
public:
    enum class Level : uint8_t { Normal, Warning, Critical };
    struct Decision {
        Level level;
        bool release;
    };

    static constexpr uint64_t mebibyte = 1024 * 1024;
    static constexpr uint64_t minimumReleaseIntervalMS = 15000;

    Decision sample(uint64_t nowMS, bool hasFreeRAM, uint64_t freeRAM, bool nativeLowMemory, bool allowRelease = true)
    {
        auto previous = m_level;
        if (nativeLowMemory || (hasFreeRAM && freeRAM <= 8 * mebibyte))
            m_level = Level::Critical;
        else if (hasFreeRAM) {
            if (freeRAM >= 24 * mebibyte)
                m_level = Level::Normal;
            else if (m_level == Level::Critical) {
                if (freeRAM >= 12 * mebibyte)
                    m_level = Level::Warning;
            } else if (freeRAM <= 16 * mebibyte)
                m_level = Level::Warning;
        }

        // Permit one immediate escalation from a noncritical release. Repeated
        // notifications, and recovery followed by pressure, retain the cooldown.
        bool escalated = m_level == Level::Critical && previous != Level::Critical
            && m_lastReleaseLevel != Level::Critical;
        bool intervalElapsed = nowMS >= m_lastReleaseMS
            && nowMS - m_lastReleaseMS >= minimumReleaseIntervalMS;
        bool release = allowRelease && m_level != Level::Normal && (!m_hasReleased || escalated || intervalElapsed);
        if (release) {
            m_hasReleased = true;
            m_lastReleaseMS = nowMS;
            m_lastReleaseLevel = m_level;
        }
        return { m_level, release };
    }

    Level level() const { return m_level; }

private:
    Level m_level { Level::Normal };
    Level m_lastReleaseLevel { Level::Normal };
    uint64_t m_lastReleaseMS { 0 };
    bool m_hasReleased { false };
};

} // namespace WTF::SymbianSystem
