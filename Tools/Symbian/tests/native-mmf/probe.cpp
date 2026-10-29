// Native Symbian MMF probe: public SDK APIs, real window, bounded async stages.
#include "CommandLine.h"
#include <e32base.h>
#include <e32debug.h>
#include <hal.h>
#include <hal_data.h>
#include <fbs.h>
#include <w32std.h>
#include <videoplayer2.h>
#include <graphics/surface.h>
#include <surfaceeventhandler.h>
#include <mdaaudiosampleplayer.h>
#include <mmf/common/mmfstandardcustomcommands.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static FILE* output;
static TUint epochTick;
static TInt tickPeriodMicroseconds;
static unsigned checks, failures;

static TInt64 elapsedMilliseconds()
{
    return static_cast<TInt64>(static_cast<TUint>(User::TickCount() - epochTick)) * tickPeriodMicroseconds / 1000;
}

static void report(const char* format, ...)
{
    char message[1800];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    char line[1900];
    snprintf(line, sizeof(line), "MMF t=%lld %s", static_cast<long long>(elapsedMilliseconds()), message);
    TUint16 units[512];
    unsigned length = 0;
    while (line[length] && length < 511) {
        units[length] = static_cast<unsigned char>(line[length]);
        ++length;
    }
    RDebug::RawPrint(TPtrC16(units, length));
    if (output) {
        fprintf(output, "%s\n", line);
        fflush(output);
    }
}

static bool expect(bool condition, const char* message)
{
    ++checks;
    if (!condition) ++failures;
    report("%s %s", condition ? "PASS" : "FAIL", message);
    return condition;
}

struct Options {
    bool video = false;
    bool url = false;
    bool resourceNotifications = false;
    bool surfaceEvents = false;
    const char* source = nullptr;
    TInt holdMilliseconds = 10000;
    TInt timeoutMilliseconds = 30000;
    TInt cancelOpenMilliseconds = -1;
    TInt expectedOpenError = 0;
    bool anyOpenError = false;
};

class Window;
class WindowRequest final : public CActive {
public:
    WindowRequest(Window& window, bool redraw) : CActive(EPriorityStandard), owner(window), redraw(redraw) { CActiveScheduler::Add(this); }
    ~WindowRequest() { Cancel(); }
    void start();
private:
    void RunL() override;
    void DoCancel() override;
    Window& owner;
    bool redraw;
};

class Window {
public:
    void openL()
    {
        User::LeaveIfError(session.Connect()); connected = true;
        User::LeaveIfError(RFbsSession::Connect()); fbsConnected = true;
        screen = new (ELeave) CWsScreenDevice(session);
        User::LeaveIfError(screen->Construct());
        group = new (ELeave) RWindowGroup(session);
        User::LeaveIfError(group->Construct(1, ETrue)); groupOpen = true;
        native = new (ELeave) RWindow(session);
        User::LeaveIfError(native->Construct(*group, 2)); windowOpen = true;
        TSize size = screen->SizeInPixels();
        if (size.iWidth > 320) size.iWidth = 320;
        if (size.iHeight > 360) size.iHeight = 360;
        extent = TRect(TPoint(8, 40), TSize(size.iWidth - 16, size.iHeight - 48));
        group->SetOrdinalPosition(0);
        native->SetRequiredDisplayMode(EColor16MA);
        native->SetExtent(TPoint(0, 0), size);
        User::LeaveIfError(screen->CreateContext(graphics));
        native->SetVisible(ETrue); native->Activate();
        events = new (ELeave) WindowRequest(*this, false);
        redraws = new (ELeave) WindowRequest(*this, true);
        events->start(); redraws->start();
        native->Invalidate();
        session.Flush();
        report("WINDOW visible width=%d height=%d videoRect=%d,%d,%d,%d", size.iWidth, size.iHeight,
            extent.iTl.iX, extent.iTl.iY, extent.Width(), extent.Height());
    }

    void draw()
    {
        native->BeginRedraw();
        graphics->Activate(*native);
        graphics->SetBrushStyle(CGraphicsContext::ESolidBrush);
        graphics->SetBrushColor(TRgb(32, 40, 64));
        graphics->Clear();
        graphics->SetPenColor(TRgb(255, 255, 255));
        graphics->DrawRect(extent);
        graphics->Deactivate();
        native->EndRedraw();
        session.Flush();
    }

