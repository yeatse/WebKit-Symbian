// The players for <audio> and <video>: the MMF audio player utility, and the
// video player utility showing its video in a window under the page.
#pragma once

#include <WebKitSymbian.h>
#include <e32std.h>

class RWindowBase;

class MNativeVideoWindow {
public:
    virtual RWindowBase& VideoWindow() = 0;
    // Called before MMF can attach a background surface. Unless the video
    // window is under a composited page, the embedding first retires its page
    // surface, then makes the page window transparent where it has alpha.
    virtual TInt PrepareNativeVideoWindow() = 0;
protected:
    ~MNativeVideoWindow() = default;
};

// A WebView::MediaPlayerFactory; context is its MNativeVideoWindow embedding.
WebKitSymbian::MediaPlayer* CreateMediaPlayer(void* aWindow, WebKitSymbian::MediaPlayerClient& aClient, bool aHasVideo);
