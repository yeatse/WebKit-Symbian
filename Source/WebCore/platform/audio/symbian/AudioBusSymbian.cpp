/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "config.h"

#if ENABLE(WEB_AUDIO) && OS(SYMBIAN)

#include "AudioBus.h"

#include "AudioBusSymbianHRTFData.h"
#include "AudioChannel.h"
#include "SymbianHRTFResource.h"
#include "SymbianWAVReader.h"

namespace WebCore {

RefPtr<AudioBus> AudioBus::loadPlatformResource(const char* name, float sampleRate)
{
    if (!name)
        return nullptr;
    auto range = SymbianHRTF::resourceRange(name);
    if (!range)
        return nullptr;
    SymbianWAV::Data metadata { { }, SymbianHRTF::sampleRate, 2, 16, 1, range->second };
    if (!SymbianWAV::convertedFrameCount(metadata, false, sampleRate))
        return nullptr;

    // The generated table is linked read-only data, not a runtime file or URL.
    // The generator validates every table range against the individual WAVs.
    auto bus = AudioBus::create(2, range->second);
    bus->setSampleRate(SymbianHRTF::sampleRate);
    for (unsigned channel = 0; channel < 2; ++channel) {
        auto output = bus->channel(channel)->mutableSpan();
        for (size_t frame = 0; frame < range->second; ++frame)
            output[frame] = SymbianHRTF::samples[(range->first + frame) * 2 + channel] / 32768.0f;
    }
    if (!sampleRate || sampleRate == SymbianHRTF::sampleRate)
        return bus;
    return AudioBus::createBySampleRateConverting(bus, false, sampleRate);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO) && OS(SYMBIAN)
