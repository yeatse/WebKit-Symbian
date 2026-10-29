// The players for <audio> and <video>: the MMF audio player utility, and the
// video player utility showing its video in a window under the page.
#pragma once

#include <WebKitSymbian.h>

class RWindowBase;

// A WebView::MediaPlayerFactory; context is the RWindowBase the view paints
// into, where video plays.
WebKitSymbian::MediaPlayer* CreateMediaPlayer(void* aWindow, WebKitSymbian::MediaPlayerClient& aClient, bool aHasVideo);