    ~Window()
    {
        delete events; delete redraws; delete graphics;
        if (windowOpen) native->Close();
        if (groupOpen) group->Close();
        delete native; delete group; delete screen;
        if (connected) session.Close();
        if (fbsConnected) RFbsSession::Disconnect();
    }

    RWsSession session;
    CWsScreenDevice* screen = nullptr;
    RWindow* native = nullptr;
    TRect extent;
private:
    RWindowGroup* group = nullptr;
    CWindowGc* graphics = nullptr;
    WindowRequest* events = nullptr;
    WindowRequest* redraws = nullptr;
    bool connected = false, fbsConnected = false, groupOpen = false, windowOpen = false;
};

void WindowRequest::start()
{
    if (redraw) owner.session.RedrawReady(&iStatus);
    else owner.session.EventReady(&iStatus);
    SetActive();
}
void WindowRequest::RunL()
{
    if (iStatus.Int() != KErrNone) { report("WINDOW request error=%d", iStatus.Int()); return; }
    if (redraw) {
        TWsRedrawEvent event;
        owner.session.GetRedraw(event);
        if (event.Handle() == 2) owner.draw();
    } else {
        TWsEvent event;
        owner.session.GetEvent(event);
        report("WINDOW event=%d", event.Type());
    }
    start();
}
void WindowRequest::DoCancel()
{
    if (redraw) owner.session.RedrawReadyCancel();
    else owner.session.EventReadyCancel();
}

