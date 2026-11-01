/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace WebCore::SymbianWAV {

// Keep decodeAudioData memory bounded independently of container metadata.
constexpr size_t maximumInputBytes = 32 * 1024 * 1024;
constexpr size_t maximumAudioBusBytes = 16 * 1024 * 1024;
constexpr unsigned minimumSampleRate = 3000;
constexpr unsigned maximumSampleRate = 384000;

inline uint16_t read16(std::span<const uint8_t> bytes, size_t offset)
{
    return bytes[offset] | static_cast<uint16_t>(bytes[offset + 1]) << 8;
}

inline uint32_t read32(std::span<const uint8_t> bytes, size_t offset)
{
    return read16(bytes, offset) | static_cast<uint32_t>(read16(bytes, offset + 2)) << 16;
}

inline bool hasTag(std::span<const uint8_t> bytes, size_t offset, const char (&tag)[5])
{
    return bytes[offset] == tag[0] && bytes[offset + 1] == tag[1]
        && bytes[offset + 2] == tag[2] && bytes[offset + 3] == tag[3];
}

struct Data {
    std::span<const uint8_t> bytes;
    unsigned sampleRate;
    unsigned channels;
    unsigned bitsPerSample;
    unsigned format;
    size_t frames;

    std::optional<float> sample(size_t frame, unsigned channel) const
    {
        if (frame >= frames || channel >= channels)
            return std::nullopt;
        size_t offset = (frame * channels + channel) * (bitsPerSample / 8);
        double value;
        if (format == 3) {
            static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
            static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);
            if (bitsPerSample == 32)
                value = std::bit_cast<float>(read32(bytes, offset));
            else {
                uint64_t bits = read32(bytes, offset) | static_cast<uint64_t>(read32(bytes, offset + 4)) << 32;
                value = std::bit_cast<double>(bits);
            }
        } else {
            switch (bitsPerSample) {
            case 8:
                value = (static_cast<int>(bytes[offset]) - 128) / 128.0;
                break;
            case 16:
                value = std::bit_cast<int16_t>(read16(bytes, offset)) / 32768.0;
                break;
            case 24: {
                uint32_t bits = read16(bytes, offset) | static_cast<uint32_t>(bytes[offset + 2]) << 16;
                int32_t signedValue = bits & 0x800000 ? static_cast<int32_t>(bits) - 0x1000000 : static_cast<int32_t>(bits);
                value = signedValue / 8388608.0;
                break;
            }
            case 32:
                value = std::bit_cast<int32_t>(read32(bytes, offset)) / 2147483648.0;
                break;
            default:
                return std::nullopt;
            }
        }
        // Float WAVE may have headroom, but non-finite or non-float-representable
        // values are rejected rather than feeding infinities/NaNs to the graph.
        if (!std::isfinite(value) || value > std::numeric_limits<float>::max() || value < -std::numeric_limits<float>::max())
            return std::nullopt;
        return static_cast<float>(value);
    }
};

inline std::optional<Data> parse(std::span<const uint8_t> bytes)
{
    if (bytes.size() < 12 || bytes.size() > maximumInputBytes || !hasTag(bytes, 0, "RIFF") || !hasTag(bytes, 8, "WAVE"))
        return std::nullopt;
    // Accept exactly one complete RIFF form; RF64, RIFX and appended forms are
    // deliberately outside this backend's supported formats.
    uint32_t riffSize = read32(bytes, 4);
    if (riffSize < 4 || static_cast<uint64_t>(riffSize) + 8 != bytes.size())
        return std::nullopt;

    std::span<const uint8_t> formatBytes;
    std::span<const uint8_t> dataBytes;
    bool haveData = false;
    for (size_t offset = 12; offset < bytes.size();) {
        if (bytes.size() - offset < 8)
            return std::nullopt;
        uint32_t chunkSize = read32(bytes, offset + 4);
        size_t bodyOffset = offset + 8;
        if (chunkSize > bytes.size() - bodyOffset)
            return std::nullopt;
        size_t paddedSize = static_cast<size_t>(chunkSize) + (chunkSize & 1);
        if (paddedSize > bytes.size() - bodyOffset)
            return std::nullopt;
        if (hasTag(bytes, offset, "fmt ")) {
            if (!formatBytes.empty())
                return std::nullopt;
            formatBytes = bytes.subspan(bodyOffset, chunkSize);
            if (formatBytes.size() != 16 && (formatBytes.size() != 18 || read16(formatBytes, 16)))
                return std::nullopt;
        } else if (hasTag(bytes, offset, "data")) {
            if (haveData)
                return std::nullopt;
            haveData = true;
            dataBytes = bytes.subspan(bodyOffset, chunkSize);
        }
        offset = bodyOffset + paddedSize;
    }
    if (formatBytes.empty() || !haveData || dataBytes.empty())
        return std::nullopt;

    unsigned format = read16(formatBytes, 0);
    unsigned channels = read16(formatBytes, 2);
    unsigned sampleRate = read32(formatBytes, 4);
    unsigned byteRate = read32(formatBytes, 8);
    unsigned blockAlign = read16(formatBytes, 12);
    unsigned bits = read16(formatBytes, 14);
    if (channels < 1 || channels > 2 || sampleRate < minimumSampleRate || sampleRate > maximumSampleRate)
        return std::nullopt;
    bool supported = (format == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32))
        || (format == 3 && (bits == 32 || bits == 64));
    if (!supported || blockAlign != channels * (bits / 8) || byteRate != sampleRate * blockAlign || dataBytes.size() % blockAlign)
        return std::nullopt;
    return Data { dataBytes, sampleRate, channels, bits, format, dataBytes.size() / blockAlign };
}

// The source and converted AudioBus allocations coexist while the common
// sinc resampler runs. Check their combined sample storage before allocating.
inline std::optional<size_t> convertedFrameCount(const Data& data, bool mixToMono, float requestedSampleRate)
{
    double target = requestedSampleRate ? requestedSampleRate : data.sampleRate;
    if (!std::isfinite(target) || target < minimumSampleRate || target > maximumSampleRate)
        return std::nullopt;
    unsigned channels = mixToMono ? 1 : data.channels;
    size_t sourceBytes = data.frames * channels * sizeof(float);
    if (sourceBytes > maximumAudioBusBytes)
        return std::nullopt;
    if (target == data.sampleRate)
        return data.frames;
    double ratio = static_cast<double>(data.sampleRate) / target;
    double destinationFrames = data.frames / ratio;
    size_t remainingSamples = (maximumAudioBusBytes - sourceBytes) / (channels * sizeof(float));
    if (destinationFrames < 1 || destinationFrames > remainingSamples)
        return std::nullopt;
    return static_cast<size_t>(destinationFrames);
}

} // namespace WebCore::SymbianWAV
