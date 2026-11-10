#include "RunLoopDriver.h"

#include <WebKitSymbian.h>

using WebKitSymbian::WebView;

CRunLoopDriver* CRunLoopDriver::NewL()
{
    CRunLoopDriver* self = new (ELeave) CRunLoopDriver;
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop(self);
    return self;
}

CRunLoopDriver::CRunLoopDriver()
    : CActive(EPriorityStandard)
{
}

void CRunLoopDriver::ConstructL()
{
    // A process-owned handle, so the engine's other threads can complete
    // requests of this thread.
    User::LeaveIfError(iThread.Open(RThread().Id(), EOwnerProcess));
    iTimer = new (ELeave) CWorkTimer(*this);
    iTimer->ConstructL();
    CActiveScheduler::Add(this);
    WaitForWakeUp();
    WebView::setWakeUpCallback(WakeUp, this);
    RunWork();
}

CRunLoopDriver::~CRunLoopDriver()
{
    WebView::setWakeUpCallback(nullptr, nullptr);
    Cancel();
    delete iTimer;
    iThread.Close();
}

void CRunLoopDriver::WaitForWakeUp()
{
    iStatus = KRequestPending;
    SetActive();
    __atomic_store_n(&iSignalled, 0, __ATOMIC_SEQ_CST);
}

void CRunLoopDriver::WakeUp(void* aContext)
{
    CRunLoopDriver* self = static_cast<CRunLoopDriver*>(aContext);
    if (__atomic_exchange_n(&self->iSignalled, 1, __ATOMIC_SEQ_CST))
        return;
    TRequestStatus* status = &self->iStatus;
    self->iThread.RequestComplete(status, KErrNone);
}

void CRunLoopDriver::RunL()
{
    WaitForWakeUp();
    RunWork();
}

void CRunLoopDriver::DoCancel()
{
    // Complete our own pending request so that Cancel() does not wait forever.
    if (!__atomic_exchange_n(&iSignalled, 1, __ATOMIC_SEQ_CST)) {
        TRequestStatus* status = &iStatus;
        User::RequestComplete(status, KErrCancel);
    }
}

void CRunLoopDriver::RunWork()
{
    WebView::runPendingWork();
    iTimer->Cancel();
    int64_t delay = WebView::microsecondsUntilNextTimer();
    if (delay < 0)
        return;
    // CTimer::After takes 32-bit microseconds; a later timer is rearmed then.
    iTimer->After(TTimeIntervalMicroSeconds32(delay > KMaxTInt ? KMaxTInt : static_cast<TInt>(delay)));
}

CRunLoopDriver::CWorkTimer::CWorkTimer(CRunLoopDriver& aDriver)
    : CTimer(EPriorityStandard)
    , iDriver(aDriver)
{
    CActiveScheduler::Add(this);
}

void CRunLoopDriver::CWorkTimer::RunL()
{
    iDriver.RunWork();
}
