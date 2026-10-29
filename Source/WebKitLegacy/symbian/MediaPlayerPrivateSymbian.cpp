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

#include "MediaPlayerPrivateSymbian.h"

#include <WebCore/GraphicsContext.h>
#include <WebCore/MediaPlayer.h>
#include <WebCore/PlatformMediaResourceLoader.h>
#include <WebCore/PlatformTimeRanges.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>

namespace WebKitSymbian {

using namespace WebCore;

static WebView::MediaPlayerFactory s_factory;
static void* s_factoryContext;

static String& mediaDirectory()
{
    static NeverDestroyed<String> directory;
    return directory;
}

static HashSet<MediaPlayerPrivateSymbian*>& players()
{
    static NeverDestroyed<HashSet<MediaPlayerPrivateSymbian*>> players;
    return players;
}

// What the phones' MMF plug-ins play (and EKA2L1's FFmpeg backend).
static const HashSet<String, ASCIICaseInsensitiveHash>& mimeTypes()
{
    static NeverDestroyed<HashSet<String, ASCIICaseInsensitiveHash>> types(std::initializer_list<String> {
        "audio/3gpp"_s, "audio/aac"_s, "audio/amr"_s, "audio/mp3"_s, "audio/mp4"_s, "audio/mpeg"_s,
        "audio/wav"_s, "audio/x-m4a"_s, "audio/x-wav"_s, "video/3gpp"_s, "video/mp4"_s, "video/x-m4v"_s,
    });
    return types;
}

static bool isSupportedCodec(const String& codec)
{
    // H.264 (Baseline on the phones), MPEG-4 Part 2, AAC, MP3 and AMR.
    for (auto prefix : { "avc1", "mp4v", "mp4a", "mp3", "samr", "1" }) {
        if (codec.startsWithIgnoringASCIICase(prefix))
            return true;
    }
    return false;
}

class MediaPlayerFactorySymbian final : public MediaPlayerFactory {
private:
    // Like the hole-punch engine, which this port's would be the only one.
    MediaPlayerEnums::MediaEngineIdentifier identifier() const final { return MediaPlayerEnums::MediaEngineIdentifier::HolePunch; }

    std::unique_ptr<MediaPlayerPrivateInterface> createMediaEnginePlayer(WebCore::MediaPlayer* player) const final
    {
        return makeUnique<MediaPlayerPrivateSymbian>(player);
    }

    void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types) const final
    {
        if (s_factory)
            types = mimeTypes();
    }

    WebCore::MediaPlayer::SupportsType supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters) const final
    {
        if (!s_factory || parameters.isMediaSource || parameters.isMediaStream || !mimeTypes().contains(parameters.type.containerType()))
            return WebCore::MediaPlayer::SupportsType::IsNotSupported;
        auto codecs = parameters.type.codecs();
        if (codecs.isEmpty())
            return WebCore::MediaPlayer::SupportsType::MayBeSupported;
        for (auto& codec : codecs) {
            if (!isSupportedCodec(codec))
                return WebCore::MediaPlayer::SupportsType::IsNotSupported;
        }
        return WebCore::MediaPlayer::SupportsType::IsSupported;
    }
};

void MediaPlayerPrivateSymbian::registerMediaEngine(const String& directory)
{
    mediaDirectory() = FileSystem::pathByAppendingComponent(directory, "Media");
    // Files left by a previous run.
    for (auto& path : FileSystem::listDirectory(mediaDirectory(), "*"))
        FileSystem::deleteFile(path);
    MediaPlayerFactorySupport::callRegisterMediaEngine([](MediaEngineRegistrar registrar) {
        registrar(makeUnique<MediaPlayerFactorySymbian>());
    });
}

void MediaPlayerPrivateSymbian::setFactory(WebView::MediaPlayerFactory factory, void* context)
{
    s_factory = factory;
    s_factoryContext = context;
}

