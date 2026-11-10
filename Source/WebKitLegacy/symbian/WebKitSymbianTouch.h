// Native/engine boundary types. No Symbian or WebCore types or allocations.
#pragma once

namespace WebKitSymbian {

enum class TouchState { Pressed, Moved, Released, Stationary, Cancelled };

struct TouchPoint {
    unsigned identifier { 0 };
    TouchState state { TouchState::Stationary };
    int x { 0 };
    int y { 0 };
    int screenX { 0 };
    int screenY { 0 };
    float pressure { 0 };
    bool isPrimary { false };
};

// The embedding reports the smaller of its enabled native capacity and this
// storage bound. It never reports fabricated simultaneous contacts.
constexpr unsigned maximumSupportedTouchPoints = 32;

} // namespace WebKitSymbian
