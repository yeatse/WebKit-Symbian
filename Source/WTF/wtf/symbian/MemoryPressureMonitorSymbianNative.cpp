/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

// Keep native descriptors and CActive declarations separate from WTF/libc++.
#include "MemoryPressureMonitorSymbianNative.h"

#include <e32base.h>
#include <hal.h>

namespace WTF::SymbianSystem {

static_assert(memoryChangeFree == EChangesFreeMemory);
static_assert(memoryChangeLow == EChangesLowMemory);

class CMemoryPressureMonitor final : public CActive {
public:
    CMemoryPressureMonitor(MemoryPressureNotification notification, void* context)
        : CActive(EPriorityStandard)
        , m_notification(notification)
        , m_context(context)
    {
        CActiveScheduler::Add(this);
    }

    ~CMemoryPressureMonitor() override
    {
        Cancel();
        m_notifier.Close();
    }

    // CBase's non-leaving new uses User::AllocZ, not the C++ runtime allocator.
    static void operator delete(void* pointer) noexcept { User::Free(pointer); }
    static void operator delete(void* pointer, unsigned) noexcept { User::Free(pointer); }

    int start()
    {
        TInt error = m_notifier.Create();
        return error == KErrNone ? arm() : error;
    }

private:
    int arm()
    {
        TInt error = m_notifier.Logon(iStatus);
        if (error == KErrNone)
            SetActive();
        return error;
    }

    void RunL() override
    {
        TInt result = iStatus.Int();
        if (result < 0) {
            m_notification(m_context, 0, result);
            return;
        }
        // Arm before handing off the completed notification. The callback only
        // schedules work; it cannot destroy this active object synchronously.
        TInt error = arm();
        m_notification(m_context, static_cast<unsigned>(result), error);
    }

    void DoCancel() override { m_notifier.LogonCancel(); }

    RChangeNotifier m_notifier;
    MemoryPressureNotification m_notification;
    void* m_context;
};

void* createMemoryPressureMonitor(MemoryPressureNotification notification, void* context, int& error)
{
    if (!notification || !CActiveScheduler::Current()) {
        error = KErrNotReady;
        return nullptr;
    }
    auto* monitor = new CMemoryPressureMonitor(notification, context);
    if (!monitor) {
        error = KErrNoMemory;
        return nullptr;
    }
    error = monitor->start();
    if (error != KErrNone) {
        delete monitor;
        return nullptr;
    }
    return monitor;
}

void destroyMemoryPressureMonitor(void* monitor)
{
    delete static_cast<CMemoryPressureMonitor*>(monitor);
}

bool systemFreeMemory(uint64_t& bytes)
{
    TInt value = 0;
    if (HAL::Get(HALData::EMemoryRAMFree, value) != KErrNone || value < 0)
        return false;
    bytes = static_cast<uint64_t>(value);
    return true;
}

} // namespace WTF::SymbianSystem
