/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "config.h"
#include "MemoryPressureHandlerSymbian.h"

#include "MemoryPressureMonitorSymbianNative.h"
#include "MemoryPressurePolicy.h"
#include <wtf/FastMalloc.h>
#include <wtf/MainThread.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/MonotonicTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>

namespace WTF {
namespace {

using PressureLevel = SymbianSystem::MemoryPressurePolicy::Level;

class MemoryPressureMonitor {
public:
    MemoryPressureMonitor()
        : m_eventTimer(RunLoop::mainSingleton(), "SymbianMemoryPressure::Event"_s, [this] { sample(); })
        , m_pollTimer(RunLoop::mainSingleton(), "SymbianMemoryPressure::Poll"_s, [this] { sample(); })
    {
    }

    void start()
    {
        ASSERT(isMainThread());
        if (m_running)
            return;
        m_running = true;
        ++m_generation;
        m_policy = { };
        m_holdOffUntil = { };
        subscribe();
        // Observe an already-low system even if the platform's initial notifier
        // completion does not include memory flags.
        m_eventTimer.startOneShot(0_s);
    }

    void stop()
    {
        ASSERT(isMainThread());
        m_running = false;
        ++m_generation;
        m_eventTimer.stop();
        m_pollTimer.stop();
        SymbianSystem::destroyMemoryPressureMonitor(std::exchange(m_native, nullptr));
        m_pendingLowMemory = false;
        m_subscriptionFailed = false;
        MemoryPressureHandler::singleton().setMemoryPressureStatus(SystemMemoryPressureStatus::Normal);
    }

    void holdOff(Seconds duration)
    {
        ASSERT(isMainThread());
        m_holdOffUntil = MonotonicTime::now() + std::max(duration, 0_s);
    }

private:
    void subscribe()
    {
        SymbianSystem::destroyMemoryPressureMonitor(std::exchange(m_native, nullptr));
        int error = 0;
        m_native = SymbianSystem::createMemoryPressureMonitor(nativeNotification, this, error);
        m_subscriptionFailed = !m_native;
        m_lastSubscribe = MonotonicTime::now();
        if (error)
            WTFLogAlways("Symbian memory pressure: native notifier unavailable (%d); system RAM sampling remains active", error);
    }

    static void nativeNotification(void* context, unsigned changes, int error)
    {
        auto& monitor = *static_cast<MemoryPressureMonitor*>(context);
        ASSERT(isMainThread());
        if (!monitor.m_running)
            return;
        if (error) {
            monitor.m_subscriptionFailed = true;
            WTFLogAlways("Symbian memory pressure: native notification failed (%d)", error);
        }
        // EChangesOutOfMemory is intentionally ignored: it reports any system
        // allocation failure, not this process's memory pressure.
        if (!(changes & (SymbianSystem::memoryChangeFree | SymbianSystem::memoryChangeLow)) && !error)
            return;
        monitor.m_pendingLowMemory |= changes & SymbianSystem::memoryChangeLow;
        if (!monitor.m_eventTimer.isActive())
            monitor.m_eventTimer.startOneShot(250_ms);
    }

    void sample()
    {
        ASSERT(isMainThread());
        if (!m_running)
            return;
        uint64_t generation = m_generation;
        auto now = MonotonicTime::now();
        if (m_subscriptionFailed && now - m_lastSubscribe >= 30_s)
            subscribe();
        uint64_t freeRAM = 0;
        bool hasFreeRAM = SymbianSystem::systemFreeMemory(freeRAM);
        bool nativeLow = std::exchange(m_pendingLowMemory, false);
        auto decision = m_policy.sample(static_cast<uint64_t>(now.secondsSinceEpoch().milliseconds()), hasFreeRAM, freeRAM, nativeLow, now >= m_holdOffUntil);
        auto& handler = MemoryPressureHandler::singleton();
        handler.setMemoryPressureStatus(decision.level == PressureLevel::Critical ? SystemMemoryPressureStatus::Critical
            : decision.level == PressureLevel::Warning ? SystemMemoryPressureStatus::Warning : SystemMemoryPressureStatus::Normal);
        // Status observers can synchronously uninstall, or stop then restart,
        // monitoring. An obsolete delivery must not invoke a new owner's handler.
        if (!m_running || generation != m_generation)
            return;
        if (decision.release) {
            WTFLogAlways("Symbian memory pressure: relief level=%u systemFreeRAM=%llu valid=%u nativeLow=%u", static_cast<unsigned>(decision.level), static_cast<unsigned long long>(freeRAM), static_cast<unsigned>(hasFreeRAM), static_cast<unsigned>(nativeLow));
            // Timer delivery is outside native notification and allocator stacks.
            // Live JS objects are retained by normal GC; no process kill or fake
            // footprint threshold is derived from the system-wide RAM sample.
            handler.releaseMemory(decision.level == PressureLevel::Critical ? Critical::Yes : Critical::No, Synchronous::No);
        }
        // Native notifications provide the prompt path. A slow independent HAL
        // sample also detects a missed event and observes recovery hysteresis.
        if (m_running && generation == m_generation)
            m_pollTimer.startOneShot(decision.level == PressureLevel::Normal ? 30_s : 5_s);
    }

    RunLoop::Timer m_eventTimer;
    RunLoop::Timer m_pollTimer;
    SymbianSystem::MemoryPressurePolicy m_policy;
    MonotonicTime m_lastSubscribe;
    MonotonicTime m_holdOffUntil;
    void* m_native { nullptr };
    uint64_t m_generation { 0 };
    bool m_running { false };
    bool m_pendingLowMemory { false };
    bool m_subscriptionFailed { false };
};

MemoryPressureMonitor& monitor()
{
    static NeverDestroyed<MemoryPressureMonitor> monitor;
    return monitor;
}

} // namespace

void MemoryPressureHandler::install() { monitor().start(); }
void MemoryPressureHandler::uninstall() { monitor().stop(); }
void MemoryPressureHandler::holdOff(Seconds duration) { monitor().holdOff(duration); }
void MemoryPressureHandler::respondToMemoryPressure(Critical critical, Synchronous synchronous) { releaseMemory(critical, synchronous); }
void MemoryPressureHandler::platformReleaseMemory(Critical) { releaseFastMallocFreeMemory(); }

std::optional<MemoryPressureHandler::ReliefLogger::MemoryUsage> MemoryPressureHandler::ReliefLogger::platformMemoryUsage()
{
    // HAL reports system free RAM, not this process's resident/physical footprint.
    return std::nullopt;
}

void stopMemoryPressureMonitoring() { monitor().stop(); }

} // namespace WTF
