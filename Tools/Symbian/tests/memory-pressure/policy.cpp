#include "MemoryPressurePolicy.h"
#include <cstdio>
#include <cstdlib>
using Policy = WTF::SymbianSystem::MemoryPressurePolicy;
using Level = Policy::Level;
static unsigned checks;
static void expect(bool value) { ++checks; if (!value) { std::fprintf(stderr, "policy check %u failed\n", checks); std::abort(); } }
int main()
{
    constexpr auto MB = Policy::mebibyte;
    Policy policy;
    auto result = policy.sample(0, false, 0, false);
    expect(result.level == Level::Normal && !result.release);
    result = policy.sample(100, true, 16 * MB, false);
    expect(result.level == Level::Warning && result.release);
    result = policy.sample(200, true, 20 * MB, false);
    expect(result.level == Level::Warning && !result.release);
    result = policy.sample(300, true, 8 * MB, false);
    expect(result.level == Level::Critical && result.release);
    for (unsigned t = 301; t < 10000; ++t)
        expect(!policy.sample(t, true, 7 * MB, true).release);
    result = policy.sample(10000, true, 11 * MB, false);
    expect(result.level == Level::Critical && !result.release);
    result = policy.sample(10001, true, 12 * MB, false);
    expect(result.level == Level::Warning && !result.release);
    result = policy.sample(10002, true, 24 * MB, false);
    expect(result.level == Level::Normal && !result.release);
    result = policy.sample(10003, true, 30 * MB, true);
    expect(result.level == Level::Critical && !result.release);
    result = policy.sample(15300, false, 0, false);
    expect(result.level == Level::Critical && result.release);
    expect(!policy.sample(1, false, 0, false).release); // No unsigned clock underflow.
    result = policy.sample(16000, true, 24 * MB, false);
    expect(result.level == Level::Normal && !result.release);
    Policy notificationOnly;
    result = notificationOnly.sample(0, false, 0, true);
    expect(result.level == Level::Critical && result.release);
    expect(!notificationOnly.sample(14999, false, 0, true).release);
    expect(notificationOnly.sample(15000, false, 0, true).release);
    Policy held;
    expect(!held.sample(100, true, 7 * MB, true, false).release);
    expect(held.level() == Level::Critical);
    expect(held.sample(101, true, 7 * MB, false, true).release);
    expect(!held.sample(102, true, 7 * MB, false, true).release);
    std::printf("PASS pressure policy: %u checks (thresholds, hysteresis, 9699-event flood, cooldown, unavailable HAL)\n", checks);
}
