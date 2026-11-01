#include "TouchTracker.h"
#include <cassert>
#include <cstdio>

using namespace SymbianBrowser;
using WebKitSymbian::TouchState;

int main()
{
    TouchFrame frame;
    TouchTracker disabled;
    assert(!disabled.update(TouchState::Pressed, 0, 1, 2, 11, 22, 0.5f, frame));
    TouchTracker tracker(3);
    assert(tracker.update(TouchState::Pressed, 2, 1, 2, 11, 22, 0.5f, frame));
    assert(frame.count == 1 && frame.points[0].identifier == 2 && frame.points[0].isPrimary);
    assert(tracker.contains(2) && !tracker.contains(0));
    assert(frame.points[0].screenY == 22 && frame.points[0].state == TouchState::Pressed);
    assert(tracker.update(TouchState::Pressed, 0, 3, 4, 13, 24, 0.7f, frame));
    assert(frame.count == 2 && frame.points[0].state == TouchState::Stationary && !frame.points[1].isPrimary);
    assert(tracker.update(TouchState::Moved, 0, 8, 9, 18, 29, 0.8f, frame));
    assert(frame.count == 2 && frame.points[0].x == 1 && frame.points[1].x == 8);
    assert(frame.points[0].state == TouchState::Stationary && frame.points[1].state == TouchState::Moved);
    assert(!tracker.update(TouchState::Pressed, 0, 99, 99, 99, 99, 1, frame));
    assert(tracker.activeCount() == 2 && !frame.count);
    assert(tracker.update(TouchState::Released, 2, 2, 3, 12, 23, 1, frame));
    assert(frame.count == 2 && frame.points[0].state == TouchState::Released && frame.points[0].isPrimary);
    assert(!frame.points[0].pressure && tracker.activeCount() == 1);
    assert(!tracker.contains(2) && tracker.contains(0));
    // The surviving contact, and a reused native number, do not become primary
    // until the whole sequence has ended.
    assert(tracker.update(TouchState::Pressed, 2, 5, 6, 15, 26, 0.5f, frame));
    assert(!frame.points[0].isPrimary && !frame.points[1].isPrimary);
    assert(tracker.cancelAll(frame));
    assert(frame.count == 2 && !tracker.activeCount());
    for (unsigned i = 0; i < frame.count; ++i)
        assert(frame.points[i].state == TouchState::Cancelled && !frame.points[i].pressure);
    assert(!tracker.update(TouchState::Moved, 0, 0, 0, 0, 0, 0.5f, frame));
    assert(!tracker.update(TouchState::Released, 2, 0, 0, 0, 0, 0, frame));
    assert(!tracker.cancelAll(frame));
    assert(tracker.update(TouchState::Pressed, 0, 7, 8, 17, 28, 0.5f, frame));
    assert(frame.points[0].isPrimary);
    assert(!tracker.update(TouchState::Pressed, 3, 0, 0, 0, 0, 0, frame));
    TouchTracker bounded(1000);
    assert(bounded.capacity() == WebKitSymbian::maximumSupportedTouchPoints);
    std::puts("PASS: native touch snapshots, identity reuse, primary lifetime, cancellation, capacity");
}
