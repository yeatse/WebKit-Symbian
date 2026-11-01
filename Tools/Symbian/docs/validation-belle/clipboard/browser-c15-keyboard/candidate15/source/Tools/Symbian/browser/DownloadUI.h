// Native AVKON UI, separate from WebCore headers and allocation conventions.
#pragma once

#include <WebKitSymbian.h>

bool ChooseNativeDownloadDestination(WebKitSymbian::Text suggestedFilename, WebKitSymbian::TextResult&, int& nativeError);
void ShowNativeDownloadResult(WebKitSymbian::Text path, WebKitSymbian::DownloadFailure, int nativeError, int cleanupError, WebKitSymbian::Text description);
