/* Copyright (C) 2026 The symbian-webkit authors. SPDX-License-Identifier: BSD-2-Clause */
// Native leaves/descriptors are isolated from the no-exceptions WebCore TU.
#include "AudioDestinationSymbianNative.h"
#include "SymbianAudioPCM.h"

#include <e32base.h>
#include <mda/common/audio.h>
#include <mdaaudiooutputstream.h>
#include <mmf/common/mmffourcc.h>
#include <mmf/common/mmfaudio.h>
#include <mmf/common/mmfstandardcustomcommands.h>
#include <new>

namespace WebCore::SymbianAudio {
namespace {

constexpr TInt pollMicroseconds = 5000;
constexpr unsigned openTimeoutPolls = 1000;
constexpr unsigned stalledTimeoutPolls = 1000;

struct Control {
    unsigned channels;
    Callbacks callbacks;
    void* context;
    uint32_t requested { 0 };
    uint32_t signalled { 0 };
    uint32_t ready { 0 };
    uint32_t queuedFrames { 0 };
    RThread thread;
    TRequestStatus* status { nullptr };
};

void signal(Control& control)
{
    if (!__atomic_load_n(&control.ready, __ATOMIC_ACQUIRE))
        return;
    if (__atomic_exchange_n(&control.signalled, 1, __ATOMIC_ACQ_REL))
        return;
    auto* status = control.status;
    control.thread.RequestComplete(status, KErrNone);
}

class Session final : public MMdaAudioOutputStreamCallback, public MMMFAudioResourceNotificationCallback {
public:
    explicit Session(Control& control)
        : m_control(control)
    {
    }
    ~Session()
    {
        m_closing = true;
        m_running = false;
        delete m_timer;
        closeStream();
        delete m_command;
    }
    void initializeL();
    void command(uint32_t word);
    void poll();

private:
    class Command final : public CActive {
    public:
        static void operator delete(TAny* pointer) noexcept { User::Free(pointer); }
        static void operator delete(TAny* pointer, TLeave) noexcept { User::Free(pointer); }
        explicit Command(Session& owner)
            : CActive(EPriorityStandard)
            , m_owner(owner)
        {
            CActiveScheduler::Add(this);
        }
        ~Command() override { Cancel(); }
        void arm()
        {
            iStatus = KRequestPending;
            SetActive();
            __atomic_store_n(&m_owner.m_control.signalled, 0, __ATOMIC_RELEASE);
            if (__atomic_load_n(&m_owner.m_control.requested, __ATOMIC_ACQUIRE) != m_processed)
                signal(m_owner.m_control);
        }
        TRequestStatus* status() { return &iStatus; }
    private:
        void RunL() override
        {
            m_processed = __atomic_load_n(&m_owner.m_control.requested, __ATOMIC_ACQUIRE);
            arm();
            m_owner.command(m_processed);
        }
        void DoCancel() override
        {
            if (!__atomic_exchange_n(&m_owner.m_control.signalled, 1, __ATOMIC_ACQ_REL)) {
                auto* status = &iStatus;
                User::RequestComplete(status, KErrCancel);
            }
        }
        Session& m_owner;
        uint32_t m_processed { 0 };
    };
    class Timer final : public CTimer {
    public:
        static void operator delete(TAny* pointer) noexcept { User::Free(pointer); }
        static void operator delete(TAny* pointer, TLeave) noexcept { User::Free(pointer); }
        explicit Timer(Session& owner)
            : CTimer(EPriorityStandard)
            , m_owner(owner)
        {
            CActiveScheduler::Add(this);
        }
        void initializeL() { ConstructL(); }
        void schedule()
        {
            if (!IsActive())
                After(TTimeIntervalMicroSeconds32(pollMicroseconds));
        }
    private:
        void RunL() override { m_owner.poll(); }
        Session& m_owner;
    };
    struct Slot {
        uint8_t bytes[slotFrames * 2 * 2];
        TPtrC8 descriptor;
        bool queued { false };
        unsigned filledFrames { 0 };
    };

    void MaoscOpenComplete(TInt) override;
    void MaoscBufferCopied(TInt, const TDesC8&) override;
    void MaoscPlayComplete(TInt) override;
    void MarncResourceAvailable(TUid, const TDesC8&) override;
    void registerResourceNotification();
    void reportPolicyInterruption(TInt);
    void begin();
    void suspend();
    void closeStream();
    void pump();
    void clock();
    void failed(TInt);
    void notify(Event, TInt = KErrNone);

