// The players for <audio> and <video>: the MMF audio player utility, and the
// video player utility showing its video in a window under the page.
#pragma once

#include <WebKitSymbian.h>
#include <e32std.h>

class RWindow;

class MNativeVideoWindow {
public:
    virtual RWindow& VideoWindow() = 0;
    // Called before MMF can attach a background surface. Unless the video
    // window is under a composited page, the embedding first retires its page
    // surface, then makes the page window transparent where it has alpha.
    virtual TInt PrepareNativeVideoWindow() = 0;
protected:
    ~MNativeVideoWindow() = default;
};

// A WebView::MediaPlayerFactory; context is its MNativeVideoWindow embedding.
WebKitSymbian::MediaPlayer* CreateMediaPlayer(void* aWindow, WebKitSymbian::MediaPlayerClient& aClient, bool aHasVideo);

// The browser's media volume, which the volume keys set: every player plays
// at the page's volume scaled by it, from 0 (silent) to KMediaVolumeSteps
// (the device's full volume). It is kept in C:\Data\WebKit\media-volume.
const TInt KMediaVolumeSteps = 10;
TInt MediaVolumeLevel();
// Sets the level, clamped to 0..KMediaVolumeSteps, for the players playing
// now and later; returns the level set.
TInt SetMediaVolumeLevel(TInt aLevel);