namespace {

class ResourceClient final : public PlatformMediaResourceClient {
public:
    static Ref<ResourceClient> create(MediaPlayerPrivateSymbian& player) { return adoptRef(*new ResourceClient(player)); }

private:
    explicit ResourceClient(MediaPlayerPrivateSymbian& player)
        : m_player(makeWeakPtr(player))
    {
    }

    void responseReceived(PlatformMediaResource&, const ResourceResponse& response, CompletionHandler<void(ShouldContinuePolicyCheck)>&& completionHandler) final
    {
        if (response.httpStatusCode() >= 400) {
            completionHandler(ShouldContinuePolicyCheck::No);
            if (m_player)
                m_player->didFailLoading();
            return;
        }
        completionHandler(ShouldContinuePolicyCheck::Yes);
    }

    // The data goes to a file, not the memory cache.
    bool shouldCacheResponse(PlatformMediaResource&, const ResourceResponse&) final { return false; }

    void dataReceived(PlatformMediaResource&, const char* data, int length) final
    {
        if (m_player)
            m_player->didReceiveData(data, length);
    }

    void accessControlCheckFailed(PlatformMediaResource&, const ResourceError&) final
    {
        if (m_player)
            m_player->didFailLoading();
    }

    void loadFailed(PlatformMediaResource&, const ResourceError&) final
    {
        if (m_player)
            m_player->didFailLoading();
    }

    void loadFinished(PlatformMediaResource&, const NetworkLoadMetrics&) final
    {
        if (m_player)
            m_player->didFinishLoading();
    }

    WeakPtr<MediaPlayerPrivateSymbian> m_player;
};

} // namespace

MediaPlayerPrivateSymbian::MediaPlayerPrivateSymbian(WebCore::MediaPlayer* player)
    : m_player(player)
{
    players().add(this);
}

MediaPlayerPrivateSymbian::~MediaPlayerPrivateSymbian()
{
    players().remove(this);
    cancelLoad();
}

void MediaPlayerPrivateSymbian::load(const String& url)
{
    cancelLoad();
    m_url = URL(URL(), url);
    setNetworkState(WebCore::MediaPlayer::NetworkState::Loading);
    setReadyState(WebCore::MediaPlayer::ReadyState::HaveNothing);
    if (m_preload == WebCore::MediaPlayer::Preload::None) {
        m_loadDeferred = true;
        setNetworkState(WebCore::MediaPlayer::NetworkState::Idle);
        return;
    }
    startLoading();
}

void MediaPlayerPrivateSymbian::prepareToPlay()
{
    if (!m_loadDeferred)
        return;
    m_loadDeferred = false;
    setNetworkState(WebCore::MediaPlayer::NetworkState::Loading);
    startLoading();
}

void MediaPlayerPrivateSymbian::startLoading()
{
    if (!s_factory) {
        setNetworkState(WebCore::MediaPlayer::NetworkState::FormatError);
        return;
    }

    // A local file opens where it is. Its path is "/C:/...".
    if (m_url.isLocalFile()) {
        String path = m_url.fileSystemPath();
        if (path.length() > 2 && path[0] == '/' && path[2] == ':')
            path = path.substring(1);
        open(path);
        return;
    }

    auto loader = m_player->createResourceLoader();
    if (!loader) {
        setNetworkState(WebCore::MediaPlayer::NetworkState::NetworkError);
        return;
    }

    // Keep the extension, which the MMF uses in choosing a plug-in.
    static unsigned fileNumber;
    String name = makeString(++fileNumber);
    auto lastComponent = m_url.lastPathComponent().toString();
    size_t dot = lastComponent.reverseFind('.');
    if (dot != notFound && lastComponent.length() - dot <= 5)
        name = makeString(name, lastComponent.substring(dot));
    FileSystem::makeAllDirectories(mediaDirectory());
    m_downloadPath = FileSystem::pathByAppendingComponent(mediaDirectory(), name);
    m_file = FileSystem::openFile(m_downloadPath, FileSystem::FileOpenMode::Write);
    if (!FileSystem::isHandleValid(m_file)) {
        setNetworkState(WebCore::MediaPlayer::NetworkState::NetworkError);
        return;
    }

    m_resource = loader->requestResource(ResourceRequest(m_url), PlatformMediaResourceLoader::DisallowCaching);
    if (!m_resource) {
        didFailLoading();
        return;
    }
    m_resource->setClient(ResourceClient::create(*this));
}