    Control& m_control;
    Command* m_command { nullptr };
    Timer* m_timer { nullptr };
    CMdaAudioOutputStream* m_stream { nullptr };
    TMdaAudioDataSettings m_settings;
    Slot m_slots[slotCount];
    alignas(8) float m_left[renderQuantum];
    alignas(8) float m_right[renderQuantum];
    PlaybackClock m_clock;
    uint64_t m_submitted { 0 };
    uint64_t m_lastProgress { 0 };
    double m_timestamp { 0 };
    uint32_t m_serial { 0 };
    uint32_t m_acquisitionSerial { 0 };
    unsigned m_openPolls { 0 };
    unsigned m_stallPolls { 0 };
    unsigned m_nextSlot { 0 };
    bool m_opening { false };
    bool m_running { false };
    bool m_paused { false };
    bool m_closing { false };
    bool m_pumping { false };
    bool m_resetClock { true };
    bool m_resourceRegistered { false };
    bool m_policyInterrupted { false };
    bool m_availableNotified { false };
    uint32_t m_interruptionSerial { 0 };
    TInt m_error { KErrNone };
};

void Session::initializeL()
{
    User::LeaveIfError(m_control.thread.Open(RThread().Id(), EOwnerProcess));
    m_command = new (ELeave) Command(*this);
    m_timer = new (ELeave) Timer(*this);
    m_timer->initializeL();
    m_control.status = m_command->status();
    m_command->arm();
    __atomic_store_n(&m_control.ready, 1, __ATOMIC_RELEASE);
    if (__atomic_load_n(&m_control.requested, __ATOMIC_ACQUIRE))
        signal(m_control);
}

void Session::notify(Event event, TInt error)
{
    if (!m_closing)
        m_control.callbacks.event(m_control.context, event, m_serial, error);
}

void Session::clock()
{
    if (!m_stream || m_opening)
        return;
    m_clock.update(static_cast<uint32_t>(m_stream->GetBytes()), m_stream->Position().Int64(), m_control.channels * 2, m_submitted);
    m_timestamp = m_control.callbacks.monotonicTime();
    __atomic_store_n(&m_control.queuedFrames, static_cast<uint32_t>(m_submitted - m_clock.frames()), __ATOMIC_RELEASE);
}

void Session::closeStream()
{
    m_opening = false;
    m_running = false;
    m_paused = false;
    m_policyInterrupted = false;
    m_availableNotified = false;
    if (m_stream) {
        if (m_resourceRegistered)
            m_stream->CancelRegisterAudioResourceNotification(KMMFEventCategoryAudioResourceAvailable);
        m_resourceRegistered = false;
        // Stop may synchronously return descriptors and PlayComplete. Keep
        // their storage and this observer alive; callbacks see !m_running.
        m_stream->Stop();
        delete m_stream;
        m_stream = nullptr;
    }
    // The utility destructor is the native callback/descriptor barrier. Only
    // after it returns may a new stream reuse these stable descriptors.
    for (auto& slot : m_slots) {
        slot.queued = false;
        slot.filledFrames = 0;
    }
    m_submitted = 0;
    __atomic_store_n(&m_control.queuedFrames, 0, __ATOMIC_RELEASE);
    m_lastProgress = 0;
    m_stallPolls = 0;
    m_nextSlot = 0;
    m_resetClock = true;
    m_error = KErrNone;
}

void Session::failed(TInt error)
{
    // Never delete a utility from inside its own observer callback. The
    // timer AO performs Stop/destruction after that native callback returns.
    m_running = false;
    m_opening = false;
    if (m_error == KErrNone)
        m_error = error == KErrNone ? KErrGeneral : error;
    m_timer->Cancel();
    m_timer->schedule();
}

void Session::begin()
{
    if (m_error != KErrNone || m_policyInterrupted)
        closeStream();
    if (m_running) {
        notify(Event::Started);
        return;
    }
    if (m_opening)
        return;
    if (m_paused && m_stream) {
        TInt error = m_stream->Resume();
        if (error == KErrNone) {
            m_paused = false;
            m_running = true;
            m_stallPolls = 0;
            pump();
            if (m_running)
                notify(Event::Started);
            return;
        }
        closeStream();
    }
    m_opening = true;
    m_openPolls = 0;
    TRAPD(error, m_stream = CMdaAudioOutputStream::NewL(*this, EMdaPriorityNormal, EMdaPriorityPreferenceNone));
    if (error != KErrNone) {
        failed(error);
        return;
    }
    m_settings.iSampleRate = TMdaAudioDataSettings::ESampleRate44100Hz;
    m_settings.iChannels = m_control.channels == 1 ? TMdaAudioDataSettings::EChannelsMono : TMdaAudioDataSettings::EChannelsStereo;
    registerResourceNotification();
    m_stream->Open(&m_settings);
    m_timer->schedule();
}

void Session::suspend()
{
    // The observer can have recorded a policy error before its timer runs.
    // Suspending must not erase that real acquisition event in closeStream.
    if (m_error == KErrInUse || m_error == KErrDied || m_error == KErrAccessDenied)
        reportPolicyInterruption(m_error);
    m_timer->Cancel();
    if (m_policyInterrupted && m_stream) {
        // Keep the utility/notification observer alive while preempted. Stop
        // may return descriptors; no slot is reused until closeStream's barrier.
        m_stream->Stop();
        notify(Event::Suspended);
        return;
    }
    if (m_running && m_stream) {
        m_running = false;
        clock();
        TInt error = m_stream->Pause();
        if (error == KErrNone) {
            m_paused = true;
            notify(Event::Suspended);
            return;
        }
        // The ordinary API permits NotSupported. Stop/re-open is an explicit
        // suspension fallback, never a claim of native true-pause support.
    }
    closeStream();
    notify(Event::Suspended);
}

void Session::command(uint32_t word)
{
    m_serial = word >> 2;
    switch (static_cast<Operation>(word & 3)) {
    case Operation::Resume:
        m_acquisitionSerial = m_serial;
        begin();
        break;
    case Operation::Suspend:
        suspend();
        break;
    case Operation::Close:
        m_closing = true;
        m_timer->Cancel();
        closeStream();
        __atomic_store_n(&m_control.ready, 0, __ATOMIC_RELEASE);
        CActiveScheduler::Stop();
        break;
    }
}

void Session::MaoscOpenComplete(TInt error)
{
    if (!m_opening || m_closing)
        return;
    if (error != KErrNone) {
        failed(error);
        return;
    }
    TRAP(error, m_stream->SetDataTypeL(KMMFFourCCCodePCM16); m_stream->SetAudioPropertiesL(m_settings.iSampleRate, m_settings.iChannels));
    if (error != KErrNone) {
        failed(error);
        return;
    }
    registerResourceNotification();
    // Continuous queued output works without KeepOpenAtEnd; an adaptation
    // may reject it. Unexpected playback completion remains a real error.
    m_stream->KeepOpenAtEnd();
    m_stream->SetVolume(m_stream->MaxVolume());
    m_opening = false;
    m_clock.reset(static_cast<uint32_t>(m_stream->GetBytes()));
    m_running = true;
    pump();
    if (m_running)
        notify(Event::Started);
}

void Session::MaoscBufferCopied(TInt error, const TDesC8& buffer)
{
    for (auto& slot : m_slots) {
        if (slot.queued && buffer.Ptr() == slot.descriptor.Ptr() && buffer.Length() == slot.descriptor.Length()) {
            slot.queued = false;
            slot.filledFrames = 0;
            if (m_running && error != KErrNone) {
                failed(error);
                return;
            }
            if (m_running)
                pump();
            return;
        }
    }
    // A callback for an unknown descriptor must not free/reuse any live slot.
    if (m_running)
        failed(KErrCorrupt);
}

void Session::MaoscPlayComplete(TInt error)
{
    if (m_running)
        failed(error == KErrNone ? KErrUnderflow : error);
}

void Session::registerResourceNotification()
{
    if (!m_resourceRegistered && m_stream)
        m_resourceRegistered = m_stream->RegisterAudioResourceNotification(*this, KMMFEventCategoryAudioResourceAvailable) == KErrNone;
    // NotSupported/NotReady does not imply resources are available. Explicit
    // script resume can still retry normal acquisition; no automatic playback.
}

void Session::reportPolicyInterruption(TInt error)
{
    m_error = KErrNone;
    m_policyInterrupted = true;
    m_interruptionSerial = m_acquisitionSerial;
    m_availableNotified = false;
    // Bind the policy event to acquisition, not a later Suspend command.
    // Queue it before registration/Stop may report synchronous availability.
    m_control.callbacks.event(m_control.context, Event::Interrupted, m_interruptionSerial, error);
    if (m_stream)
        registerResourceNotification();
}

void Session::MarncResourceAvailable(TUid event, const TDesC8&)
{
    if (m_closing || !m_policyInterrupted || m_availableNotified || event != KMMFEventCategoryAudioResourceAvailable)
        return;
    m_availableNotified = true;
    // Bind availability to the interruption, not to later Suspend mailboxes.
    m_control.callbacks.event(m_control.context, Event::ResourceAvailable, m_interruptionSerial, KErrNone);
}

void Session::pump()
{
    if (!m_running || m_pumping)
        return;
    m_pumping = true;
    clock();
    for (unsigned attempt = 0; attempt < slotCount; ++attempt) {
        auto& slot = m_slots[m_nextSlot];
        if (!m_running || slot.queued || m_submitted - m_clock.frames() + slotFrames > slotCount * slotFrames)
            break;
        bool rendered = true;
        // A main-thread suspend can arrive between graph quanta. Keep any
        // already rendered prefix through true pause instead of dropping graph
        // time; Stop/re-prime deliberately discards it and resets the epoch.
        for (; slot.filledFrames < slotFrames; slot.filledFrames += renderQuantum) {
            unsigned frame = slot.filledFrames;
            unsigned queued = static_cast<unsigned>(m_submitted - m_clock.frames()) + frame;
            if (!m_control.callbacks.render(m_control.context, m_left, m_right, m_clock.frames(), m_resetClock, queued, m_timestamp)) {
                rendered = false;
                break;
            }
            m_resetClock = false;
            interleavePCM16(m_left, m_right, m_control.channels, renderQuantum, slot.bytes + frame * m_control.channels * 2);
        }
        if (!rendered)
            break;
        slot.descriptor.Set(slot.bytes, slotFrames * m_control.channels * 2);
        slot.queued = true;
        m_submitted += slotFrames;
        TRAPD(error, m_stream->WriteL(slot.descriptor));
        if (error != KErrNone) {
            slot.queued = false;
            m_pumping = false;
            failed(error);
            return;
        }
        __atomic_store_n(&m_control.queuedFrames, static_cast<uint32_t>(m_submitted - m_clock.frames()), __ATOMIC_RELEASE);
        m_nextSlot = (m_nextSlot + 1) % slotCount;
    }
    m_pumping = false;
    if (m_running)
        m_timer->schedule();
}

void Session::poll()
{
    if (m_error != KErrNone) {
        TInt error = m_error;
        if (error == KErrInUse || error == KErrDied || error == KErrAccessDenied) {
            reportPolicyInterruption(error);
            if (m_stream)
                m_stream->Stop();
        } else {
            closeStream();
            notify(Event::Failed, error);
        }
        return;
    }
    if (m_opening) {
        if (++m_openPolls >= openTimeoutPolls) {
            failed(KErrTimedOut);
            return;
        }
        m_timer->schedule();
        return;
    }
    if (!m_running)
        return;
    pump();
    if (!m_running)
        return;
    if (m_clock.frames() != m_lastProgress) {
        m_lastProgress = m_clock.frames();
        m_stallPolls = 0;
    } else if (++m_stallPolls >= stalledTimeoutPolls) {
        failed(KErrTimedOut);
        return;
    }
}

} // namespace

void* create(unsigned channels, Callbacks callbacks, void* context)
{
    if (channels < 1 || channels > 2 || !callbacks.render || !callbacks.monotonicTime || !callbacks.event)
        return nullptr;
    return new (std::nothrow) Control { channels, callbacks, context };
}

void run(void* opaque)
{
    auto& control = *static_cast<Control*>(opaque);
    // SDK pthread_legacy already supplies a cleanup stack. Also support a
    // system pthread that did not install a native trap handler.
    CTrapCleanup* cleanup = User::TrapHandler() ? nullptr : CTrapCleanup::New();
    if (!User::TrapHandler()) {
        control.callbacks.event(control.context, Event::ThreadFailed, 0, KErrNoMemory);
        return;
    }
    CActiveScheduler* previousScheduler = CActiveScheduler::Current();
    CActiveScheduler* scheduler = nullptr;
    Session* session = nullptr;
    TRAPD(error,
        scheduler = new (ELeave) CActiveScheduler;
        CActiveScheduler::Install(scheduler);
        session = new (std::nothrow) Session(control);
        User::LeaveIfNull(session);
        session->initializeL());
    if (error == KErrNone)
        CActiveScheduler::Start();
    __atomic_store_n(&control.ready, 0, __ATOMIC_RELEASE);
    delete session;
    CActiveScheduler::Install(previousScheduler);
    delete scheduler;
    control.thread.Close();
    delete cleanup;
    if (error != KErrNone)
        control.callbacks.event(control.context, Event::ThreadFailed, 0, error);
}

void submit(void* opaque, Operation operation, uint32_t serial)
{
    auto& control = *static_cast<Control*>(opaque);
    uint32_t word = ((serial & 0x3fffffff) << 2) | static_cast<uint32_t>(operation);
    __atomic_store_n(&control.requested, word, __ATOMIC_RELEASE);
    signal(control);
}

unsigned queuedFrames(void* opaque)
{
    if (!opaque)
        return 0;
    return __atomic_load_n(&static_cast<Control*>(opaque)->queuedFrames, __ATOMIC_ACQUIRE);
}

void destroy(void* opaque)
{
    delete static_cast<Control*>(opaque);
}

} // namespace WebCore::SymbianAudio
