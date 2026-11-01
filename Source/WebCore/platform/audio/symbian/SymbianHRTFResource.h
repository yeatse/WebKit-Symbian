/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <optional>
#include <string_view>
#include <utility>

namespace WebCore::SymbianHRTF {

constexpr unsigned sampleRate = 44100;
constexpr unsigned responseFrames = 256;
constexpr unsigned responseCount = 240;

inline std::optional<std::pair<size_t, size_t>> resourceRange(std::string_view name)
{
    if (name == "Composite")
        return std::pair<size_t, size_t> { 0, responseFrames * responseCount };
    constexpr std::string_view prefix = "IRC_Composite_C_R0195_T";
    if (!name.starts_with(prefix) || name.size() != prefix.size() + 8 || name.substr(prefix.size() + 3, 2) != "_P")
        return std::nullopt;
    auto number = [](std::string_view digits) -> std::optional<unsigned> {
        unsigned result = 0;
        for (char character : digits) {
            if (character < '0' || character > '9')
                return std::nullopt;
            result = result * 10 + character - '0';
        }
        return result;
    };
    auto azimuth = number(name.substr(prefix.size(), 3));
    auto elevation = number(name.substr(prefix.size() + 5, 3));
    if (!azimuth || !elevation || *azimuth > 345 || *azimuth % 15)
        return std::nullopt;
    unsigned elevationIndex;
    if (*elevation <= 90 && !(*elevation % 15))
        elevationIndex = *elevation / 15;
    else if (*elevation >= 315 && *elevation <= 345 && !(*elevation % 15))
        elevationIndex = *elevation / 15 - 14;
    else
        return std::nullopt;
    return std::pair<size_t, size_t> { (*azimuth / 15 * 10 + elevationIndex) * responseFrames, responseFrames };
}

} // namespace WebCore::SymbianHRTF
