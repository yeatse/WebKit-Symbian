/*
 * Copyright (C) 2026 The symbian-webkit authors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "WebKitSymbian.h"
#include <WebCore/IntRect.h>
#include <WebCore/DestinationColorSpace.h>
#include <WebCore/MediaPlayerPrivate.h>
#include <wtf/FileSystem.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class PlatformMediaResource;
}

namespace WebKitSymbian {

// <audio> and <video> on the application's MediaPlayer: the media is
// downloaded to a file through WebCore's loader, and opened while it
// downloads. Video plays under the page, which leaves its box transparent.
class MediaPlayerPrivateSymbian final : public WebCore::MediaPlayerPrivateInterface, public MediaPlayerClient, public CanMakeWeakPtr<MediaPlayerPrivateSymbian>, public RefCounted<MediaPlayerPrivateSymbian> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(MediaPlayerPrivateSymbian);
public:
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }
    constexpr WebCore::MediaPlayerType mediaPlayerType() const final { return WebCore::MediaPlayerType::Symbian; }
    WebCore::DestinationColorSpace colorSpace() final { return WebCore::DestinationColorSpace::SRGB(); }
    // Once per process; media files go in directory.
    static void registerMediaEngine(const String& directory);
    static void setFactory(WebView::MediaPlayerFactory, void* context);

    // Around each paint of the view, to tell which videos went out of sight.
    static void willPaint();
    static void didPaint(const WebCore::IntRect&);
    static bool requiresNativeVideoWindow();

    explicit MediaPlayerPrivateSymbian(WebCore::MediaPlayer&);
    ~MediaPlayerPrivateSymbian();

    void didReceiveResponse(long long expectedContentLength);
    void didReceiveData(std::span<const uint8_t>);
    void didFinishLoading();
    void didFailLoading();
    uint64_t loadGeneration() const { return m_loadGeneration; }
    bool isCurrentLoad(uint64_t generation) const { return generation == m_loadGeneration; }

private:
    void load(const String&) final;
#if ENABLE(MEDIA_SOURCE)
    void load(const URL&, const LoadOptions&, WebCore::MediaSourcePrivateClient&) final { setNetworkState(WebCore::MediaPlayer::NetworkState::FormatError); }
#endif
#if ENABLE(MEDIA_STREAM)
    void load(WebCore::MediaStreamPrivate&) final { setNetworkState(WebCore::MediaPlayer::NetworkState::FormatError); }
#endif
    void cancelLoad() final;
    void prepareToPlay() final;
    void play() final;
    void pause() final;

    String engineDescription() const final { return "Symbian MMF"_s; }
    WebCore::FloatSize naturalSize() const final { return m_videoSize; }
    bool hasVideo() const final { return !m_videoSize.isEmpty(); }
    bool hasAudio() const final { return m_hasAudio; }
    void setPageIsVisible(bool) final;

    MediaTime duration() const final;
    MediaTime currentTime() const final;
    void seekToTarget(const WebCore::SeekTarget&) final;
    bool seeking() const final { return m_seeking; }
    bool paused() const final { return m_paused; }
    bool timeIsProgressing() const final { return m_playing && !m_stalled && !m_opening; }
    void setVolume(float) final;
    void setMuted(bool) final;

    WebCore::MediaPlayer::NetworkState networkState() const final { return m_networkState; }
    WebCore::MediaPlayer::ReadyState readyState() const final { return m_readyState; }
    MediaTime maxTimeSeekable() const final;
    const WebCore::PlatformTimeRanges& buffered() const final;
    double rate() const final { return 1; }
    double maxFastForwardRate() const final { return 1; }
    double minFastReverseRate() const final { return 0; }
    bool didLoadingProgress() const final;
    void setPreload(WebCore::MediaPlayer::Preload preload) final { m_preload = preload; }
    WebCore::MediaPlayer::MovieLoadType movieLoadType() const final { return WebCore::MediaPlayer::MovieLoadType::Download; }

    void paint(WebCore::GraphicsContext&, const WebCore::FloatRect&) final;
    // The MMF surface is window content, not a CPU-readable frame. Canvas
    // painting must not reposition the native video window or punch a hole.
    void paintCurrentFrameInContext(WebCore::GraphicsContext&, const WebCore::FloatRect&) final { }

    // MediaPlayerClient
    void mediaPlayerOpened(int error, double duration, int videoWidth, int videoHeight, bool hasAudio) final;
    void mediaPlayerEnded(int error) final;

    void startLoading();
    bool createBackend();
    void scanBoxes(const uint8_t*, size_t);
    unsigned long long bytesToResume() const;
    bool hasEnoughToOpen() const;
    void openIfReady();
    void open();
    void setNetworkState(WebCore::MediaPlayer::NetworkState);
    void setReadyState(WebCore::MediaPlayer::ReadyState);
    void updateVolume();
    void setVideoRect(const WebCore::IntRect&, const WebCore::IntRect& = { });
    bool handleNativeError(int);

    ThreadSafeWeakPtr<WebCore::MediaPlayer> m_player;
    std::unique_ptr<MediaPlayerClient> m_backendClient;
    std::unique_ptr<MediaPlayer> m_backend;
    URL m_url;
    RefPtr<WebCore::PlatformMediaResource> m_resource;
    // The file the backend opens: the download, or a local file.
    String m_path;
    bool m_isDownload { false };

    // The download, which plays as it arrives.
    unsigned long long m_bytesReceived { 0 };
    long long m_expectedLength { 0 };
    bool m_complete { false };
    // The size to reach before trying to open, which grows after a failed try.
    unsigned long long m_openThreshold { 0 };
    // What was there when the backend opened the file; partly means it
    // opened before the download was complete, so it can end early.
    unsigned long long m_bytesAtOpen { 0 };
    bool m_openedPartly { false };
    bool m_opening { false };
    // The backend ended early, at m_stallBytes, and opens again when more has
    // arrived; it then goes on from m_resumePosition.
    bool m_stalled { false };
    unsigned long long m_stallBytes { 0 };
    double m_resumePosition { 0 };
    mutable double m_lastPosition { 0 };
    mutable bool m_positionErrorReported { false };

    // An MP4 or 3GP file can play once its 'moov' box is there: it holds the
    // index of the media data. A file with the data first can only play when
    // it is complete.
    enum class Boxes { None, Scanning, FoundMovie, DataFirst };
    Boxes m_boxes { Boxes::None };
    unsigned long long m_nextBox { 0 };
    unsigned long long m_movieEnd { 0 };
    uint8_t m_boxHeader[8];
    unsigned m_boxHeaderLength { 0 };

    WebCore::MediaPlayer::NetworkState m_networkState { WebCore::MediaPlayer::NetworkState::Empty };
    WebCore::MediaPlayer::ReadyState m_readyState { WebCore::MediaPlayer::ReadyState::HaveNothing };
    WebCore::MediaPlayer::Preload m_preload { WebCore::MediaPlayer::Preload::Auto };
    WebCore::FloatSize m_videoSize;
    double m_duration { 0 };
    float m_volume { 1 };
    bool m_muted { false };
    bool m_loadDeferred { false };
    bool m_opened { false };
    bool m_hasAudio { false };
    bool m_pageVisible { true };
    uint64_t m_loadGeneration { 0 };
    mutable WebCore::PlatformTimeRanges m_buffered;
    bool m_paused { true };
    bool m_playing { false };
    bool m_ended { false };
    bool m_seeking { false };
    mutable bool m_loadingProgressed { false };
    // Where the video was last painted, in view pixels, and whether it was
    // painted in the current paint.
    WebCore::IntRect m_videoRect;
    WebCore::IntRect m_videoClip;
    bool m_paintedVideo { false };
};

} // namespace WebKitSymbian
