/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "config.h"

#if ENABLE(WEB_AUDIO) && OS(SYMBIAN)

#include "AudioFileReader.h"

#include "AudioBus.h"
#include "AudioChannel.h"
#include "SymbianWAVReader.h"

namespace WebCore {

RefPtr<AudioBus> createBusFromInMemoryAudioFile(std::span<const uint8_t> bytes, bool mixToMono, float sampleRate)
{
    auto data = SymbianWAV::parse(bytes);
    if (!data || !SymbianWAV::convertedFrameCount(*data, mixToMono, sampleRate))
        return nullptr;

    unsigned channels = mixToMono ? 1 : data->channels;
    auto bus = AudioBus::create(channels, data->frames);
    bus->setSampleRate(data->sampleRate);
    for (size_t frame = 0; frame < data->frames; ++frame) {
        for (unsigned channel = 0; channel < channels; ++channel) {
            auto value = data->sample(frame, channel);
            if (!value)
                return nullptr;
            if (mixToMono && data->channels == 2) {
                auto right = data->sample(frame, 1);
                if (!right)
                    return nullptr;
                // Half before adding so finite float headroom cannot overflow.
                *value = *value * 0.5f + *right * 0.5f;
            }
            bus->channel(channel)->mutableSpan()[frame] = *value;
        }
    }
    if (!sampleRate || sampleRate == data->sampleRate)
        return bus;
    return AudioBus::createBySampleRateConverting(bus, false, sampleRate);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO) && OS(SYMBIAN)