class Probe final : public CBase, public MVideoPlayerUtilityObserver,
    public MMdaAudioPlayerCallback, public MMMFAudioResourceNotificationCallback, public MMMFSurfaceEventHandler {
public:
    explicit Probe(const Options& options) : options(options) { }
    void startL()
    {
        window.openL();
        source = HBufC::NewL(strlen(options.source));
        TPtr descriptor = source->Des();
        descriptor.Copy(TPtrC8(reinterpret_cast<const TUint8*>(options.source)));
        if (!options.url) {
            for (TInt i = 0; i < descriptor.Length(); ++i) if (descriptor[i] == '/') descriptor[i] = '\\';
        }
        timer = CPeriodic::NewL(CActive::EPriorityHigh);
        timer->Start(100000, 100000, TCallBack(&Probe::tickCallback, this));
        if (options.video)
            video = CVideoPlayerUtility2::NewL(*this, EMdaPriorityNormal, EMdaPriorityPreferenceTimeAndQuality);
        else
            audio = CMdaAudioPlayerUtility::NewL(*this, EMdaPriorityNormal, EMdaPriorityPreferenceTimeAndQuality);
        report("OPEN call kind=%s source=%s", options.url ? "url" : "file", options.source);
        openStart = elapsedMilliseconds();
        openOnStack = true;
        TRAPD(error, {
            if (options.video) {
                if (options.url) video->OpenUrlL(*source);
                else video->OpenFileL(*source);
            } else {
                if (options.url) audio->OpenUrlL(*source);
                else audio->OpenFileL(*source);
            }
        });
        openOnStack = false;
        report("OPEN returned leave=%d", error);
        if (error) { opened(error); return; }
        if (!finished && options.cancelOpenMilliseconds == 0) cancelOpen();
    }

    ~Probe()
    {
        delete timer;
        if (video) {
            if (surfaceDisplayAdded) video->RemoveDisplay(0);
            if (displayAdded) video->RemoveDisplayWindow(*window.native);
            video->Close();
        }
        if (audio) audio->Close();
        delete video; delete audio; delete source;
    }

    bool done() const { return finished; }
    void run()
    {
        if (finished) return;
        schedulerRunning = true;
        CActiveScheduler::Start();
        schedulerRunning = false;
    }
    void summarize() const
    {
        report("SURFACE counts created=%u changed=%u removed=%u live=%d", surfacesCreated, surfacesChanged, surfacesRemoved, !liveSurface.IsNull());
        report("RESULT {\"probe\":\"native-mmf\",\"version\":1,\"mode\":\"%s\",\"transport\":\"%s\",\"passed\":%u,\"failed\":%u,\"openCallbacks\":%u,\"prepareCallbacks\":%u,\"playCallbacks\":%u,\"resourceCallbacks\":%u,\"positionAdvances\":%u,\"cancelled\":%s}",
            options.video ? "video" : "audio", options.url ? "url" : "file", checks - failures, failures,
            openCallbacks, prepareCallbacks, playCallbacks, resourceCallbacks, positionAdvances, cancelled ? "true" : "false");
    }

private:
    static TInt tickCallback(TAny* object)
    {
        auto& probe = *static_cast<Probe*>(object);
        TRAPD(error, probe.tickL());
        if (error) { report("TIMER leave=%d", error); probe.fail("timer action left"); }
        return !probe.finished;
    }
    void finish()
    {
        if (finished) return;
        finished = true;
        if (resourceRegistered) {
            TInt error = options.video ? video->CancelRegisterAudioResourceNotification(KMMFEventCategoryAudioResourceAvailable)
                                       : audio->CancelRegisterAudioResourceNotification(KMMFEventCategoryAudioResourceAvailable);
            report("RESOURCE unregister error=%d", error);
            expect(error == KErrNone, "audio resource notification cancellation");
            resourceRegistered = false;
        }
        if (schedulerRunning) CActiveScheduler::Stop();
    }
    void fail(const char* reason) { expect(false, reason); finish(); }
    bool callbackAllowed(const char* name)
    {
        report("CALLBACK %s openOnStack=%d cancelled=%d", name, openOnStack, cancelled);
        if (openOnStack) { fail("open callback was synchronous"); return false; }
        if (cancelled) { fail("callback arrived after Close cancelled open"); return false; }
        if (finished) { expect(false, "callback arrived after terminal result"); return false; }
        return true;
    }
    void opened(TInt error)
    {
        report("OPEN complete error=%d", error);
        if (options.expectedOpenError || options.anyOpenError) {
            expect(options.anyOpenError ? error != KErrNone : error == options.expectedOpenError, "expected open error observed");
            finish(); return;
        }
        if (error) { fail("open failed"); return; }
        if (options.cancelOpenMilliseconds >= 0) { fail("open completed before cancellation deadline"); return; }
        expect(true, "open succeeded");
        if (options.video) {
            TRAPD(displayError, video->AddDisplayWindowL(window.session, *window.screen, *window.native, window.extent, window.extent));
            report("DISPLAY AddDisplayWindowL error=%d", displayError);
            if (displayError) { fail("video display window registration"); return; }
            displayAdded = true;
            if (options.surfaceEvents) {
                TRAPD(surfaceError, video->AddDisplayL(window.session, 0, *this));
                report("DISPLAY AddDisplayL error=%d", surfaceError);
                if (surfaceError) { fail("surface observer registration"); return; }
                surfaceDisplayAdded = true;
            }
            report("PREPARE call");
            video->Prepare();
        } else {
            report("PREPARE audio initialization completed with MapcInitComplete");
            TRAPD(readyError, readyL());
            if (readyError) { report("READY leave=%d", readyError); fail("audio ready action left"); }
        }
    }
    void readyL()
    {
        if (options.resourceNotifications) {
            TInt error = options.video
                ? video->RegisterAudioResourceNotification(*this, KMMFEventCategoryAudioResourceAvailable)
                : audio->RegisterAudioResourceNotification(*this, KMMFEventCategoryAudioResourceAvailable);
            report("RESOURCE register error=%d", error);
            resourceRegistered = error == KErrNone;
            expect(resourceRegistered, "audio resource notification registration");
        }
        duration = options.video ? video->DurationL().Int64() : audio->Duration().Int64();
        report("READY duration-us=%lld", static_cast<long long>(duration));
        if (!expect(duration >= 6000000, "fixture has at least six seconds for playback controls")) { finish(); return; }
        if (options.video) {
            const TInt maximum = video->MaxVolume();
            video->SetVolumeL(maximum * 3 / 4);
            report("VOLUME max=%d set=%d", maximum, maximum * 3 / 4);
        } else {
            const TInt maximum = audio->MaxVolume();
            TInt error = audio->SetVolume(maximum * 3 / 4);
            report("VOLUME max=%d set=%d error=%d", maximum, maximum * 3 / 4, error);
            User::LeaveIfError(error);
        }
        playStart = elapsedMilliseconds();
        stageStart = playStart;
        stage = 1;
        initialPosition = positionL("before-play");
        report("PLAY call initial");
        if (options.video) video->Play(); else audio->Play();
    }
    TInt64 positionL(const char* label)
    {
        TTimeIntervalMicroSeconds position(0);
        if (options.video) position = video->PositionL();
        else User::LeaveIfError(audio->GetPosition(position));
        report("POSITION %s us=%lld", label, static_cast<long long>(position.Int64()));
        return position.Int64();
    }
    void cancelOpen()
    {
        cancelled = true;
        cancelTime = elapsedMilliseconds();
        report("CLOSE cancel-open");
        if (options.video) video->Close(); else audio->Close();
        report("CLOSE returned; observing callbacks for 1500 ms");
    }
    void tickL()
    {
        if (finished) return;
        const TInt64 now = elapsedMilliseconds();
        if (now >= options.timeoutMilliseconds) { fail("strict probe timeout expired"); return; }
        if (cancelled) {
            if (now - cancelTime >= 1500) { expect(true, "Close cancellation produced no late callback"); finish(); }
            return;
        }
        if (options.cancelOpenMilliseconds >= 0 && now - openStart >= options.cancelOpenMilliseconds) { cancelOpen(); return; }
        if (!stage) return;
        if (stage == 1 && now - stageStart >= 1500) {
            const TInt64 position = positionL("playing");
            if (expect(position > initialPosition + 250000, "position advanced while playing")) ++positionAdvances;
            report("PAUSE call");
            if (options.video) video->PauseL(); else User::LeaveIfError(audio->Pause());
            pausedPosition = positionL("paused-start");
            stage = 2; stageStart = now;
        } else if (stage == 2 && now - stageStart >= 600) {
            const TInt64 position = positionL("paused-end");
            expect(llabs(position - pausedPosition) <= 100000, "position stayed stable while paused");
            if (options.surfaceEvents) {
                TRect crop;
                video->GetCropRegionL(crop);
                expect(crop.Width() > 4 && crop.Height() > 4, "surface crop has usable dimensions");
                crop.Shrink(1, 1);
                report("SURFACE SetCropRegionL rect=%d,%d,%d,%d", crop.iTl.iX, crop.iTl.iY, crop.Width(), crop.Height());
                video->SetCropRegionL(crop);
            }
            seekPosition = duration / 2;
            if (seekPosition > 3000000) seekPosition = 3000000;
            report("SEEK call target-us=%lld", static_cast<long long>(seekPosition));
            if (options.video) video->SetPositionL(TTimeIntervalMicroSeconds(seekPosition));
            else audio->SetPosition(TTimeIntervalMicroSeconds(seekPosition));
            const TInt64 actual = positionL("after-seek");
            expect(llabs(actual - seekPosition) <= 500000, "seek position is within 500 ms");
            report("PLAY call resume");
            if (options.video) video->Play(); else audio->Play();
            stage = 3; stageStart = now;
        } else if (stage == 3 && now - stageStart >= 1000) {
            const TInt64 position = positionL("resumed");
            if (expect(position > seekPosition + 250000, "position advanced after resume")) ++positionAdvances;
            stage = 4; stageStart = now;
        } else if (stage == 4 && now - stageStart >= 1000) {
            positionL("capture-hold"); stageStart = now;
        }
        if (stage == 5 && now - stageStart >= 1000) {
            expect(surfacesCreated > 0, "surface creation callback observed");
            expect(surfacesChanged > 0, "surface crop change callback observed");
            expect(surfacesRemoved > 0 && liveSurface.IsNull(), "Close removed the observed surface");
            video->RemoveDisplay(0);
            surfaceDisplayAdded = false;
            finish();
        }
        if (stage == 4 && now - playStart >= options.holdMilliseconds) {
            report("STOP call");
            TInt error = KErrNone;
            if (options.video) error = video->Stop(); else audio->Stop();
            report("STOP returned error=%d", error);
            expect(error == KErrNone, "stop succeeded");
            expect(positionAdvances == 2, "both playback intervals advanced");
            if (options.surfaceEvents) {
                stage = 5; stageStart = now;
                report("SURFACE Close call; observing removal for 1000 ms");
                video->Close();
            } else finish();
        }
    }

    void MvpuoOpenComplete(TInt error) override { ++openCallbacks; if (callbackAllowed("MvpuoOpenComplete")) opened(error); }
    void MapcInitComplete(TInt error, const TTimeIntervalMicroSeconds& clipDuration) override
    {
        ++openCallbacks;
        report("AUDIO init duration-us=%lld", static_cast<long long>(clipDuration.Int64()));
        if (callbackAllowed("MapcInitComplete")) opened(error);
    }
    void MvpuoPrepareComplete(TInt error) override
    {
        ++prepareCallbacks;
        if (!callbackAllowed("MvpuoPrepareComplete")) return;
        report("PREPARE complete error=%d", error);
        if (error) { fail("prepare failed"); return; }
        TRAPD(readyError, readyL());
        if (readyError) { report("READY leave=%d", readyError); fail("video ready action left"); }
    }
    void playComplete(TInt error, const char* callback)
    {
        ++playCallbacks;
        if (!callbackAllowed(callback)) return;
        report("PLAY complete error=%d stage=%d", error, stage);
        if (error) fail("play completion reported an error");
        else if (stage < 4) fail("fixture ended before playback controls completed");
    }
    void MvpuoPlayComplete(TInt error) override { playComplete(error, "MvpuoPlayComplete"); }
    void MapcPlayComplete(TInt error) override { playComplete(error, "MapcPlayComplete"); }
    void MvpuoFrameReady(CFbsBitmap&, TInt error) override { report("CALLBACK MvpuoFrameReady error=%d", error); }
    void MvpuoEvent(const TMMFEvent& event) override { report("CALLBACK MvpuoEvent uid=0x%x error=%d", event.iEventType.iUid, event.iErrorCode); }
    void MarncResourceAvailable(TUid event, const TDesC8& data) override
    {
        ++resourceCallbacks;
        report("CALLBACK resource-available uid=0x%x bytes=%d", event.iUid, data.Length());
        TInt error = options.video ? video->WillResumePlay() : audio->WillResumePlay();
        report("RESOURCE WillResumePlay error=%d", error);
    }
    void MmsehSurfaceCreated(TInt display, const TSurfaceId& id, const TRect& crop, TVideoAspectRatio aspect) override
    {
        ++surfacesCreated;
        report("CALLBACK surface-created display=%d id=%08x:%08x:%08x:%08x crop=%d,%d,%d,%d aspect=%d/%d",
            display, id.iInternal[0], id.iInternal[1], id.iInternal[2], id.iInternal[3], crop.iTl.iX, crop.iTl.iY,
            crop.Width(), crop.Height(), aspect.iNumerator, aspect.iDenominator);
        expect(!id.IsNull() && display == 0 && !crop.IsEmpty(), "surface creation carries a real identity and crop");
        expect(liveSurface.IsNull(), "previous surface was removed before replacement");
        liveSurface = id;
    }
    void MmsehSurfaceParametersChanged(const TSurfaceId& id, const TRect& crop, TVideoAspectRatio aspect) override
    {
        ++surfacesChanged;
        report("CALLBACK surface-changed crop=%d,%d,%d,%d aspect=%d/%d", crop.iTl.iX, crop.iTl.iY,
            crop.Width(), crop.Height(), aspect.iNumerator, aspect.iDenominator);
        expect(!id.IsNull() && id == liveSurface && !crop.IsEmpty(), "surface change matches active surface");
    }
    void MmsehRemoveSurface(const TSurfaceId& id) override
    {
        ++surfacesRemoved;
        report("CALLBACK surface-removed id=%08x:%08x:%08x:%08x", id.iInternal[0], id.iInternal[1], id.iInternal[2], id.iInternal[3]);
        expect(!id.IsNull() && id == liveSurface, "surface removal matches active surface");
        liveSurface = TSurfaceId::CreateNullId();
    }

    Options options;
    Window window;
    CPeriodic* timer = nullptr;
    HBufC* source = nullptr;
    CVideoPlayerUtility2* video = nullptr;
    CMdaAudioPlayerUtility* audio = nullptr;
    bool openOnStack = false, finished = false, cancelled = false, displayAdded = false, resourceRegistered = false, schedulerRunning = false;
    TInt stage = 0;
    TInt64 duration = 0, initialPosition = 0, pausedPosition = 0, seekPosition = 0;
    TInt64 playStart = 0, stageStart = 0, cancelTime = 0, openStart = 0;
    unsigned openCallbacks = 0, prepareCallbacks = 0, playCallbacks = 0, resourceCallbacks = 0, positionAdvances = 0;
    unsigned surfacesCreated = 0, surfacesChanged = 0, surfacesRemoved = 0;
    bool surfaceDisplayAdded = false;
    TSurfaceId liveSurface = TSurfaceId::CreateNullId();
};

