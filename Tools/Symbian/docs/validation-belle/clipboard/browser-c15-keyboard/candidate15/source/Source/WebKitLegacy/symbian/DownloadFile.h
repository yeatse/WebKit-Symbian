/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <stdint.h>

namespace WebKitSymbian {

enum class DownloadFailure : uint8_t {
    None, Cancelled, Unsupported, HTTPStatus, Network, Destination, Write, Finish, SizeLimit, Busy
};

// A native, exclusive temporary file in the selected destination directory.
// All calls and destruction occur on the embedding thread. Methods return
// native TInt error values; zero alone means success. No allocations cross
// this interface. The implementation must never replace an existing target.
class DownloadFile {
public:
    static constexpr uint64_t maximumSize = 0x7fffffff;
    virtual ~DownloadFile() = default;
    virtual int open(const uint16_t* absolutePath, unsigned length) = 0;
    virtual int append(const uint8_t*, unsigned length) = 0;
    // Flush and close the temporary file, then publish using a same-directory
    // rename. Failure must not remove or change the selected target.
    virtual int finish() = 0;
    // Idempotent. Delete only this instance's temporary file. A cleanup error
    // is reported; failure is never converted into a successful download.
    virtual int cancel() = 0;
};

} // namespace WebKitSymbian
