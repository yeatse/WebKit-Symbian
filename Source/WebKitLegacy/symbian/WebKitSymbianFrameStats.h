/* Copyright (C) 2026 The symbian-webkit authors. All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include <stdio.h>

namespace WebKitSymbian {

// Engine-side switch, set through WebView::setFrameStatsEnabled().
bool frameStatsEnabled();

// Per-stage frame timings, summarized about once a second of presented frames.
// Shared by the engine (EGL) and the browser (ScreenPlay and bitmap), which
// supply their own clocks. Durations are milliseconds.
class FrameStats {
public:
    static constexpr int maximumStages = 5;

    FrameStats(const char* presentation, const char* const (&stages)[maximumStages], int stageCount)
        : m_presentation(presentation)
        , m_stageCount(stageCount < maximumStages ? stageCount : maximumStages)
    {
        for (int i = 0; i < m_stageCount; ++i)
            m_stages[i] = stages[i];
        reset();
    }

    // Returns true when a summary is due; format() then writes and resets it.
    bool add(const double (&durations)[maximumStages], double sinceLastFrame)
    {
        ++m_frames;
        m_window += sinceLastFrame;
        for (int i = 0; i < m_stageCount; ++i) {
            m_sum[i] += durations[i];
            if (durations[i] > m_max[i])
                m_max[i] = durations[i];
        }
        return m_window >= 1000 || m_frames >= 120;
    }

    void format(char* buffer, int size)
    {
        int used = snprintf(buffer, size, "frame-stats: %s frames=%u window=%.0fms", m_presentation, m_frames, m_window);
        for (int i = 0; i < m_stageCount && used > 0 && used < size; ++i)
            used += snprintf(buffer + used, size - used, " %s=%.2f/%.2f", m_stages[i], m_sum[i] / m_frames, m_max[i]);
        reset();
    }

private:
    void reset()
    {
        m_frames = 0;
        m_window = 0;
        for (int i = 0; i < maximumStages; ++i)
            m_sum[i] = m_max[i] = 0;
    }

    const char* m_presentation;
    const char* m_stages[maximumStages] { };
    int m_stageCount;
    unsigned m_frames;
    double m_window;
    double m_sum[maximumStages];
    double m_max[maximumStages];
};

} // namespace WebKitSymbian