void MediaPlayerPrivateSymbian::didReceiveData(const char* data, int length)
{
    if (FileSystem::writeToFile(m_file, data, length) != length) {
        didFailLoading();
        return;
    }
    m_loadingProgressed = true;
}

void MediaPlayerPrivateSymbian::didFinishLoading()
{
    FileSystem::closeFile(m_file);
    m_resource = nullptr;
    open(m_downloadPath);
}

void MediaPlayerPrivateSymbian::didFailLoading()
{
    cancelLoad();
    setNetworkState(WebCore::MediaPlayer::NetworkState::NetworkError);
}

void MediaPlayerPrivateSymbian::cancelLoad()
{
    if (m_resource) {
        m_resource->stop();
        m_resource->setClient(nullptr);
        m_resource = nullptr;
    }
    FileSystem::closeFile(m_file);
    setVideoRect({ });
    m_backend = nullptr;
    if (!m_downloadPath.isEmpty()) {
        FileSystem::deleteFile(m_downloadPath);
        m_downloadPath = String();
    }
    m_loadDeferred = false;
    m_opened = false;
    m_playing = false;
    m_ended = false;
    m_duration = 0;
    m_videoSize = { };
}

void MediaPlayerPrivateSymbian::open(const String& path)
{
    // The player for video can't tell an audio file's MIME type from a
    // video's, so only the element and the type choose.
    bool hasVideo = m_player->isVideoPlayer() && !m_player->contentMIMEType().startsWithIgnoringASCIICase("audio/");
    m_backend.reset(s_factory(s_factoryContext, *this, hasVideo));
    if (!m_backend) {
        setNetworkState(WebCore::MediaPlayer::NetworkState::FormatError);
        return;
    }
    m_backend->open(path.utf8().data());
}

void MediaPlayerPrivateSymbian::mediaPlayerOpened(int error, double duration, int videoWidth, int videoHeight)
{
    // The application calls back from its own event loop; WebCore expects
    // these from its run loop.
    RunLoop::main().dispatch([weakThis = makeWeakPtr(*this), error, duration, videoWidth, videoHeight] {
        if (!weakThis)
            return;
        auto& player = *weakThis;
        if (error) {
            player.m_backend = nullptr;
            player.setNetworkState(WebCore::MediaPlayer::NetworkState::FormatError);
            return;
        }
        player.m_opened = true;
        player.m_duration = duration;
        player.m_videoSize = FloatSize(videoWidth, videoHeight);
        player.m_loadingProgressed = true;
        player.updateVolume();
        player.m_player->durationChanged();
        if (!player.m_videoSize.isEmpty())
            player.m_player->sizeChanged();
        player.m_player->characteristicChanged();
        player.setReadyState(WebCore::MediaPlayer::ReadyState::HaveEnoughData);
        player.setNetworkState(WebCore::MediaPlayer::NetworkState::Loaded);
        if (!player.m_paused) {
            player.m_backend->play();
            player.m_playing = true;
        }
    });
}

void MediaPlayerPrivateSymbian::mediaPlayerEnded(int error)
{
    RunLoop::main().dispatch([weakThis = makeWeakPtr(*this), error] {
        if (!weakThis)
            return;
        auto& player = *weakThis;
        player.m_playing = false;
        if (error) {
            player.setNetworkState(WebCore::MediaPlayer::NetworkState::DecodeError);
            return;
        }
        // HTMLMediaElement sees the end in currentTime, and pauses or loops.
        player.m_ended = true;
        player.m_player->timeChanged();
    });
}

void MediaPlayerPrivateSymbian::play()
{
    m_paused = false;
    if (m_opened && !m_playing) {
        m_backend->play();
        m_playing = true;
    }
    m_player->playbackStateChanged();
}

