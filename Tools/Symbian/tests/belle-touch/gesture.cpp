#include "SymbianTouchGesture.h"
#include <cassert>
#include <cstdio>

using namespace WebKitSymbian;

int main()
{
    TouchGesture gesture;
    TouchPoint points[2] = { { 0, TouchState::Pressed, 10, 10, 10, 10, 0.5f, true } };
    auto send = [&](TouchState state, int x, int y, bool prevented = false) {
        points[0].state = state; points[0].x = x; points[0].y = y;
        return gesture.update(points, 1, prevented);
    };
    assert(send(TouchState::Pressed, 10, 10).action == TouchGesture::Action::None);
    assert(send(TouchState::Released, 12, 12).action == TouchGesture::Action::Tap);
    assert(!gesture.isActive());
    send(TouchState::Pressed, 10, 10);
    assert(send(TouchState::Released, 10, 40).action == TouchGesture::Action::None);
    points[0] = { 0, TouchState::Pressed, 10, 10, 10, 10, 0.5f, true };
    gesture.update(points, 1, false, true);
    assert(send(TouchState::Moved, 10, 40).action == TouchGesture::Action::BeginScroll);
    assert(send(TouchState::Released, 10, 40).action == TouchGesture::Action::None);
    points[0] = { 0, TouchState::Pressed, 10, 10, 10, 10, 0.5f, true };
    gesture.update(points, 1, false, true);
    assert(send(TouchState::Released, 10, 10).action == TouchGesture::Action::None);
    gesture.reset();
    const int low = std::numeric_limits<int>::min();
    const int high = std::numeric_limits<int>::max();
    send(TouchState::Pressed, low, high);
    auto extremes = send(TouchState::Moved, high, low);
    assert(extremes.action == TouchGesture::Action::BeginScroll);
    assert(extremes.deltaX == high && extremes.deltaY == low);
    extremes = send(TouchState::Moved, low, high);
    assert(extremes.action == TouchGesture::Action::Scroll);
    assert(extremes.deltaX == low && extremes.deltaY == high);
    send(TouchState::Cancelled, low, high);
    send(TouchState::Pressed, 10, 10, true);
    assert(send(TouchState::Released, 10, 10).action == TouchGesture::Action::None);
    send(TouchState::Pressed, 10, 10);
    assert(send(TouchState::Moved, 10, 30, true).action == TouchGesture::Action::None);
    assert(send(TouchState::Moved, 10, 40).action == TouchGesture::Action::None);
    assert(send(TouchState::Released, 10, 40).action == TouchGesture::Action::None);
    gesture.setPanDirections(false, false);
    send(TouchState::Pressed, 10, 10);
    assert(send(TouchState::Moved, 10, 40).action == TouchGesture::Action::None);
    assert(send(TouchState::Released, 10, 40).action == TouchGesture::Action::None);
    gesture.setPanDirections(false, true);
    send(TouchState::Pressed, 10, 10);
    auto action = send(TouchState::Moved, 30, 40);
    assert(action.action == TouchGesture::Action::BeginScroll && action.deltaX == 0 && action.deltaY == 30);
    action = send(TouchState::Moved, 35, 45);
    assert(action.action == TouchGesture::Action::Scroll && action.deltaX == 0 && action.deltaY == 5);
    assert(send(TouchState::Released, 35, 45).action == TouchGesture::Action::None);
    send(TouchState::Pressed, 10, 10);
    points[0].state = TouchState::Stationary;
    points[1] = { 1, TouchState::Pressed, 20, 20, 20, 20, 0.5f, false };
    gesture.update(points, 2, false);
    points[1].state = TouchState::Released;
    gesture.update(points, 2, false);
    assert(send(TouchState::Moved, 10, 40).action == TouchGesture::Action::None);
    assert(send(TouchState::Released, 10, 40).action == TouchGesture::Action::None);
    send(TouchState::Pressed, 10, 10);
    assert(send(TouchState::Cancelled, 10, 10).action == TouchGesture::Action::None);
    assert(!gesture.isActive());
    assert(send(TouchState::Released, 10, 10).action == TouchGesture::Action::None);
    std::puts("PASS: touch preventDefault, pan axes, one scroll start, tap suppression, multi-contact and cancellation");
}
