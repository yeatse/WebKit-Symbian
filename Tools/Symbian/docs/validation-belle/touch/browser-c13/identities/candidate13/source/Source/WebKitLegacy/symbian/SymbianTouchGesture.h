// Default single-finger actions after DOM dispatch. The embedding supplies
// complete contact snapshots and the DOM's default-prevented result.
#pragma once

#include "WebKitSymbianTouch.h"
#include <cstdint>
#include <limits>

namespace WebKitSymbian {

class TouchGesture {
public:
    enum class Action { None, Tap, BeginScroll, Scroll };
    struct Result {
        Action action { Action::None };
        int x { 0 };
        int y { 0 };
        int deltaX { 0 };
        int deltaY { 0 };
        unsigned identifier { 0 };
    };

    void reset() { *this = TouchGesture(); }
    bool isActive() const { return m_active; }
    bool isScrolling() const { return m_scrolling; }
    void setPanDirections(bool x, bool y) { m_panX = x; m_panY = y; }

    Result update(const TouchPoint* points, unsigned count, bool defaultPrevented, bool suppressCompatibilityMouse = false)
    {
        Result result;
        unsigned active = 0;
        for (unsigned i = 0; i < count; ++i) {
            if (points[i].state == TouchState::Cancelled) {
                reset();
                return result;
            }
            if (points[i].state != TouchState::Released)
                ++active;
        }
        if (count == 1 && points[0].state == TouchState::Pressed && points[0].isPrimary) {
            m_active = true;
            m_identifier = points[0].identifier;
            m_startX = m_lastX = points[0].x;
            m_startY = m_lastY = points[0].y;
            m_tap = true;
            m_multiple = false;
            m_prevented = false;
            m_scrolling = false;
        }
        if (!m_active)
            return result;
        if (active > 1) {
            m_multiple = true;
            m_tap = false;
            m_scrolling = false;
        }
        m_prevented |= defaultPrevented;
        // Cancelling pointerdown suppresses compatibility mouse events, while
        // touch-action and cancelling TouchEvents govern the native pan.
        if (m_prevented || suppressCompatibilityMouse)
            m_tap = false;
        for (unsigned i = 0; i < count; ++i) {
            const auto& point = points[i];
            if (point.identifier != m_identifier || !point.isPrimary)
                continue;
            result.identifier = m_identifier;
            result.x = point.x;
            result.y = point.y;
            if (point.state == TouchState::Moved || point.state == TouchState::Released) {
                int64_t movedX = static_cast<int64_t>(point.x) - m_startX;
                int64_t movedY = static_cast<int64_t>(point.y) - m_startY;
                bool pastX = movedX > 8 || movedX < -8;
                bool pastY = movedY > 8 || movedY < -8;
                if (pastX || pastY)
                    m_tap = false;
                if (point.state == TouchState::Moved && !m_prevented && !m_multiple && ((m_panX && pastX) || (m_panY && pastY) || m_scrolling)) {
                    result.action = m_scrolling ? Action::Scroll : Action::BeginScroll;
                    result.deltaX = m_panX ? saturatedDelta(point.x, m_lastX) : 0;
                    result.deltaY = m_panY ? saturatedDelta(point.y, m_lastY) : 0;
                    m_scrolling = true;
                }
                m_lastX = point.x;
                m_lastY = point.y;
            }
            if (point.state == TouchState::Released) {
                if (m_tap && !m_prevented && !m_multiple) {
                    result.action = Action::Tap;
                    result.x = m_startX;
                    result.y = m_startY;
                }
                m_tap = false;
                m_scrolling = false;
            }
        }
        if (!active)
            reset();
        return result;
    }

private:
    static int saturatedDelta(int current, int previous)
    {
        int64_t delta = static_cast<int64_t>(current) - previous;
        if (delta > std::numeric_limits<int>::max())
            return std::numeric_limits<int>::max();
        if (delta < std::numeric_limits<int>::min())
            return std::numeric_limits<int>::min();
        return static_cast<int>(delta);
    }

    bool m_active { false };
    bool m_tap { false };
    bool m_multiple { false };
    bool m_prevented { false };
    bool m_scrolling { false };
    bool m_panX { true };
    bool m_panY { true };
    unsigned m_identifier { 0 };
    int m_startX { 0 };
    int m_startY { 0 };
    int m_lastX { 0 };
    int m_lastY { 0 };
};

} // namespace WebKitSymbian
