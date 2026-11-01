/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <stdint.h>

namespace WebKitSymbian {

inline bool isDownloadFilenameControl(uint16_t character)
{
    return character < 0x20 || (character >= 0x7f && character <= 0x9f)
        || character == 0x061c || character == 0x200e || character == 0x200f
        || (character >= 0x202a && character <= 0x202e)
        || (character >= 0x2066 && character <= 0x2069);
}

inline bool isDownloadFilenameForbidden(uint16_t character)
{
    return isDownloadFilenameControl(character) || character == ':' || character == '*'
        || character == '?' || character == '"' || character == '<' || character == '>'
        || character == '|' || character == '/' || character == '\\';
}

// Suggested names are untrusted hints, never paths. The caller supplies the
// bounded output buffer; no NUL terminator is written. UTF-16 pairs remain
// intact, malformed surrogates and misleading control characters become '_'.
template<typename Character>
inline unsigned makeDownloadFilename(const Character* input, unsigned length, uint16_t* output, unsigned capacity)
{
    if (!output || !capacity)
        return 0;
    if (!input)
        length = 0;
    unsigned start = 0;
    for (unsigned i = 0; i < length; ++i) {
        if (input[i] == '/' || input[i] == '\\')
            start = i + 1;
    }
    unsigned written = 0;
    for (unsigned i = start; i < length && written < capacity; ++i) {
        auto character = input[i];
        if (character >= 0xd800 && character <= 0xdbff) {
            if (i + 1 < length && input[i + 1] >= 0xdc00 && input[i + 1] <= 0xdfff) {
                if (capacity - written < 2)
                    break;
                output[written++] = character;
                output[written++] = input[++i];
                continue;
            }
            character = '_';
        } else if ((character >= 0xdc00 && character <= 0xdfff) || isDownloadFilenameForbidden(character))
            character = '_';
        output[written++] = character;
    }
    while (written && (output[written - 1] == '.' || output[written - 1] == ' '))
        --written;
    if (written)
        return written;
    constexpr uint16_t fallback[] = { 'd', 'o', 'w', 'n', 'l', 'o', 'a', 'd' };
    for (auto character : fallback) {
        if (written == capacity)
            break;
        output[written++] = character;
    }
    return written;
}

// Validate the user-selected absolute path before entering the native file
// server. File existence, media readiness and permissions are checked there.
inline bool isValidDownloadDestination(const uint16_t* path, unsigned length)
{
    if (!path || length < 4 || length > 256)
        return false;
    auto drive = path[0];
    if (!((drive >= 'A' && drive <= 'Z') || (drive >= 'a' && drive <= 'z')) || path[1] != ':' || (path[2] != '/' && path[2] != '\\'))
        return false;
    unsigned component = 3;
    for (unsigned i = 3; i <= length; ++i) {
        if (i == length || path[i] == '/' || path[i] == '\\') {
            if (i == component || path[i - 1] == '.' || path[i - 1] == ' ')
                return false;
            component = i + 1;
            continue;
        }
        auto character = path[i];
        if (isDownloadFilenameForbidden(character))
            return false;
        if (character >= 0xd800 && character <= 0xdbff) {
            if (i + 1 >= length || path[i + 1] < 0xdc00 || path[i + 1] > 0xdfff)
                return false;
            ++i;
        } else if (character >= 0xdc00 && character <= 0xdfff)
            return false;
    }
    return true;
}

} // namespace WebKitSymbian
