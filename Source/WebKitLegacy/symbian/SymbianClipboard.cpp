/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "WebKitSymbianClipboard.h"

namespace WebKitSymbian {

static ClipboardClient* client;

void setClipboardClient(ClipboardClient* newClient)
{
    client = newClient;
}

ClipboardClient* clipboardClient()
{
    return client;
}

} // namespace WebKitSymbian
