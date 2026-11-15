// The probe's native window, kept apart from the Skia headers.
#include "window.h"
#include <e32base.h>
#include <fbs.h>
#include <w32std.h>

static RWsSession session;
static RWindowGroup* group;
static RWindow* native;
static CWsScreenDevice* screen;
static TRequestStatus event, redraw;
static bool requestsOpen;

void* probeOpenWindow(int* width, int* height)
{
    if (session.Connect() != KErrNone || RFbsSession::Connect() != KErrNone)
        return nullptr;
    screen = new CWsScreenDevice(session);
    if (!screen || screen->Construct() != KErrNone)
        return nullptr;
    const TSize size = screen->SizeInPixels();
    *width = size.iWidth;
    *height = size.iHeight;
    group = new RWindowGroup(session);
    if (!group || group->Construct(1, ETrue) != KErrNone)
        return nullptr;
    native = new RWindow(session);
    if (!native || native->Construct(*group, 2) != KErrNone)
        return nullptr;
    group->SetOrdinalPosition(0);
    native->SetRequiredDisplayMode(EColor16MA);
    native->SetExtent(TPoint(0, 0), size);
    native->SetVisible(ETrue);
    native->Activate();
    session.EventReady(&event);
    session.RedrawReady(&redraw);
    requestsOpen = true;
    session.Flush();
    return native;
}

void probePumpWindow(unsigned milliseconds)
{
    for (unsigned elapsed = 0; elapsed < milliseconds; elapsed += 20) {
        if (requestsOpen && event != KRequestPending) {
            TWsEvent value;
            session.GetEvent(value);
            session.EventReady(&event);
        }
        if (requestsOpen && redraw != KRequestPending) {
            TWsRedrawEvent value;
            session.GetRedraw(value);
            if (value.Handle() == 2) {
                native->BeginRedraw(value.Rect());
                native->EndRedraw();
            }
            session.RedrawReady(&redraw);
        }
        session.Flush();
        User::After(20000);
    }
}

void probeCloseWindow()
{
    if (requestsOpen) {
        session.EventReadyCancel();
        session.RedrawReadyCancel();
        User::WaitForRequest(event);
        User::WaitForRequest(redraw);
        requestsOpen = false;
    }
    if (native)
        native->Close();
    if (group)
        group->Close();
    delete native;
    delete group;
    delete screen;
    native = nullptr;
    group = nullptr;
    screen = nullptr;
    session.Close();
    RFbsSession::Disconnect();
}
