/* Copyright (C) 2026 The symbian-webkit authors. SPDX-License-Identifier: BSD-2-Clause */
#pragma once
#include "AudioDestinationSymbianNative.h"
#include <cstddef>
#include <cstdint>

namespace WebCore::SymbianAudio {

inline int16_t pcm16(float sample)
{
    if (sample != sample)
        return 0;
    if (sample <= -1)
        return -32768;
    if (sample >= 1)
        return 32767;
    return static_cast<int16_t>(sample * 32768);
}

inline void interleavePCM16(const float* left, const float* right, unsigned channels, size_t frames, uint8_t* output)
{
    for (size_t frame = 0; frame < frames; ++frame) {
        for (unsigned channel = 0; channel < channels; ++channel) {
            uint16_t value = static_cast<uint16_t>(pcm16(channel ? right[frame] : left[frame]));
            *output++ = static_cast<uint8_t>(value);
            *output++ = static_cast<uint8_t>(value >> 8);
        }
    }
}

// This counter accepts modulo-2^32 byte wraps and rejects implausible jumps
// or counts beyond submitted PCM without changing the trusted baseline. Position is an
// independent microsecond fallback. Both candidates must be monotonic and
// bounded by the data submitted in this stream epoch.
class PlaybackClock {
public:
    void reset(uint32_t bytes)
    {
        m_lastBytes = bytes;
        m_totalBytes = 0;
        m_frames = 0;
    }
    uint64_t update(uint32_t bytes, int64_t positionMicroseconds, unsigned bytesPerFrame, uint64_t submittedFrames)
    {
        if (!bytesPerFrame || submittedFrames > UINT64_MAX / bytesPerFrame)
            return m_frames;
        uint32_t delta = bytes - m_lastBytes;
        uint64_t submittedBytes = submittedFrames * bytesPerFrame;
        if (m_totalBytes <= submittedBytes && delta <= submittedBytes - m_totalBytes) {
            m_totalBytes += delta;
            m_lastBytes = bytes;
        }
        uint64_t candidate = m_totalBytes / bytesPerFrame;
        if (candidate >= m_frames && candidate <= submittedFrames)
            m_frames = candidate;
        if (positionMicroseconds >= 0) {
            uint64_t wholeSeconds = static_cast<uint64_t>(positionMicroseconds) / 1000000;
            uint64_t remaining = static_cast<uint64_t>(positionMicroseconds) % 1000000;
            // Avoid overflowing on corrupt native times; valid data is bounded
            // by submittedFrames before multiplication.
            if (wholeSeconds <= submittedFrames / outputRate) {
                candidate = wholeSeconds * outputRate + remaining * outputRate / 1000000;
                if (candidate >= m_frames && candidate <= submittedFrames)
                    m_frames = candidate;
            }
        }
        return m_frames;
    }
    uint64_t frames() const { return m_frames; }
private:
    static constexpr uint64_t outputRate = outputSampleRate;
    uint32_t m_lastBytes { 0 };
    uint64_t m_totalBytes { 0 };
    uint64_t m_frames { 0 };
};

} // namespace WebCore::SymbianAudio
