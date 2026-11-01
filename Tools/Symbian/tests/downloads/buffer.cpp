#include "CurlDownloadBuffer.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <limits>
#include <mutex>
#include <thread>

using WebCore::CurlDownloadBuffer;
using Reservation = CurlDownloadBuffer::Reservation;
static unsigned checks;
static void check(bool value, const char* name)
{
    ++checks;
    if (!value) { std::printf("FAIL: %s\n", name); std::exit(1); }
    std::printf("PASS: %s\n", name);
}
int main()
{
    CurlDownloadBuffer budget;
    check(budget.reserve(4096) == Reservation::Disabled && !budget.pending(), "normal resource loading does not acquire download accounting");
    check(budget.setLimit(256 * 1024), "download enables a fixed queue limit");
    for (unsigned i = 0; i < 16; ++i)
        check(budget.reserve(16 * 1024) == Reservation::Accepted, "complete curl chunk accepted below or at boundary");
    check(budget.full() && budget.pending() == 256 * 1024, "queue reaches but never exceeds high-water mark");
    check(budget.reserve(16 * 1024) == Reservation::Paused && budget.pending() == 256 * 1024, "paused replay has not allocated or charged another chunk");
    check(!budget.setLimit(0), "active queue cannot silently lose its accounting");
    check(budget.release(16 * 1024) && budget.reserve(16 * 1024) == Reservation::Accepted, "draining permits the same paused chunk exactly once");
    check(!budget.release(256 * 1024 + 1) && budget.pending() == 256 * 1024, "invalid release cannot underflow");
    check(budget.release(256 * 1024) && !budget.pending(), "stale or cancelled callbacks can release all retained bytes");
    check(budget.reserve(std::numeric_limits<size_t>::max()) == Reservation::TooLarge && !budget.pending(), "oversized chunk fails instead of overflowing or pausing forever");
    check(budget.setLimit(0) && budget.reserve(1) == Reservation::Disabled, "idle queue can return to ordinary loading");

    budget.setLimit(256 * 1024);
    std::deque<size_t> queue;
    std::mutex mutex;
    std::atomic<bool> complete { false }, broken { false };
    std::thread producer([&] {
        for (size_t i = 0; i < 50000; ++i) {
            size_t size = 1 + (i * 7919) % (16 * 1024);
            for (;;) {
                auto result = budget.reserve(size);
                if (result == Reservation::Accepted)
                    break;
                if (result != Reservation::Paused) broken = true;
                std::this_thread::yield();
            }
            if (budget.pending() > budget.limit()) broken = true;
            std::lock_guard lock(mutex); queue.push_back(size);
        }
        complete = true;
    });
    size_t consumed = 0;
    while (!complete || budget.pending()) {
        size_t size = 0;
        { std::lock_guard lock(mutex); if (!queue.empty()) { size = queue.front(); queue.pop_front(); } }
        if (!size) { std::this_thread::yield(); continue; }
        if (!budget.release(size)) broken = true;
        ++consumed;
    }
    producer.join();
    check(!broken && !budget.pending() && consumed == 50000, "real producer/consumer threads preserve bound and exactly-once accounting");
    std::printf("RESULT checks=%u failures=0 scope=production-download-buffer\n", checks);
}
