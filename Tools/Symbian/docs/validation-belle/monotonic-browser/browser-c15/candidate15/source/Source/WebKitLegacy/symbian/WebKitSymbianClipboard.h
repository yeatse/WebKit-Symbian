/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "WebKitSymbian.h"

namespace WebKitSymbian {

// Bound native allocation and UTF-16 conversion to one MiB of text.
inline constexpr int maximumClipboardTextLength = 512 * 1024;

// Synchronous, application-thread calls. Native allocations never cross this
// interface. A successful read calls setText exactly once, including for an
// empty string; false means no plain text or an error. Implementations catch
// native leaves and report write failures before the editor removes cut text.
class ClipboardClient {
public:
    virtual bool hasPlainText() = 0;
    virtual bool readPlainText(TextResult&) = 0;
    virtual bool writePlainText(Text) = 0;
    virtual bool clear() = 0;

protected:
    ~ClipboardClient() = default;
};

// The application retains ownership and unregisters before destroying client.
// With no client, clipboard access fails; it does not create a private copy.
void setClipboardClient(ClipboardClient*);
ClipboardClient* clipboardClient();

} // namespace WebKitSymbian
