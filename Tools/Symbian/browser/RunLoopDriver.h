// Runs the engine's timers and tasks from the UI thread's active scheduler.
#pragma once

#include <e32base.h>

class CRunLoopDriver : public CActive {
public:
    static CRunLoopDriver* NewL();
    ~CRunLoopDriver();

private:
    class CWorkTimer : public CTimer {
    public:
        explicit CWorkTimer(CRunLoopDriver& aDriver);
        void ConstructL() { CTimer::ConstructL(); }

    private:
        void RunL();
        CRunLoopDriver& iDriver;
    };

    CRunLoopDriver();
    void ConstructL();
    void RunL();
    void DoCancel();
    void WaitForWakeUp();
    void RunWork();

    static void WakeUp(void* aContext);

    RThread iThread;
    CWorkTimer* iTimer { nullptr };
    // Set by the first WakeUp() after the request is pending; only that one
    // completes it, so the request is never completed twice.
    volatile TInt iSignalled { 0 };
};
