/* Copyright (C) 2026 The symbian-webkit authors. SPDX-License-Identifier: BSD-2-Clause */
#pragma once

#include <cstdint>

namespace WebCore::SymbianAudio {

constexpr unsigned outputSampleRate = 44100;
constexpr unsigned renderQuantum = 128;
constexpr unsigned slotFrames = 1024;
constexpr unsigned slotCount = 3;

// Main-thread commands are a single atomic latest-desired-state mailbox. Each
// superseded completion is resolved by the portable owner; native events carry
// the exact mailbox serial, so an old Open/Stop cannot complete a new command.
enum class Operation : uint32_t { Resume = 1, Suspend = 2, Close = 3 };
enum class Event : uint32_t { Started, Suspended, Interrupted, ResourceAvailable, Failed, ThreadFailed };

struct Callbacks {
    // Called only on the native owner/render thread, for exactly 128 frames.
    // nativeFramesPlayed is the hardware-rendered count in this stream epoch.
    // resetClock is true on the first block after a Stop/re-open, not true pause.
    bool (*render)(void*, float* left, float* right, uint64_t nativeFramesPlayed, bool resetClock, unsigned queuedFrames, double monotonicTime);
    // Sampled immediately after native played-position queries, before graph work.
    double (*monotonicTime)();
    // Called on the native owner thread. The portable owner posts to main.
    void (*event)(void*, Event, uint32_t serial, int error);
};

// create/destroy contain no native active objects. run() must be called once on
// the existing WTF rendering thread; it creates/opens/destroys all native APIs
// and installs its scheduler there. After Close, join the rendering thread
// before destroy(): that is the callback/descriptor lifetime barrier.
void* create(unsigned channels, Callbacks, void* context);
void run(void*);
void submit(void*, Operation, uint32_t serial);
void destroy(void*);
unsigned queuedFrames(void*);
// 0..1 of the device maximum (the default 1), for all streams, open or not.
// Any thread; an open stream takes it within a poll.
void setOutputVolume(double);

} // namespace WebCore::SymbianAudio