void MediaPlayerPrivateSymbian::pause()
{
    m_paused = true;
    if (m_playing) {
        m_backend->pause();
        m_playing = false;
    }
    m_player->playbackStateChanged();
}

MediaTime MediaPlayerPrivateSymbian::durationMediaTime() const
{
    return MediaTime::createWithDouble(m_duration);
}

MediaTime MediaPlayerPrivateSymbian::currentMediaTime() const
{
    if (!m_opened)
        return MediaTime::zeroTime();
    if (m_ended)
        return durationMediaTime();
    return MediaTime::createWithDouble(std::min(m_backend->position(), m_duration));
}

void MediaPlayerPrivateSymbian::seek(const MediaTime& time)
{
    if (!m_opened)
        return;
    m_seeking = true;
    m_ended = false;
    m_backend->setPosition(std::max(0.0, std::min(time.toDouble(), m_duration)));
    // A player that reached the end has stopped.
    if (!m_paused && !m_playing) {
        m_backend->play();
        m_playing = true;
    }
    RunLoop::main().dispatch([weakThis = makeWeakPtr(*this)] {
        if (!weakThis)
            return;
        weakThis->m_seeking = false;
        weakThis->m_player->timeChanged();
    });
}

void MediaPlayerPrivateSymbian::setVolume(float volume)
{
    m_volume = volume;
    updateVolume();
}

void MediaPlayerPrivateSymbian::setMuted(bool muted)
{
    m_muted = muted;
    updateVolume();
}

void MediaPlayerPrivateSymbian::updateVolume()
{
    if (m_opened)
        m_backend->setVolume(m_muted ? 0 : m_volume);
}

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateSymbian::buffered() const
{
    if (!m_opened || !m_duration)
        return makeUnique<PlatformTimeRanges>();
    return makeUnique<PlatformTimeRanges>(MediaTime::zeroTime(), durationMediaTime());
}

bool MediaPlayerPrivateSymbian::didLoadingProgress() const
{
    bool progressed = m_loadingProgressed;
    m_loadingProgressed = false;
    return progressed;
}

void MediaPlayerPrivateSymbian::setNetworkState(WebCore::MediaPlayer::NetworkState state)
{
    if (m_networkState == state)
        return;
    m_networkState = state;
    m_player->networkStateChanged();
}

void MediaPlayerPrivateSymbian::setReadyState(WebCore::MediaPlayer::ReadyState state)
{
    if (m_readyState == state)
        return;
    m_readyState = state;
    m_player->readyStateChanged();
}

void MediaPlayerPrivateSymbian::paint(GraphicsContext& context, const FloatRect& rect)
{
    if (context.paintingDisabled() || !hasVideo())
        return;

    // A hole for the video, which plays under the page.
    context.save();
    context.setCompositeOperation(CompositeOperator::Copy);
    context.fillRect(rect, Color::transparentBlack);
    context.restore();

    // The view paints with a transform from the page to view pixels.
    m_paintedVideo = true;
    setVideoRect(enclosingIntRect(context.getCTM().mapRect(rect)));
}

void MediaPlayerPrivateSymbian::setVideoRect(const IntRect& rect)
{
    if (rect == m_videoRect)
        return;
    m_videoRect = rect;
    if (!m_backend)
        return;
    Rect videoRect { rect.x(), rect.y(), rect.width(), rect.height() };
    m_backend->setVideoRect(videoRect, videoRect);
}

void MediaPlayerPrivateSymbian::willPaint()
{
    for (auto* player : players())
        player->m_paintedVideo = false;
}

void MediaPlayerPrivateSymbian::didPaint(const IntRect& area)
{
    // A video in the repainted area that didn't paint is no longer there:
    // scrolled away, hidden or removed.
    for (auto* player : players()) {
        if (!player->m_paintedVideo && player->m_videoRect.intersects(area))
            player->setVideoRect({ });
    }
}

} // namespace WebKitSymbian
