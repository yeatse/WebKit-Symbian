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
        if (m_player)
            m_player->didReceiveResponse(response.expectedContentLength());
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

// How much of a download to have before opening it, and before opening it
// again after the backend ran out of it.
static constexpr unsigned long long minimumBytesToOpen = 64 * 1024;
static constexpr unsigned long long bytesAfterMovie = 64 * 1024;
static constexpr double secondsToResume = 5;

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

bool MediaPlayerPrivateSymbian::createBackend()
{
    // The player for video can't tell an audio file's MIME type from a
    // video's, so only the element and the type choose.
    bool hasVideo = m_player->isVideoPlayer() && !m_player->contentMIMEType().startsWithIgnoringASCIICase("audio/");
    m_backend.reset(s_factory ? s_factory(s_factoryContext, *this, hasVideo) : nullptr);
    if (!m_backend) {
        setNetworkState(WebCore::MediaPlayer::NetworkState::FormatError);
        return false;
    }
    return true;
}

void MediaPlayerPrivateSymbian::startLoading()
{
    if (!createBackend())
        return;

    // A local file opens where it is. Its path is "/C:/...".
    if (m_url.isLocalFile()) {
        m_path = m_url.fileSystemPath();
        if (m_path.length() > 2 && m_path[0] == '/' && m_path[2] == ':')
            m_path = m_path.substring(1);
        m_complete = true;
        open();
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
    m_path = FileSystem::pathByAppendingComponent(mediaDirectory(), name);
    m_isDownload = true;
    if (!m_backend->createFile(m_path.utf8().data())) {
        setNetworkState(WebCore::MediaPlayer::NetworkState::NetworkError);
        return;
    }

    auto type = m_player->contentMIMEType();
    bool isMP4 = equalLettersIgnoringASCIICase(type, "video/mp4") || equalLettersIgnoringASCIICase(type, "audio/mp4")
        || equalLettersIgnoringASCIICase(type, "video/3gpp") || equalLettersIgnoringASCIICase(type, "audio/3gpp")
        || equalLettersIgnoringASCIICase(type, "video/x-m4v") || equalLettersIgnoringASCIICase(type, "audio/x-m4a");
    m_boxes = isMP4 ? Boxes::Scanning : Boxes::None;
    m_openThreshold = minimumBytesToOpen;

    m_resource = loader->requestResource(ResourceRequest(m_url), PlatformMediaResourceLoader::DisallowCaching);
    if (!m_resource) {
        didFailLoading();
        return;
    }
    m_resource->setClient(ResourceClient::create(*this));
}

void MediaPlayerPrivateSymbian::didReceiveResponse(long long expectedContentLength)
{
    m_expectedLength = std::max(expectedContentLength, 0LL);
}

void MediaPlayerPrivateSymbian::didReceiveData(const char* data, int length)
{
    if (!m_backend || !m_backend->appendToFile(data, length)) {
        didFailLoading();
        return;
    }
    scanBoxes(reinterpret_cast<const uint8_t*>(data), length);
    m_bytesReceived += length;
    m_loadingProgressed = true;
    openIfReady();
}

// Follows the top-level boxes, [size:32][type:32][contents], of the data
// that arrives at m_bytesReceived, to see where the 'moov' box ends.
void MediaPlayerPrivateSymbian::scanBoxes(const uint8_t* data, size_t length)
{
    unsigned long long start = m_bytesReceived;
    unsigned long long end = start + length;
    while (m_boxes == Boxes::Scanning && m_nextBox + m_boxHeaderLength < end) {
        // A header can span the chunks.
        unsigned long long from = std::max(start, m_nextBox + m_boxHeaderLength);
        while (m_boxHeaderLength < sizeof(m_boxHeader) && from < end)
            m_boxHeader[m_boxHeaderLength++] = data[from++ - start];
        if (m_boxHeaderLength < sizeof(m_boxHeader))
            return;
        m_boxHeaderLength = 0;

        unsigned long long size = (static_cast<unsigned long long>(m_boxHeader[0]) << 24) | (m_boxHeader[1] << 16) | (m_boxHeader[2] << 8) | m_boxHeader[3];
        const char* type = reinterpret_cast<const char*>(m_boxHeader + 4);
        if (!memcmp(type, "mdat", 4) || size < sizeof(m_boxHeader)) {
            // The data first, or a 64-bit size, which only 'mdat' boxes need.
            m_boxes = Boxes::DataFirst;
            return;
        }
        if (!memcmp(type, "moov", 4)) {
            m_movieEnd = m_nextBox + size;
            m_boxes = Boxes::FoundMovie;
            return;
        }
        m_nextBox += size;
    }
}

// A few seconds of the media at its average bit rate, when that is known.
unsigned long long MediaPlayerPrivateSymbian::bytesToResume() const
{
    if (!m_expectedLength || m_duration <= 0)
        return 4 * minimumBytesToOpen;
    return std::max(minimumBytesToOpen, static_cast<unsigned long long>(m_expectedLength / m_duration * secondsToResume));
}

bool MediaPlayerPrivateSymbian::hasEnoughToOpen() const
{
    if (m_complete)
        return true;
    if (m_stalled)
        return m_bytesReceived >= m_stallBytes + bytesToResume();
    if (m_bytesReceived < m_openThreshold)
        return false;
    switch (m_boxes) {
    case Boxes::None:
        return true;
    case Boxes::FoundMovie:
        return m_bytesReceived >= m_movieEnd + bytesAfterMovie;
    case Boxes::Scanning:
    case Boxes::DataFirst:
        return false;
    }
    return false;
}

void MediaPlayerPrivateSymbian::openIfReady()
{
    if (!m_backend || m_opening || (m_opened && !m_stalled) || !hasEnoughToOpen())
        return;
    open();
}

void MediaPlayerPrivateSymbian::didFinishLoading()
{
    if (m_backend)
        m_backend->closeFile();
    m_resource = nullptr;
    m_complete = true;
    if (m_opened && !m_stalled)
        setNetworkState(WebCore::MediaPlayer::NetworkState::Loaded);
    openIfReady();
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
    setVideoRect({ });
    // This closes the file before it goes.
    m_backend = nullptr;
    if (m_isDownload)
        FileSystem::deleteFile(m_path);
    m_path = String();
    m_isDownload = false;
    m_bytesReceived = 0;
    m_expectedLength = 0;
    m_complete = false;
    m_openThreshold = 0;
    m_bytesAtOpen = 0;
    m_openedPartly = false;
    m_opening = false;
    m_stalled = false;
    m_stallBytes = 0;
    m_resumePosition = 0;
    m_lastPosition = 0;
    m_boxes = Boxes::None;
    m_nextBox = 0;
    m_movieEnd = 0;
    m_boxHeaderLength = 0;
    m_loadDeferred = false;
    m_opened = false;
    m_playing = false;
    m_ended = false;
    m_duration = 0;
    m_videoSize = { };
}

void MediaPlayerPrivateSymbian::open()
{
    m_opening = true;
    m_bytesAtOpen = m_bytesReceived;
    m_openedPartly = !m_complete;
    m_playing = false;
    m_backend->open(m_path.utf8().data());
}

void MediaPlayerPrivateSymbian::mediaPlayerOpened(int error, double duration, int videoWidth, int videoHeight)
{
    // The application calls back from its own event loop; WebCore expects
    // these from its run loop.
    RunLoop::main().dispatch([weakThis = makeWeakPtr(*this), error, duration, videoWidth, videoHeight] {
        if (!weakThis)
            return;
        auto& player = *weakThis;
        player.m_opening = false;
        if (error) {
            if (!player.m_complete) {
                // Not enough of it yet: try again with more.
                player.m_openThreshold = player.m_bytesReceived * 2;
                player.m_stallBytes = player.m_bytesReceived;
                return;
            }
            player.m_opened = false;
            player.setNetworkState(WebCore::MediaPlayer::NetworkState::FormatError);
            return;
        }

        // Other than an MP4 index, what a partial file has tells the duration
        // of only that part. Scale it to the whole, as for a constant bit rate.
        double fullDuration = duration;
        if (player.m_openedPartly && player.m_boxes == Boxes::None && player.m_expectedLength > 0 && player.m_bytesAtOpen)
            fullDuration = duration * player.m_expectedLength / player.m_bytesAtOpen;

        bool wasOpened = player.m_opened;
        player.m_opened = true;
        player.m_stalled = false;
        player.m_loadingProgressed = true;
        if (fullDuration != player.m_duration) {
            player.m_duration = fullDuration;
            player.m_player->durationChanged();
        }
        FloatSize videoSize(videoWidth, videoHeight);
        if (videoSize != player.m_videoSize) {
            player.m_videoSize = videoSize;
            player.m_player->sizeChanged();
        }
        if (!wasOpened)
            player.m_player->characteristicChanged();
        player.updateVolume();
        if (player.m_resumePosition > 0) {
            player.m_backend->setPosition(player.m_resumePosition);
            player.m_lastPosition = player.m_resumePosition;
            player.m_resumePosition = 0;
        }
        player.setReadyState(WebCore::MediaPlayer::ReadyState::HaveEnoughData);
        if (player.m_complete)
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
        // A file opened before it was complete ends where it ended then:
        // wait for more and go on from there.
        if (player.m_openedPartly && (error || player.m_lastPosition < player.m_duration - 1)) {
            player.m_stalled = true;
            player.m_stallBytes = player.m_bytesReceived;
            player.m_resumePosition = player.m_lastPosition;
            player.setReadyState(WebCore::MediaPlayer::ReadyState::HaveCurrentData);
            if (player.m_complete)
                player.setNetworkState(WebCore::MediaPlayer::NetworkState::Loading);
            player.openIfReady();
            return;
        }
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
    if (m_opened && !m_stalled && !m_opening && !m_playing) {
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
    if (m_stalled || m_opening)
        return MediaTime::createWithDouble(m_resumePosition ? m_resumePosition : m_lastPosition);
    m_lastPosition = std::min(m_backend->position(), m_duration);
    return MediaTime::createWithDouble(m_lastPosition);
}

void MediaPlayerPrivateSymbian::seek(const MediaTime& time)
{
    if (!m_opened)
        return;
    m_seeking = true;
    m_ended = false;
    double position = std::max(0.0, std::min(time.toDouble(), m_duration));
    if (m_stalled || m_opening) {
        // For when it opens again.
        m_resumePosition = position;
    } else {
        m_backend->setPosition(position);
        m_lastPosition = position;
        // A player that reached the end has stopped.
        if (!m_paused && !m_playing) {
            m_backend->play();
            m_playing = true;
        }
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
    if (m_complete)
        return makeUnique<PlatformTimeRanges>(MediaTime::zeroTime(), durationMediaTime());
    if (!m_expectedLength)
        return makeUnique<PlatformTimeRanges>();
    // As if the file were played at a constant rate.
    double fraction = std::min(1.0, static_cast<double>(m_bytesReceived) / m_expectedLength);
    return makeUnique<PlatformTimeRanges>(MediaTime::zeroTime(), MediaTime::createWithDouble(m_duration * fraction));
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
