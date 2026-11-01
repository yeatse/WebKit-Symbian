/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

// Compile this native boundary separately, with Symbian native leaves enabled.
#include "NetworkStateNotifierSymbianNative.h"

#include <e32base.h>
#include <nifvar.h>
#include <rconnmon.h>

namespace WebCore::SymbianNetworkState {
namespace {

constexpr TInt requestTimeoutMicroseconds = 5000000;
constexpr TInt retryMicroseconds = 5000000;
constexpr TInt resyncMicroseconds = 30000000;
constexpr TUint maximumConnections = 1024;

class Observer final : public CActive, public MConnectionMonitorObserver {
public:
    // CBase allocates on User's heap. Its locally emitted deleting destructor
    // must not call the executable's portable/mimalloc global delete.
    static void operator delete(TAny* pointer) noexcept { User::Free(pointer); }
    static void operator delete(TAny* pointer, TLeave) noexcept { User::Free(pointer); }

    Observer(ChangeCallback callback, void* context)
        : CActive(EPriorityStandard)
        , m_callback(callback)
        , m_context(context)
    {
        CActiveScheduler::Add(this);
    }

    ~Observer() override
    {
        m_stopping = true;
        m_callback = nullptr;
        delete m_timer;
        m_timer = nullptr;
        disconnect();
    }

    void initializeL()
    {
        m_timer = new (ELeave) RefreshTimer(*this);
        m_timer->initializeL();
    }

    void refresh()
    {
        if (m_stopping)
            return;
        if (IsActive()) {
            m_changedDuringSnapshot = true;
            return;
        }
        if (!m_connected) {
            TRAPD(error, connectL());
            if (error != KErrNone) {
                failed();
                return;
            }
        }
        m_changedDuringSnapshot = false;
        m_sawConnected = false;
        m_incomplete = false;
        m_count = 0;
        m_index = 0;
        m_request = EConnMonGetConnectionCount;
        m_monitor.GetConnectionCount(m_count, iStatus);
        SetActive();
        m_timer->schedule(requestTimeoutMicroseconds);
    }

    bool state(bool& online) const
    {
        if (!m_known)
            return false;
        online = m_online;
        return true;
    }

private:
    class RefreshTimer final : public CTimer {
    public:
        static void operator delete(TAny* pointer) noexcept { User::Free(pointer); }
        static void operator delete(TAny* pointer, TLeave) noexcept { User::Free(pointer); }

        explicit RefreshTimer(Observer& observer)
            : CTimer(EPriorityStandard)
            , m_observer(observer)
        {
            CActiveScheduler::Add(this);
        }

        void initializeL() { ConstructL(); }
        void schedule(TInt delay)
        {
            Cancel();
            After(TTimeIntervalMicroSeconds32(delay));
        }

    private:
        void RunL() override { m_observer.timerFired(); }
        Observer& m_observer;
    };

    void connectL()
    {
        User::LeaveIfError(m_monitor.ConnectL());
        m_connected = true;
        // Subscribe before enumerating so a connection handover invalidates
        // the in-flight snapshot instead of publishing an old offline result.
        User::LeaveIfError(m_monitor.NotifyEventL(*this));
    }

    void disconnect()
    {
        if (m_connected)
            m_monitor.CancelNotifications();
        Cancel();
        m_monitor.Close();
        m_connected = false;
    }

    void failed()
    {
        // API errors are not evidence that the network is offline. Reconnect
        // and retry while retaining the last complete, trustworthy snapshot.
        disconnect();
        if (!m_stopping)
            m_timer->schedule(retryMicroseconds);
    }

    void timerFired()
    {
        if (IsActive()) {
            // A lost server completion must not leave the observer stuck.
            failed();
            return;
        }
        refresh();
    }

    void EventL(const CConnMonEventBase& event) override
    {
        switch (event.EventType()) {
        case EConnMonCreateConnection:
        case EConnMonDeleteConnection:
        case EConnMonConnectionStatusChange:
        case EConnMonNetworkStatusChange:
        case EConnMonBearerChange:
            refresh();
            break;
        default:
            break;
        }
    }

    void RunL() override
    {
        m_timer->Cancel();
        if (iStatus.Int() != KErrNone) {
            failed();
            return;
        }
        if (m_request == EConnMonGetConnectionCount) {
            if (m_count > maximumConnections) {
                failed();
                return;
            }
        } else {
            // These are the public nifvar.h connected stages. Transitional,
            // suspended or bearer-specific stages are unknown, not offline.
            switch (m_connectionStatus) {
            case KConnectionOpen:
            case KLinkLayerOpen:
                m_sawConnected = true;
                break;
            case KConnectionUninitialised:
            case KConnectionFailure:
            case KConnectionClosed:
            case KLinkLayerClosed:
                break;
            default:
                m_incomplete = true;
                break;
            }
        }
        nextConnection();
    }

    void nextConnection()
    {
        while (++m_index <= m_count) {
            TUint connection = 0;
            TUint subconnections = 0;
            if (m_monitor.GetConnectionInfo(m_index, connection, subconnections) != KErrNone) {
                m_incomplete = true;
                continue;
            }
            m_request = EConnMonGetIntAttribute;
            m_connectionStatus = KConnectionUninitialised;
            m_monitor.GetIntAttribute(connection, 0, KConnectionStatus, m_connectionStatus, iStatus);
            SetActive();
            m_timer->schedule(requestTimeoutMicroseconds);
            return;
        }
        finishSnapshot();
    }

    void finishSnapshot()
    {
        if (m_changedDuringSnapshot) {
            refresh();
            return;
        }
        m_timer->schedule(m_incomplete ? retryMicroseconds : resyncMicroseconds);
        if (!m_sawConnected && m_incomplete)
            return;
        if (m_known && m_online == m_sawConnected)
            return;
        m_online = m_sawConnected;
        m_known = true;
        // This must remain the final operation: a client may destroy us from
        // its callback. WebCore only queues work on its main run loop here.
        if (m_callback)
            m_callback(m_context);
    }

    void DoCancel() override
    {
        m_monitor.CancelAsyncRequest(m_request);
    }

    TInt RunError(TInt) override
    {
        failed();
        return KErrNone;
    }

    RConnectionMonitor m_monitor;
    RefreshTimer* m_timer { nullptr };
    ChangeCallback m_callback;
    void* m_context;
    TInt m_request { EConnMonGetConnectionCount };
    TInt m_connectionStatus { KConnectionUninitialised };
    TUint m_count { 0 };
    TUint m_index { 0 };
    bool m_connected { false };
    bool m_stopping { false };
    bool m_changedDuringSnapshot { false };
    bool m_sawConnected { false };
    bool m_incomplete { false };
    bool m_known { false };
    bool m_online { false };
};

} // namespace

void* createObserver(ChangeCallback callback, void* context)
{
    if (!CActiveScheduler::Current())
        return nullptr;
    Observer* observer = nullptr;
    TRAPD(error, observer = new (ELeave) Observer(callback, context); observer->initializeL());
    if (error != KErrNone) {
        delete observer;
        return nullptr;
    }
    observer->refresh();
    return observer;
}

void destroyObserver(void* observer)
{
    delete static_cast<Observer*>(observer);
}

bool state(void* observer, bool& online)
{
    return observer && static_cast<Observer*>(observer)->state(online);
}

} // namespace WebCore::SymbianNetworkState
