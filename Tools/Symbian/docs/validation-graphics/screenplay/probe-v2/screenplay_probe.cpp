// ScreenPlay normal-API lifecycle canary. Root owns containment and execution.
// No screen capture, native identifier output, layout diagnostics or timing claim.
#include <e32base.h>
#include <fbs.h>
#include <w32std.h>
#include <graphics/surfacemanager.h>
#include <graphics/surfaceupdateclient.h>
#include <graphics/surfaceconfiguration.h>
#include <stdio.h>
#include <stdint.h>
#include <new>

namespace {
constexpr TInt KCycleCount = 2;
constexpr TInt KBufferCount = 2;

const char* Bool(bool value) { return value ? "true" : "false"; }

struct Cycle {
    TInt rc = KErrNotReady;
    TInt createRc = KErrNotReady, infoRc = KErrNotReady, mapRc = KErrNotReady;
    TInt offsetsRc = KErrNotReady, registerRc = KErrNotReady;
    TInt configurationRc = KErrNotReady, setRc = KErrNotReady, getRc = KErrNotReady;
    TInt syncRc = KErrNotReady, submit0Rc = KErrNotReady, submit1Rc = KErrNotReady;
    TInt firstAvailableRc = KErrNotReady, secondAvailableRc = KErrNotReady;
    TInt detachRc = KErrNotReady, finishRc = KErrNotReady, closeSurfaceRc = KErrNotReady;
    bool exactTwoBuffers = false, layoutValid = false, pixelsVerified = false;
    bool backgroundOwned = false, overlayUsesKeyColor = false, firstPendingBeforeSecond = false;
    bool twoBuffersSubmitted = false, availableAfterSecond = false;
    bool detached = false, notificationsDrained = true, mappingClosed = false;
    bool unregistered = false, surfaceClosed = false, timedOut = false;
};

struct Snapshot {
    TInt rc = KErrNotReady;
    TInt wsRc = KErrNotReady, fbsRc = KErrNotReady, screenRc = KErrNotReady;
    TInt groupRc = KErrNotReady, windowRc = KErrNotReady, graphicsRc = KErrNotReady;
    TInt managerRc = KErrNotReady, updateRc = KErrNotReady;
    TInt attempted = 0, completed = 0, submissions = 0;
    bool managerClosed = false, updateClosed = false, windowClosed = false;
    bool groupClosed = false, wsClosed = false, fbsDisconnected = false;
    bool opaqueWindow = false, retainedPendingRequests = false, timedOut = false;
    Cycle cycles[KCycleCount];
};

bool ReadToken(char* token) {
    TBuf<80> command;
    TInt length = User::CommandLineLength();
    if (length < 14 || length > command.MaxLength()) return false;
    User::CommandLine(command);
    command.TrimLeft();
    _LIT(prefix, "--run ");
    if (command.Left(prefix().Length()) != prefix) return false;
    TPtrC value = command.Mid(prefix().Length());
    if (value.Length() < 8 || value.Length() > 64) return false;
    for (TInt i = 0; i < value.Length(); ++i) {
        TUint c = value[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
              || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
        token[i] = static_cast<char>(c);
    }
    token[value.Length()] = 0;
    return true;
}

// The finite timer concerns an ordinary request completion. No clock performance
// or memory-layout information enters the public record.
bool WaitBounded(TRequestStatus& status) {
    if (status.Int() != KRequestPending) return true;
    RTimer timer;
    if (timer.CreateLocal() != KErrNone) return false;
    TRequestStatus deadline;
    timer.After(deadline, TTimeIntervalMicroSeconds32(1500000));
    User::WaitForRequest(status, deadline);
    bool complete = status.Int() != KRequestPending;
    timer.Cancel();
    User::WaitForRequest(deadline);
    timer.Close();
    return complete;
}

class CProbe final : public CBase {
public:
    static void operator delete(TAny* pointer) noexcept { User::Free(pointer); }
    static void operator delete(TAny* pointer, TLeave) noexcept { User::Free(pointer); }
    explicit CProbe(Snapshot& output) : iOut(output) { }
    ~CProbe() override { Close(); }

    void OpenL() {
        iOut.wsRc = iWs.Connect();
        User::LeaveIfError(iOut.wsRc);
        iWsOpen = true;
        iOut.fbsRc = RFbsSession::Connect();
        User::LeaveIfError(iOut.fbsRc);
        iFbsOpen = true;
        iScreen = new (ELeave) CWsScreenDevice(iWs);
        iOut.screenRc = iScreen->Construct(0);
        User::LeaveIfError(iOut.screenRc);
        iGroup = new (std::nothrow) RWindowGroup(iWs);
        if (!iGroup) User::Leave(KErrNoMemory);
        iOut.groupRc = iGroup->Construct(1, EFalse, iScreen);
        User::LeaveIfError(iOut.groupRc);
        iGroupOpen = true;
        iWindow = new (std::nothrow) RWindow(iWs);
        if (!iWindow) User::Leave(KErrNoMemory);
        iOut.windowRc = iWindow->Construct(*iGroup, 2);
        User::LeaveIfError(iOut.windowRc);
        iWindowOpen = true;
        iGroup->SetOrdinalPosition(0);
        User::LeaveIfError(iWindow->SetRequiredDisplayMode(EColor16MA));
        // Keep the new window's default opaque state. The deprecated display
        // mode request does not enable transparency or choose composition mode.
        // GetBackgroundSurface requires that transparency has never been enabled.
        iOut.opaqueWindow = true;
        iWindow->SetBackgroundColor(KRgbWhite);
        iWindow->SetExtent(TPoint(8, 40), TSize(64, 48));
        iOut.graphicsRc = iScreen->CreateContext(iGraphics);
        User::LeaveIfError(iOut.graphicsRc);
        iWindow->SetVisible(ETrue);
        iWindow->Activate();
        iOut.managerRc = iManager.Open();
        User::LeaveIfError(iOut.managerRc);
        iManagerOpen = true;
        iOut.updateRc = iUpdates.Connect();
        User::LeaveIfError(iOut.updateRc);
        iUpdatesOpen = true;
    }

    void RunCycleL(TInt index) {
        Cycle& out = iOut.cycles[index];
        ++iOut.attempted;
        out.rc = KErrNone;
        const TSize size = index == 0 ? TSize(64, 48) : TSize(80, 56);
        iWindow->SetSize(size);

        RSurfaceManager::TSurfaceCreationAttributesBuf attributes;
        attributes().iSize = size;
        attributes().iBuffers = KBufferCount;
        attributes().iPixelFormat = EUidPixelFormatARGB_8888_PRE;
        attributes().iStride = size.iWidth * 4;
        attributes().iAlignment = RSurfaceManager::EPageAligned;
        attributes().iMappable = ETrue;
        attributes().iCacheAttrib = RSurfaceManager::ECached;
        iSurface = TSurfaceId::CreateNullId();
        out.createRc = iManager.CreateSurface(attributes, iSurface);
        User::LeaveIfError(out.createRc);
        iSurfaceOpen = true;
        RSurfaceManager::TInfoBuf information;
        out.infoRc = iManager.SurfaceInfo(iSurface, information);
        User::LeaveIfError(out.infoRc);
        out.exactTwoBuffers = information().iBuffers == KBufferCount;
        if (!out.exactTwoBuffers || information().iSize != size
            || information().iPixelFormat != EUidPixelFormatARGB_8888_PRE
            || !information().iMappable || information().iStride < size.iWidth * 4)
            User::Leave(KErrCorrupt);
        out.mapRc = iManager.MapSurface(iSurface, iMapping);
        User::LeaveIfError(out.mapRc);
        iMappingOpen = true;
        const TInt stride = information().iStride;
        TInt offsets[KBufferCount] = {};
        TInt64 bytes = TInt64(stride) * size.iHeight;
        for (TInt buffer = 0; buffer < KBufferCount; ++buffer) {
            out.offsetsRc = iManager.GetBufferOffset(iSurface, buffer, offsets[buffer]);
            User::LeaveIfError(out.offsetsRc);
            if (offsets[buffer] < 0 || (offsets[buffer] & 3) || (stride & 3)
                || bytes <= 0 || TInt64(offsets[buffer]) + bytes > iMapping.Size())
                User::Leave(KErrCorrupt);
        }
        if (!(TInt64(offsets[0]) + bytes <= offsets[1]
              || TInt64(offsets[1]) + bytes <= offsets[0]))
            User::Leave(KErrCorrupt);
        out.layoutValid = true;
        for (TInt buffer = 0; buffer < KBufferCount; ++buffer) {
            TUint8* base = iMapping.Base() + offsets[buffer];
            const TUint32 color = buffer ? 0xff2878d0 : 0xffd04830;
            for (TInt y = 0; y < size.iHeight; ++y) {
                TUint32* row = reinterpret_cast<TUint32*>(base + y * stride);
                for (TInt x = 0; x < size.iWidth; ++x) row[x] = color;
            }
            for (TInt y = 0; y < size.iHeight; ++y) {
                const TUint32* row = reinterpret_cast<const TUint32*>(base + y * stride);
                for (TInt x = 0; x < size.iWidth; ++x)
                    if (row[x] != color) User::Leave(KErrCorrupt);
            }
        }
        out.pixelsVerified = true;
        out.registerRc = iWs.RegisterSurface(0, iSurface);
        User::LeaveIfError(out.registerRc);
        iRegistered = true;
        TSurfaceConfiguration configuration(iWs.PreferredSurfaceConfigurationSize());
        out.configurationRc = configuration.SetSurfaceId(iSurface);
        if (!out.configurationRc) out.configurationRc = configuration.SetViewport(TRect(size));
        if (!out.configurationRc) out.configurationRc = configuration.SetExtent(TRect(size));
        User::LeaveIfError(out.configurationRc);
        out.setRc = iWindow->SetBackgroundSurface(configuration, ETrue);
        User::LeaveIfError(out.setRc);
        iAttached = true;
        TSurfaceConfiguration actual(iWs.PreferredSurfaceConfigurationSize());
        out.getRc = iWindow->GetBackgroundSurface(actual);
        User::LeaveIfError(out.getRc);
        TSurfaceId observed = TSurfaceId::CreateNullId();
        actual.GetSurfaceId(observed);
        TRect viewport, extent;
        actual.GetViewport(viewport);
        actual.GetExtent(extent);
        out.backgroundOwned = observed == iSurface && viewport == TRect(size) && extent == TRect(size);
        if (!out.backgroundOwned) User::Leave(KErrCorrupt);
        // A surface must already be attached for KeyColor() to report the
        // system's actual chroma/alpha composition key rather than its fallback.
        ClearWindowL();
        out.overlayUsesKeyColor = true;
        out.finishRc = iWs.Finish();
        User::LeaveIfError(out.finishRc);
        for (TInt buffer = 0; buffer < KBufferCount; ++buffer) {
            out.syncRc = iManager.SynchronizeCache(iSurface, buffer, RSurfaceManager::ESyncBeforeNonCPURead);
            User::LeaveIfError(out.syncRc);
            iStatus[buffer] = KRequestPending;
            iStatusUsed[buffer] = true;
            out.notificationsDrained = false;
            iUpdates.NotifyWhenAvailable(iStatus[buffer]);
            TInt error = iUpdates.SubmitUpdate(KAllScreens, iSurface, buffer);
            if (buffer == 0) out.submit0Rc = error; else out.submit1Rc = error;
            User::LeaveIfError(error);
            ++iOut.submissions;
            if (buffer == 0) {
                // This does not wait for Available before submitting the other
                // buffer: a compositor may retain the currently displayed one.
                User::After(TTimeIntervalMicroSeconds32(150000));
                out.firstPendingBeforeSecond = iStatus[0].Int() == KRequestPending;
            }
        }
        out.twoBuffersSubmitted = true;
        if (!WaitBounded(iStatus[0])) {
            out.timedOut = true;
            User::Leave(KErrTimedOut);
        }
        out.firstAvailableRc = iStatus[0].Int();
        User::LeaveIfError(out.firstAvailableRc);
        out.availableAfterSecond = true;
        // Fixed public fixture hold only. No display/vsync/performance claim.
        User::After(TTimeIntervalMicroSeconds32(300000));
    }

    void EndCycle(TInt index) {
        Cycle& out = iOut.cycles[index];
        if (iAttached) {
            TSurfaceConfiguration actual(iWs.PreferredSurfaceConfigurationSize());
            out.detachRc = iWindow->GetBackgroundSurface(actual);
            if (!out.detachRc) {
                TSurfaceId observed = TSurfaceId::CreateNullId();
                actual.GetSurfaceId(observed);
                if (observed == iSurface) {
                    iWindow->RemoveBackgroundSurface(ETrue);
                    out.detached = true;
                } else {
                    out.detachRc = KErrInUse;
                }
            }
            iAttached = false;
            if (out.rc == KErrNone && out.detachRc) out.rc = out.detachRc;
            out.finishRc = iWs.Finish();
            if (out.rc == KErrNone && out.finishRc) out.rc = out.finishRc;
        }
        if (iUpdatesOpen) iUpdates.CancelAllUpdateNotifications();
        out.notificationsDrained = true;
        for (TInt buffer = 0; buffer < KBufferCount; ++buffer) {
            if (!iStatusUsed[buffer]) continue;
            if (!WaitBounded(iStatus[buffer])) {
                out.notificationsDrained = false;
                out.timedOut = true;
                if (out.rc == KErrNone) out.rc = KErrTimedOut;
                continue;
            }
            if (buffer == 0 && out.firstAvailableRc == KErrNotReady)
                out.firstAvailableRc = iStatus[buffer].Int();
            if (buffer == 1) out.secondAvailableRc = iStatus[buffer].Int();
            if (out.rc == KErrNone && iStatus[buffer].Int() != KErrNone
                && iStatus[buffer].Int() != KErrCancel)
                out.rc = iStatus[buffer].Int();
            iStatusUsed[buffer] = false;
        }
        if (!out.notificationsDrained) {
            // Retain request storage and handles until normal process teardown;
            // this is a failed run, never a fake cleanup/completion success.
            iOut.retainedPendingRequests = true;
            iOut.timedOut = true;
            return;
        }
        if (iRegistered) {
            iWs.UnregisterSurface(0, iSurface);
            iRegistered = false;
            out.unregistered = true;
            out.finishRc = iWs.Finish();
            if (out.rc == KErrNone && out.finishRc) out.rc = out.finishRc;
        }
        if (iSurfaceOpen) {
            out.closeSurfaceRc = iManager.CloseSurface(iSurface);
            out.surfaceClosed = out.closeSurfaceRc == KErrNone;
            iSurfaceOpen = false;
            if (out.rc == KErrNone && out.closeSurfaceRc) out.rc = out.closeSurfaceRc;
        }
        if (iMappingOpen) {
            iMapping.Close();
            iMappingOpen = false;
            out.mappingClosed = true;
        }
        if (out.rc == KErrNone) ++iOut.completed;
        iOut.timedOut = iOut.timedOut || out.timedOut;
    }

    void Close() {
        if (iOut.retainedPendingRequests) return;
        if (iUpdatesOpen) { iUpdates.Close(); iUpdatesOpen = false; iOut.updateClosed = true; }
        if (iManagerOpen) { iManager.Close(); iManagerOpen = false; iOut.managerClosed = true; }
        delete iGraphics; iGraphics = nullptr;
        if (iWindowOpen) { iWindow->Close(); iWindowOpen = false; iOut.windowClosed = true; }
        if (iGroupOpen) { iGroup->Close(); iGroupOpen = false; iOut.groupClosed = true; }
        delete iWindow; iWindow = nullptr;
        delete iGroup; iGroup = nullptr;
        delete iScreen; iScreen = nullptr;
        if (iWsOpen) { iWs.Close(); iWsOpen = false; iOut.wsClosed = true; }
        if (iFbsOpen) { RFbsSession::Disconnect(); iFbsOpen = false; iOut.fbsDisconnected = true; }
    }

private:
    void ClearWindowL() {
        const TRgb keyColor = iWindow->KeyColor();
        iWindow->SetBackgroundColor(keyColor);
        iWindow->Invalidate();
        TRequestStatus ready;
        iWs.RedrawReady(&ready);
        if (!WaitBounded(ready)) {
            iWs.RedrawReadyCancel();
            User::WaitForRequest(ready);
            User::Leave(KErrTimedOut);
        }
        User::LeaveIfError(ready.Int());
        TWsRedrawEvent event;
        iWs.GetRedraw(event);
        if (event.Handle() != 2) User::Leave(KErrCorrupt);
        iWindow->BeginRedraw();
        iGraphics->Activate(*iWindow);
        iGraphics->SetDrawMode(CGraphicsContext::EDrawModeWriteAlpha);
        iGraphics->SetBrushStyle(CGraphicsContext::ESolidBrush);
        iGraphics->SetBrushColor(keyColor);
        iGraphics->Clear();
        iGraphics->Deactivate();
        iWindow->EndRedraw();
        User::LeaveIfError(iWs.Finish());
    }

    Snapshot& iOut;
    RWsSession iWs;
    CWsScreenDevice* iScreen = nullptr;
    RWindowGroup* iGroup = nullptr;
    RWindow* iWindow = nullptr;
    CWindowGc* iGraphics = nullptr;
    RSurfaceManager iManager;
    RSurfaceUpdateSession iUpdates;
    TSurfaceId iSurface = TSurfaceId::CreateNullId();
    RChunk iMapping;
    TRequestStatus iStatus[KBufferCount];
    bool iStatusUsed[KBufferCount] = {};
    bool iWsOpen = false, iFbsOpen = false, iGroupOpen = false, iWindowOpen = false;
    bool iManagerOpen = false, iUpdatesOpen = false, iSurfaceOpen = false;
    bool iMappingOpen = false, iRegistered = false, iAttached = false;
};

void PrintCycle(const Cycle& out, const char* token, TInt index) {
    printf("CYCLE {\"suite\":\"screenplay-capability\",\"version\":2,\"run\":\"%s\",\"cycle\":%d,"
           "\"rc\":%d,\"create_rc\":%d,\"info_rc\":%d,\"map_rc\":%d,\"offsets_rc\":%d,"
           "\"register_rc\":%d,\"configuration_rc\":%d,\"set_rc\":%d,\"get_rc\":%d,"
           "\"sync_rc\":%d,\"submit0_rc\":%d,\"submit1_rc\":%d,\"first_available_rc\":%d,"
           "\"second_available_rc\":%d,\"detach_rc\":%d,\"finish_rc\":%d,\"close_surface_rc\":%d,"
           "\"exact_two_buffers\":%s,\"layout_valid\":%s,\"pixels_verified\":%s,\"background_owned\":%s,\"overlay_uses_key_color\":%s,"
           "\"first_pending_before_second\":%s,\"two_buffers_submitted\":%s,\"available_after_second\":%s,"
           "\"detached\":%s,\"notifications_drained\":%s,\"mapping_closed\":%s,\"unregistered\":%s,"
           "\"surface_closed\":%s,\"timed_out\":%s}\n",
           token, index, out.rc, out.createRc, out.infoRc, out.mapRc, out.offsetsRc,
           out.registerRc, out.configurationRc, out.setRc, out.getRc, out.syncRc,
           out.submit0Rc, out.submit1Rc, out.firstAvailableRc, out.secondAvailableRc,
           out.detachRc, out.finishRc, out.closeSurfaceRc, Bool(out.exactTwoBuffers),
           Bool(out.layoutValid), Bool(out.pixelsVerified), Bool(out.backgroundOwned), Bool(out.overlayUsesKeyColor),
           Bool(out.firstPendingBeforeSecond), Bool(out.twoBuffersSubmitted),
           Bool(out.availableAfterSecond), Bool(out.detached), Bool(out.notificationsDrained),
           Bool(out.mappingClosed), Bool(out.unregistered), Bool(out.surfaceClosed), Bool(out.timedOut));
}
}

int main() {
    char token[65] = {};
    if (!ReadToken(token)) return 2;
    if (!freopen("C:/Data/WebKit/screenplay-capability.log", "w", stdout)) return 2;
    printf("BEGIN {\"suite\":\"screenplay-capability\",\"version\":2,\"run\":\"%s\"}\n", token);
    fflush(stdout);
    Snapshot out;
    CTrapCleanup* cleanup = CTrapCleanup::New();
    CProbe* probe = nullptr;
    if (!cleanup) out.rc = KErrNoMemory;
    else {
        TRAPD(error, {
            probe = new (ELeave) CProbe(out);
            probe->OpenL();
        });
        out.rc = error;
        if (!error) {
            for (TInt index = 0; index < KCycleCount; ++index) {
                TRAPD(cycleError, probe->RunCycleL(index));
                if (cycleError) out.cycles[index].rc = cycleError;
                if (cycleError == KErrTimedOut) out.cycles[index].timedOut = true;
                probe->EndCycle(index);
                PrintCycle(out.cycles[index], token, index);
                fflush(stdout);
                if (out.cycles[index].rc) { out.rc = out.cycles[index].rc; break; }
            }
        }
        if (probe) probe->Close();
        if (!out.retainedPendingRequests) delete probe;
        delete cleanup;
    }
    const bool available = out.rc == KErrNone && out.completed == KCycleCount
        && !out.retainedPendingRequests && !out.timedOut;
    printf("RESULT {\"suite\":\"screenplay-capability\",\"version\":2,\"run\":\"%s\",\"status\":\"%s\","
           "\"rc\":%d,\"ws_rc\":%d,\"fbs_rc\":%d,\"screen_rc\":%d,\"group_rc\":%d,"
           "\"window_rc\":%d,\"graphics_rc\":%d,\"manager_rc\":%d,\"update_rc\":%d,"
           "\"cycles_attempted\":%d,\"cycles_completed\":%d,\"submissions\":%d,"
           "\"manager_closed\":%s,\"update_session_closed\":%s,\"window_closed\":%s,"
           "\"window_group_closed\":%s,\"ws_closed\":%s,\"fbs_disconnected\":%s,"
           "\"opaque_window\":%s,\"retained_pending_requests\":%s,\"timed_out\":%s,\"display_verified\":false,"
           "\"vsync_verified\":false,\"performance_verified\":false}\n",
           token, available ? "AVAILABLE" : "UNAVAILABLE", out.rc,
           out.wsRc, out.fbsRc, out.screenRc, out.groupRc, out.windowRc,
           out.graphicsRc, out.managerRc, out.updateRc,
           out.attempted, out.completed, out.submissions,
           Bool(out.managerClosed), Bool(out.updateClosed), Bool(out.windowClosed),
           Bool(out.groupClosed), Bool(out.wsClosed), Bool(out.fbsDisconnected),
           Bool(out.opaqueWindow), Bool(out.retainedPendingRequests), Bool(out.timedOut));
    printf("screenplay-capability exiting 0\n");
    fflush(stdout);
    fclose(stdout);
    return 0; // Normal snapshot completion also includes native UNAVAILABLE.
}
