// Tracks native contact lifetimes and takes complete, allocation-free event
// snapshots. Native pointer numbers are identifiers, not contact counts.
#pragma once

#include <WebKitSymbianTouch.h>
#include <array>

namespace SymbianBrowser {

struct TouchFrame {
    std::array<WebKitSymbian::TouchPoint, WebKitSymbian::maximumSupportedTouchPoints> points;
    unsigned count { 0 };
};

class TouchTracker {
public:
    explicit TouchTracker(unsigned capacity = 0)
        : m_capacity(capacity < WebKitSymbian::maximumSupportedTouchPoints ? capacity : WebKitSymbian::maximumSupportedTouchPoints)
    {
    }

    unsigned capacity() const { return m_capacity; }
    unsigned activeCount() const { return m_count; }
    bool contains(unsigned identifier) const
    {
        for (unsigned i = 0; i < m_count; ++i) {
            if (m_points[i].identifier == identifier)
                return true;
        }
        return false;
    }

    bool update(WebKitSymbian::TouchState state, unsigned identifier, int x, int y,
        int screenX, int screenY, float pressure, TouchFrame& frame)
    {
        using WebKitSymbian::TouchState;
        frame.count = 0;
        if (identifier >= m_capacity || (state != TouchState::Pressed && state != TouchState::Moved && state != TouchState::Released))
            return false;
        unsigned index = 0;
        while (index < m_count && m_points[index].identifier != identifier)
            ++index;
        if (state == TouchState::Pressed) {
            // A repeated down must not invent another start or change the
            // primary pointer. Wait for the matching up/cancellation.
            if (index != m_count || m_count == m_capacity)
                return false;
            m_points[m_count++] = { identifier, TouchState::Stationary, x, y, screenX, screenY, pressure, index == 0 };
        } else if (index == m_count)
            return false;

        auto& changed = m_points[index];
        changed.x = x;
        changed.y = y;
        changed.screenX = screenX;
        changed.screenY = screenY;
        changed.pressure = state == TouchState::Released ? 0 : pressure;
        frame.count = m_count;
        for (unsigned i = 0; i < m_count; ++i) {
            frame.points[i] = m_points[i];
            frame.points[i].state = i == index ? state : TouchState::Stationary;
        }
        if (state == TouchState::Released) {
            for (unsigned i = index + 1; i < m_count; ++i)
                m_points[i - 1] = m_points[i];
            --m_count;
        }
        return true;
    }

    bool cancelAll(TouchFrame& frame)
    {
        frame.count = m_count;
        for (unsigned i = 0; i < m_count; ++i) {
            frame.points[i] = m_points[i];
            frame.points[i].state = WebKitSymbian::TouchState::Cancelled;
            frame.points[i].pressure = 0;
        }
        clear();
        return frame.count;
    }

    void clear() { m_count = 0; }

private:
    std::array<WebKitSymbian::TouchPoint, WebKitSymbian::maximumSupportedTouchPoints> m_points;
    unsigned m_capacity;
    unsigned m_count { 0 };
};

} // namespace SymbianBrowser
