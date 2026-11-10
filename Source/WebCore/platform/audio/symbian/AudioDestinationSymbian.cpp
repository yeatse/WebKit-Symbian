/* Copyright (C) 2026 The symbian-webkit authors. SPDX-License-Identifier: BSD-2-Clause */
#include "config.h"

#if ENABLE(WEB_AUDIO) && OS(SYMBIAN)

#include "AudioDestination.h"
#include "AudioBus.h"
#include "AudioDestinationSymbianNative.h"
#include "AudioIOCallback.h"
#include "AudioUtilities.h"
#include "MultiChannelResampler.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <wtf/MainThread.h>
#include <wtf/Threading.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {
namespace {

class AudioDestinationSymbian;

struct NativeState final : public ThreadSafeRefCounted<NativeState> {
    // owner is accessed only on main. renderOwner is immutable until the
    // rendering thread is joined. No native event retains the destination:
    // queued main callbacks retain this independent state and recheck owner.
    AudioDestinationSymbian* owner { nullptr };
    AudioDestinationSymbian* renderOwner { nullptr };
    std::atomic<bool> renderAllowed { false };
    void* control { nullptr };
};

class AudioDestinationSymbian final : public AudioDestination, public ThreadSafeRefCounted<AudioDestinationSymbian, WTF::DestructionThread::Main> {
public:
    explicit AudioDestinationSymbian(const CreationOptions&);
    ~AudioDestinationSymbian() final;
    void ref() const final { ThreadSafeRefCounted::ref(); }
    void deref() const final { ThreadSafeRefCounted::deref(); }
    void start(Function<void(Function<void()>&&)>&&, CompletionHandler<void(bool)>&&) final;
    void stop(CompletionHandler<void(bool)>&&) final;
    bool isPlaying() final { return m_playing.load(std::memory_order_acquire); }
    unsigned framesPerBuffer() const final { return SymbianAudio::renderQuantum; }
    MediaTime outputLatency() const final
    {
        // Known queued PCM interval, not a measurement of device/acoustic
        // latency. Native Position/GetBytes may have adaptation granularity.
        return MediaTime { static_cast<int64_t>(SymbianAudio::queuedFrames(m_state->control)), SymbianAudio::outputSampleRate };
    }
    bool render(float*, float*, uint64_t playedFrames, bool resetClock, unsigned queuedFrames, double monotonicTime);
    void event(SymbianAudio::Event, uint32_t serial, int error);

private:
    static bool renderNative(void*, float*, float*, uint64_t, bool, unsigned, double);
    static void eventNative(void*, SymbianAudio::Event, uint32_t, int);
    void setPlaying(bool);
    void notifyDevice(SymbianAudio::Event);
    void replaceCompletion(CompletionHandler<void(bool)>&&);
    void finish(bool);
    uint32_t nextSerial()
    {
        m_serial = (m_serial + 1) & 0x3fffffff;
        return m_serial;
    }
    static void completeLater(CompletionHandler<void(bool)>&&, bool);