static bool parseInteger(const char* string, TInt& result, TInt minimum, TInt maximum)
{
    char* end = nullptr;
    const long value = strtol(string, &end, 10);
    if (!*string || *end || value < minimum || value > maximum) return false;
    result = value; return true;
}

int main(int argc, char** argv)
{
    // The SDK startup currently calls main(1, { "symbian-app", nullptr }).
    // Read the real process arguments instead of silently discarding CLI modes.
    TBuf<2048> nativeCommandLine;
    char commandLine[2049];
    char* nativeArguments[32];
    if (argc == 1) {
        TInt length = User::CommandLineLength();
        if (length < 0 || length > nativeCommandLine.MaxLength()) return 2;
        User::CommandLine(nativeCommandLine);
        for (TInt i = 0; i < nativeCommandLine.Length(); ++i) {
            if (!nativeCommandLine[i] || nativeCommandLine[i] > 127) return 2;
            commandLine[i] = static_cast<char>(nativeCommandLine[i]);
        }
        commandLine[nativeCommandLine.Length()] = 0;
        argc = parseNativeArguments(commandLine, nativeArguments, 32);
        if (argc < 0) return 2;
        argv = nativeArguments;
    }
    Options options;
    if (argc < 4 || (strcmp(argv[1], "audio") && strcmp(argv[1], "video"))
        || (strcmp(argv[2], "file") && strcmp(argv[2], "url"))) return 2;
    options.video = !strcmp(argv[1], "video"); options.url = !strcmp(argv[2], "url"); options.source = argv[3];
    if (!*options.source || strlen(options.source) > 1024) return 2;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(options.source); *p; ++p) if (*p > 127) return 2;
    int argument = 4;
    if (argument < argc && argv[argument][0] != '-') {
        if (!parseInteger(argv[argument++], options.holdMilliseconds, 4000, 60000)) return 2;
    }
    while (argument < argc) {
        const char* option = argv[argument++];
        if (!strcmp(option, "--resource-notify")) { options.resourceNotifications = true; continue; }
        if (!strcmp(option, "--surface-events")) { options.surfaceEvents = true; continue; }
        if (argument == argc) return 2;
        const char* value = argv[argument++];
        if (!strcmp(option, "--timeout-ms")) {
            if (!parseInteger(value, options.timeoutMilliseconds, 3000, 120000)) return 2;
        } else if (!strcmp(option, "--cancel-open-ms")) {
            if (!parseInteger(value, options.cancelOpenMilliseconds, 0, 60000)) return 2;
        } else if (!strcmp(option, "--expect-open-error")) {
            if (!strcmp(value, "any")) options.anyOpenError = true;
            else if (!parseInteger(value, options.expectedOpenError, -32768, -1)) return 2;
        } else return 2;
    }
    if (options.timeoutMilliseconds < options.holdMilliseconds + 3000
        || (options.cancelOpenMilliseconds >= 0 && (options.expectedOpenError || options.anyOpenError))
        || (options.surfaceEvents && (!options.video || options.resourceNotifications))) return 2;
    mkdir("C:/Data", 0777); mkdir("C:/Data/WebKit", 0777);
    if (HAL::Get(HALData::ESystemTickPeriod, tickPeriodMicroseconds) != KErrNone || tickPeriodMicroseconds <= 0) return 1;
    epochTick = User::TickCount();
    output = fopen("C:/Data/WebKit/mmf-probe.log", "w");
    if (!output) return 1;
    report("BEGIN native-mmf version=1 mode=%s transport=%s hold-ms=%d timeout-ms=%d cancel-open-ms=%d source=%s",
        options.video ? "video" : "audio", options.url ? "url" : "file", options.holdMilliseconds,
        options.timeoutMilliseconds, options.cancelOpenMilliseconds, options.source);
    CTrapCleanup* cleanup = CTrapCleanup::New();
    CActiveScheduler* scheduler = new CActiveScheduler;
    Probe* probe = nullptr;
    if (!cleanup || !scheduler) {
        expect(false, "scheduler allocation");
    } else {
        CActiveScheduler::Install(scheduler);
        TRAPD(error, { probe = new (ELeave) Probe(options); probe->startL(); });
        if (error) { report("START leave=%d", error); expect(false, "probe initialization"); }
        else probe->run();
        if (probe) probe->summarize();
    }
    delete probe;
    CActiveScheduler::Install(nullptr);
    delete scheduler; delete cleanup;
    report("%s native-mmf checks=%u failures=%u", failures ? "FAIL" : "PASS", checks, failures);
    report("native-mmf exiting %d", failures ? 1 : 0);
    fclose(output);
    return failures ? 1 : 0;
}
