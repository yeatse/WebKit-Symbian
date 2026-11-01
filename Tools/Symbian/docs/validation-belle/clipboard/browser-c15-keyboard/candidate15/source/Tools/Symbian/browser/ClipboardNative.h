// Native system clipboard. Uses the application's thread and never transfers
// native heap ownership to WebCore.
#pragma once

#include "WebKitSymbianClipboard.h"

WebKitSymbian::ClipboardClient& NativeClipboard();