    const Ref<NativeState> m_state;
    const Ref<AudioBus> m_bus;
    std::unique_ptr<MultiChannelResampler> m_resampler;
    RefPtr<Thread> m_thread;
    CompletionHandler<void(bool)> m_completion;
    AudioIOPosition m_position;
    uint64_t m_outputFramesRendered { 0 }; // rendering owner only
    double m_outputEpoch { 0 }; // rendering owner only
    std::atomic<bool> m_playing { false };
    uint32_t m_serial { 0 }; // main only
    bool m_valid { false }; // immutable after construction
    bool m_threadFailed { false }; // main only
    uint32_t m_acquisitionSerial { 0 };
    uint32_t m_interruptedSerial { 0 };
    bool m_waitingForSuspend { false };
    bool m_interrupted { false };
};

AudioDestinationSymbian::AudioDestinationSymbian(const CreationOptions& options)
    : AudioDestination(options)
    , m_state(adoptRef(*new NativeState))
    , m_bus(AudioBus::create(std::clamp(options.numberOfOutputChannels, 1u, 2u), SymbianAudio::renderQuantum, false))
{
    ASSERT(isMainThread());
    m_state->owner = this;
    m_state->renderOwner = this;
    m_valid = !options.numberOfInputChannels && options.numberOfOutputChannels >= 1 && options.numberOfOutputChannels <= 2
        && std::isfinite(options.sampleRate) && options.sampleRate >= 3000 && options.sampleRate <= 384000;
    if (!m_valid)
        return;
    if (options.sampleRate != SymbianAudio::outputSampleRate) {
        m_resampler = makeUnique<MultiChannelResampler>(static_cast<double>(options.sampleRate) / SymbianAudio::outputSampleRate,
            options.numberOfOutputChannels, SymbianAudio::renderQuantum, [this](AudioBus& bus, size_t frames) {
                ASSERT(frames == SymbianAudio::renderQuantum);
                callRenderCallback(bus, frames, m_position);
            });
    }
    m_state->control = SymbianAudio::create(options.numberOfOutputChannels, { renderNative, [] { return MonotonicTime::now().secondsSinceEpoch().value(); }, eventNative }, m_state.ptr());
    if (!m_state->control) {
        m_valid = false;
        return;
    }
    m_thread = Thread::create("WebAudio Symbian"_s, [state = m_state.copyRef()] {
        SymbianAudio::run(state->control);
    }, ThreadType::Audio);
}

AudioDestinationSymbian::~AudioDestinationSymbian()
{
    ASSERT(isMainThread());
    m_state->owner = nullptr;
    m_acquisitionSerial = 0;
    m_state->renderAllowed.store(false, std::memory_order_release);
    clearCallback();
    if (m_state->control) {
        SymbianAudio::submit(m_state->control, SymbianAudio::Operation::Close, nextSerial());
        // Native never waits on main: Close cancels its timer/utility and
        // scheduler, then join proves no descriptor/observer/render callback
        // can still refer to this object or its graph/resampler storage.
        RELEASE_ASSERT(!m_thread->waitForCompletion());
        SymbianAudio::destroy(m_state->control);
        m_state->control = nullptr;
    }
    m_state->renderOwner = nullptr;
    if (m_completion)
        completeLater(WTF::move(m_completion), false);
}

void AudioDestinationSymbian::completeLater(CompletionHandler<void(bool)>&& completion, bool success)
{
    callOnMainThread([completion = WTF::move(completion), success]() mutable { completion(success); });
}

void AudioDestinationSymbian::replaceCompletion(CompletionHandler<void(bool)>&& completion)
{
    if (m_completion)
        completeLater(WTF::move(m_completion), false);
    m_completion = WTF::move(completion);
}

void AudioDestinationSymbian::finish(bool success)
{
    m_waitingForSuspend = false;
    if (m_completion)
        completeLater(WTF::move(m_completion), success);
}

void AudioDestinationSymbian::setPlaying(bool playing)
{
    ASSERT(isMainThread());
    if (m_playing.exchange(playing, std::memory_order_acq_rel) == playing)
        return;
    Locker locker { m_callbackLock };
    if (m_callback)
        m_callback->isPlayingDidChange();
}

void AudioDestinationSymbian::start(Function<void(Function<void()>&&)>&& dispatchToRenderThread, CompletionHandler<void(bool)>&& completion)
{
    ASSERT(isMainThread());
    Ref protectedThis { *this };
    if (dispatchToRenderThread) {
        // AudioWorklet needs a separate render-thread dispatch/barrier contract.
        // Reject explicitly rather than run a worklet graph on the native AO.
        stop([completion = WTF::move(completion)](bool) mutable { completion(false); });
        return;
    }
    m_waitingForSuspend = false;
    replaceCompletion(WTF::move(completion));
    if (!m_valid || m_threadFailed) {
        finish(false);
        return;
    }
    m_interrupted = false;
    m_state->renderAllowed.store(true, std::memory_order_release);
    m_acquisitionSerial = nextSerial();
    SymbianAudio::submit(m_state->control, SymbianAudio::Operation::Resume, m_acquisitionSerial);
}

void AudioDestinationSymbian::stop(CompletionHandler<void(bool)>&& completion)
{
    ASSERT(isMainThread());
    Ref protectedThis { *this };
    m_state->renderAllowed.store(false, std::memory_order_release);
    setPlaying(false);
    if (m_waitingForSuspend && m_completion) {
        // A device notification can request the same suspension while a
        // script suspension is pending. Both wait for the same native ack.
        auto pending = WTF::move(m_completion);
        m_completion = [pending = WTF::move(pending), completion = WTF::move(completion)](bool success) mutable {
            pending(success);
            completion(success);
        };
        return;
    }
    replaceCompletion(WTF::move(completion));
    m_waitingForSuspend = true;
    if (!m_valid || m_threadFailed) {
        finish(true);
        return;
    }
    SymbianAudio::submit(m_state->control, SymbianAudio::Operation::Suspend, nextSerial());
}

bool AudioDestinationSymbian::renderNative(void* context, float* left, float* right, uint64_t playedFrames, bool resetClock, unsigned queuedFrames, double monotonicTime)
{
    auto& state = *static_cast<NativeState*>(context);
    if (!state.renderAllowed.load(std::memory_order_acquire))
        return false;
    return state.renderOwner->render(left, right, playedFrames, resetClock, queuedFrames, monotonicTime);
}

bool AudioDestinationSymbian::render(float* left, float* right, uint64_t playedFrames, bool resetClock, unsigned queuedFrames, double monotonicTime)
{
    ASSERT(!isMainThread());
    if (resetClock)
        m_outputEpoch = static_cast<double>(m_outputFramesRendered) / SymbianAudio::outputSampleRate;
    // This is a played PCM clock, not a submitted/copy count. A Stop/re-prime
    // advances the epoch to the next output frame; true pause keeps the epoch.
    m_position = { Seconds { m_outputEpoch + static_cast<double>(playedFrames) / SymbianAudio::outputSampleRate }, MonotonicTime::fromRawSeconds(monotonicTime) };
    m_bus->setChannelMemory(0, std::span { left, SymbianAudio::renderQuantum });
    if (numberOfOutputChannels() == 2)
        m_bus->setChannelMemory(1, std::span { right, SymbianAudio::renderQuantum });
    if (m_resampler)
        m_resampler->process(m_bus, SymbianAudio::renderQuantum);
    else
        callRenderCallback(m_bus, SymbianAudio::renderQuantum, m_position);
    m_outputFramesRendered += SymbianAudio::renderQuantum;
    UNUSED_PARAM(queuedFrames);
    return true;
}

void AudioDestinationSymbian::eventNative(void* context, SymbianAudio::Event event, uint32_t serial, int error)
{
    Ref state { *static_cast<NativeState*>(context) };
    callOnMainThread([state = WTF::move(state), event, serial, error] {
        if (auto* owner = state->owner)
            owner->event(event, serial, error);
    });
}

void AudioDestinationSymbian::notifyDevice(SymbianAudio::Event event)
{
    ASSERT(isMainThread());
    AudioIOCallback* callback;
    {
        Locker locker { m_callbackLock };
        callback = m_callback;
    }
    // State callbacks can reenter stop/setPlaying; never hold callbackLock.
    // All callback replacement/destruction is on this same main thread.
    if (!callback)
        return;
    if (event == SymbianAudio::Event::Interrupted)
        callback->audioDeviceInterrupted();
    else if (event == SymbianAudio::Event::ResourceAvailable)
        callback->audioDeviceMayResume();
    else
        callback->audioDeviceFailed();
}

void AudioDestinationSymbian::event(SymbianAudio::Event event, uint32_t serial, int error)
{
    ASSERT(isMainThread());
    Ref protectedThis { *this };
    if (event == SymbianAudio::Event::ThreadFailed) {
        m_threadFailed = true;
        m_state->renderAllowed.store(false, std::memory_order_release);
        setPlaying(false);
        finish(false);
        notifyDevice(SymbianAudio::Event::Failed);
        return;
    }
    if (event == SymbianAudio::Event::ResourceAvailable) {
        if (m_interrupted && serial == m_interruptedSerial)
            notifyDevice(event);
        return;
    }
    if (event == SymbianAudio::Event::Interrupted) {
        // Suspend changes the command serial, not ownership of the current
        // device acquisition. A new Resume or Close invalidates the old one.
        if (serial != m_acquisitionSerial)
            return;
        m_interrupted = true;
        m_interruptedSerial = serial;
        m_state->renderAllowed.store(false, std::memory_order_release);
        setPlaying(false);
        if (serial == m_serial)
            finish(false);
        notifyDevice(event);
        return;
    }
    if (serial != m_serial)
        return;
    switch (event) {
    case SymbianAudio::Event::Started:
        setPlaying(true);
        finish(true);
        break;
    case SymbianAudio::Event::Suspended:
        setPlaying(false);
        finish(true);
        break;
    case SymbianAudio::Event::Interrupted:
        RELEASE_ASSERT_NOT_REACHED();
    case SymbianAudio::Event::ResourceAvailable:
        RELEASE_ASSERT_NOT_REACHED();
    case SymbianAudio::Event::Failed:
        m_interrupted = false;
        m_state->renderAllowed.store(false, std::memory_order_release);
        setPlaying(false);
        finish(false);
        notifyDevice(event);
        UNUSED_PARAM(error);
        break;
    case SymbianAudio::Event::ThreadFailed:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

} // namespace

Ref<AudioDestination> AudioDestination::create(const CreationOptions& options)
{
    return adoptRef(*new AudioDestinationSymbian(options));
}

float AudioDestination::hardwareSampleRate()
{
    // Preferred native PCM rate, configured and checked by the real utility.
    // start fails if the adaptation rejects it; it is not a hardware probe.
    return SymbianAudio::outputSampleRate;
}

unsigned long AudioDestination::maxChannelCount()
{
    return 2;
}

} // namespace WebCore

#endif
