// SPDX-License-Identifier: BSD-2-Clause
// Only platform wrappers are substituted. The setter and wake functions below
// are extracted unchanged from the production source by run.py.
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>
#include <utility>

#define OS(name) 1
namespace WTF {
template<typename> class Function;
// A stable function-pointer adapter avoids destroying a C++ callable while the
// intentionally broken old setter is executing it. It exposes the ordering
// error without making that negative test itself depend on undefined behavior.
template<> class Function<void()> {
public:
    Function(void (*callback)() = nullptr) : m_callback(callback) { }
    Function& operator=(Function&& other) { m_callback.store(other.m_callback.load()); return *this; }
    explicit operator bool() const { return m_callback.load(); }
    void operator()() const { if (auto callback = m_callback.load()) callback(); }
private:
    std::atomic<void (*)()> m_callback;
};
using std::move;
}
class Locker {
public:
    explicit Locker(std::mutex& mutex) : m_lock(mutex) { }
private:
    std::unique_lock<std::mutex> m_lock;
};
struct Condition { void notifyOne() { } };
class RunLoop {
public:
    static RunLoop& currentSingleton() { static RunLoop loop; return loop; }
    static void setWakeUpCallback(WTF::Function<void()>&&);
    void wakeUp();
    void wakeUpWithLock();
    std::mutex m_loopLock;
    Condition m_readyToRun;
    bool m_pendingTasks { false };
    WTF::Function<void()> m_wakeUpCallback;
};
#include "production.inc"

static std::mutex gate;
static std::condition_variable changed;
static bool entered, releaseCallback;
static std::atomic<bool> ownerAlive { true };
static std::atomic<bool> sawDeadOwner { false };
static std::atomic<unsigned> calls { 0 };
static void nativeCallback()
{
    ++calls;
    std::unique_lock lock(gate);
    entered = true;
    changed.notify_all();
    changed.wait(lock, [] { return releaseCallback; });
    sawDeadOwner = !ownerAlive;
}

int main()
{
    auto& loop = RunLoop::currentSingleton();
    RunLoop::setWakeUpCallback(nativeCallback);
    std::thread worker([&] { loop.wakeUp(); });
    {
        std::unique_lock lock(gate);
        changed.wait(lock, [] { return entered; });
    }
    // Callback teardown occurs on the owner thread, exactly as in the browser.
    // A second worker releases the blocked callback after the clear begins.
    std::atomic<bool> clearing { false };
    std::thread releaser([&] {
        while (!clearing.load()) std::this_thread::yield();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        { std::lock_guard lock(gate); releaseCallback = true; }
        changed.notify_all();
    });
    clearing = true;
    RunLoop::setWakeUpCallback({ });
    ownerAlive = false;
    worker.join();
    releaser.join();
    loop.wakeUp();
    bool preserved = !sawDeadOwner;
    bool detached = calls == 1;
    std::printf("%s: unregister waits for in-flight native callback\n", preserved ? "PASS" : "FAIL");
    std::printf("%s: later wakes do not access destroyed native owner\n", detached ? "PASS" : "FAIL");
    return preserved && detached ? 0 : 1;
}
