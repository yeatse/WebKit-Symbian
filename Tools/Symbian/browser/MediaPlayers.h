// The players for <audio> and <video>: the MMF audio player utility, and the
// video player utility showing its video in a window under the page.
#pragma once

#include <WebKitSymbian.h>
#include <e32std.h>

class RWindow;

class MNativeVideoWindow {
public:
    virtual RWindow& VideoWindow() = 0;
    // Called before MMF can attach a background surface. The embedding first
    // retires its page surface, then enables the native video's alpha window.
    virtual TInt PrepareNativeVideoWindow() = 0;
protected:
    ~MNativeVideoWindow() = default;
};

// A WebView::MediaPlayerFactory; context is its MNativeVideoWindow embedding.
WebKitSymbian::MediaPlayer* CreateMediaPlayer(void* aWindow, WebKitSymbian::MediaPlayerClient& aClient, bool aHasVideo);
