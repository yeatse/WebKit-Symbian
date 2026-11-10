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
#include <WebCore/SharedBuffer.h>
#include <wtf/StdLibExtras.h>
#include <limits>
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
#include <WebCore/TextureMapper.h>
#include <WebCore/TextureMapperPlatformLayer.h>
#endif

namespace WebKitSymbian {

using namespace WebCore;

static bool s_compositedVideoEnabled;
static bool s_compositingActive;

#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
// The contents of a video's composited layer: transparent pixels written
// over everything drawn below, so the native video under the window shows
// there, and layers drawn above still cover it.
class VideoHoleLayer final : public TextureMapperPlatformLayer {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(VideoHoleLayer);
public:
    explicit VideoHoleLayer(MediaPlayerPrivateSymbian& player)
        : m_player(player)
    {
    }

    void paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& rect, const TransformationMatrix& matrix, float) final
    {
        if (!m_player || !m_player->showsHole())
            return;
        // In an intermediate surface (an ancestor with opacity, a filter or a
        // mask), the hole would be drawn over by that surface's own blending,
        // and the surface's coordinates aren't the view's.
        if (textureMapper.currentSurface()) {
            if (!m_loggedSurface) {
                m_loggedSurface = true;
                WTFLogAlways("video: composited inside an intermediate surface, not shown");
            }
            return;
        }
        textureMapper.drawSolidColor(rect, matrix, Color::transparentBlack, false);
        m_player->didPaintHole(enclosingIntRect(matrix.mapRect(rect)));
    }

    void setNeedsDisplay()
    {
        if (auto* client = this->client())
            client->setPlatformLayerNeedsDisplay();
    }

    // The player is going. Its graphics layer drops this layer at the next
    // flush, but the layer tree may still point at it until then.
    void retire()
    {
        m_player = nullptr;
        if (auto* client = this->client())
            client->platformLayerWillBeDestroyed();
    }

private:
    WeakPtr<MediaPlayerPrivateSymbian> m_player;
    bool m_loggedSurface { false };
};

// Hole layers of players that are gone, deleted once a flush has passed.
static Vector<std::unique_ptr<VideoHoleLayer>>& retiredHoleLayers()
{
    static NeverDestroyed<Vector<std::unique_ptr<VideoHoleLayer>>> layers;
    return layers;
}
#endif

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
static const HashSet<String>& mimeTypes()
{
    static NeverDestroyed<HashSet<String>> types(std::initializer_list<String> {
        "audio/3gpp"_s, "audio/aac"_s, "audio/amr"_s, "audio/mp3"_s, "audio/mp4"_s, "audio/mpeg"_s,
        "audio/wav"_s, "audio/x-m4a"_s, "audio/x-wav"_s, "video/3gpp"_s, "video/mp4"_s, "video/x-m4v"_s,
    });
    return types;
}

static bool isSupportedCodec(const String& codec)
{
    // These MMF decoders are candidates until the backend opens the file.
    // Parse codec names, not broad prefixes such as "1" or arbitrary avc1.*.
    auto value = codec.convertToASCIILowercase();
    if (value == "mp3"_s || value == "samr"_s || value == "sawb"_s || value == "1"_s)
        return true;
    if (value == "mp4a.40.2"_s || value == "mp4a.40.5"_s || value == "mp4a.40.29"_s)
        return true;
    if (value == "mp4v.20.9"_s || value == "mp4v.20.8"_s)
        return true;
    if (value.length() != 11 || !value.startsWith("avc1."_s))
        return false;
    auto hex = [](UChar c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); };
    for (unsigned i = 5; i < value.length(); ++i) {
        if (!hex(value[i]))
            return false;
    }
    // Only claim the baseline profile (0x42); actual decoder selection can
    // still fail, so supportsTypeAndCodecs returns MayBeSupported.
    return value.substring(5, 2) == "42"_s;
}

class MediaPlayerFactorySymbian final : public MediaPlayerFactory {
private:
    // Like the hole-punch engine, which this port's would be the only one.
    MediaPlayerEnums::MediaEngineIdentifier identifier() const final { return MediaPlayerEnums::MediaEngineIdentifier::Symbian; }

    Ref<MediaPlayerPrivateInterface> createMediaEnginePlayer(WebCore::MediaPlayer& player) const final
    {
        return adoptRef(*new MediaPlayerPrivateSymbian(player));
    }

    void getSupportedTypes(HashSet<String>& types) const final
    {
        if (s_factory)
            types = mimeTypes();
    }

    WebCore::MediaPlayer::SupportsType supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters) const final
    {
        if (!s_factory || parameters.platformType != PlatformMediaDecodingType::FileOrHLS || !mimeTypes().contains(parameters.type.containerType().convertToASCIILowercase()))
            return WebCore::MediaPlayer::SupportsType::IsNotSupported;
        auto codecs = parameters.type.codecs();
        if (codecs.isEmpty())
            return WebCore::MediaPlayer::SupportsType::MayBeSupported;
        for (auto& codec : codecs) {
            if (!isSupportedCodec(codec))
                return WebCore::MediaPlayer::SupportsType::IsNotSupported;
        }
        return WebCore::MediaPlayer::SupportsType::MayBeSupported;
    }
};

void MediaPlayerPrivateSymbian::registerMediaEngine(const String& directory)
{
    mediaDirectory() = FileSystem::pathByAppendingComponent(directory, "MediaCache"_s);
    // Clean only files owned by this download cache. A folder named Media
    // may contain local source files and must never be swept at startup.
    for (auto& name : FileSystem::listDirectory(mediaDirectory())) {
        if (name.startsWith("webkit-media-"_s))
            FileSystem::deleteFile(FileSystem::pathByAppendingComponent(mediaDirectory(), name));
    }
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
        : m_player(WeakPtr { player })
        , m_generation(player.loadGeneration())
    {
    }

    void responseReceived(PlatformMediaResource&, const ResourceResponse& response, CompletionHandler<void(ShouldContinuePolicyCheck)>&& completionHandler) final
    {
        if (response.httpStatusCode() >= 400) {
            completionHandler(ShouldContinuePolicyCheck::No);
            if (m_player && m_player->isCurrentLoad(m_generation))
                m_player->didFailLoading();
            return;
        }
        if (m_player && m_player->isCurrentLoad(m_generation))
            m_player->didReceiveResponse(response.expectedContentLength());
        completionHandler(ShouldContinuePolicyCheck::Yes);
    }

    // The data goes to a file, not the memory cache.
    bool shouldCacheResponse(PlatformMediaResource&, const ResourceResponse&) final { return false; }

    void dataReceived(PlatformMediaResource&, const SharedBuffer& data) final
    {
        if (m_player && m_player->isCurrentLoad(m_generation))
            m_player->didReceiveData(data.span());
    }

    void accessControlCheckFailed(PlatformMediaResource&, const ResourceError&) final
    {
        if (m_player && m_player->isCurrentLoad(m_generation))
            m_player->didFailLoading();
    }

    void loadFailed(PlatformMediaResource&, const ResourceError&) final
    {
        if (m_player && m_player->isCurrentLoad(m_generation))
            m_player->didFailLoading();
    }

    void loadFinished(PlatformMediaResource&, const NetworkLoadMetrics&) final
    {
        if (m_player && m_player->isCurrentLoad(m_generation))
            m_player->didFinishLoading();
    }

    WeakPtr<MediaPlayerPrivateSymbian> m_player;
    uint64_t m_generation;
};

class NativeClient final : public MediaPlayerClient {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(NativeClient);
public:
    explicit NativeClient(MediaPlayerPrivateSymbian& player)
        : m_player(player), m_generation(player.loadGeneration()) { }
    void mediaPlayerOpened(int error, double duration, int width, int height, bool audio) final
    {
        if (m_player && m_player->isCurrentLoad(m_generation))
            static_cast<MediaPlayerClient&>(*m_player).mediaPlayerOpened(error, duration, width, height, audio);
    }
    void mediaPlayerEnded(int error) final
    {
        if (m_player && m_player->isCurrentLoad(m_generation))
            static_cast<MediaPlayerClient&>(*m_player).mediaPlayerEnded(error);
    }
private:
    WeakPtr<MediaPlayerPrivateSymbian> m_player;
    uint64_t m_generation;
};

} // namespace

// How much of a download to have before opening it, and before opening it
// again after the backend ran out of it.
static constexpr unsigned long long minimumBytesToOpen = 64 * 1024;
static constexpr unsigned long long bytesAfterMovie = 64 * 1024;
static constexpr double secondsToResume = 5;

MediaPlayerPrivateSymbian::MediaPlayerPrivateSymbian(WebCore::MediaPlayer& player)
    : m_player(player)
{
    players().add(this);
}

MediaPlayerPrivateSymbian::~MediaPlayerPrivateSymbian()
{
    players().remove(this);
    cancelLoad();
#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
    if (m_holeLayer) {
        m_holeLayer->retire();
        retiredHoleLayers().append(WTF::move(m_holeLayer));
    }
#endif
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
    RefPtr player = m_player.get();
    if (!player)
        return false;
    bool hasVideo = player->isVideoPlayer() && !player->contentMIMEType().startsWithIgnoringASCIICase("audio/"_s);
    m_backendClient = makeUnique<NativeClient>(*this);
    m_backend.reset(s_factory ? s_factory(s_factoryContext, *m_backendClient, hasVideo) : nullptr);
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
    if (m_url.protocolIsFile()) {
        m_path = m_url.fileSystemPath();
        if (m_path.length() > 2 && m_path[0] == '/' && m_path[2] == ':')
            m_path = m_path.substring(1);
        m_complete = true;
        open();
        return;
    }

    RefPtr player = m_player.get();
    if (!player) {
        setNetworkState(WebCore::MediaPlayer::NetworkState::NetworkError);
        return;
    }

    Ref loader = player->mediaResourceLoader();

    // Keep the extension, which the MMF uses in choosing a plug-in.
    static unsigned fileNumber;
    String name = makeString("webkit-media-"_s, ++fileNumber);
    auto lastComponent = m_url.lastPathComponent().toString();
    size_t dot = lastComponent.reverseFind('.');
    if (dot != notFound && lastComponent.length() - dot <= 5)
        name = makeString(name, lastComponent.substring(dot));
    FileSystem::makeAllDirectories(mediaDirectory());
    m_path = FileSystem::pathByAppendingComponent(mediaDirectory(), name);
    m_isDownload = true;
    if (m_backend->createFile(m_path.utf8().data())) {
        setNetworkState(WebCore::MediaPlayer::NetworkState::NetworkError);
        return;
    }

    auto type = player->contentMIMEType();
    bool isMP4 = equalLettersIgnoringASCIICase(type, "video/mp4") || equalLettersIgnoringASCIICase(type, "audio/mp4")
        || equalLettersIgnoringASCIICase(type, "video/3gpp") || equalLettersIgnoringASCIICase(type, "audio/3gpp")
        || equalLettersIgnoringASCIICase(type, "video/x-m4v") || equalLettersIgnoringASCIICase(type, "audio/x-m4a");
    m_boxes = isMP4 ? Boxes::Scanning : Boxes::None;
    m_openThreshold = minimumBytesToOpen;

    m_resource = loader->requestResource(ResourceRequest { URL { m_url } }, PlatformMediaResourceLoader::DisallowCaching);
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

void MediaPlayerPrivateSymbian::didReceiveData(std::span<const uint8_t> data)
{
    if (!m_backend || data.size() > std::numeric_limits<unsigned>::max()
        || m_backend->appendToFile(reinterpret_cast<const char*>(data.data()), data.size())) {
        didFailLoading();
        return;
    }
    scanBoxes(data.data(), data.size());
    m_bytesReceived += data.size();
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
    bool hadVideo = hasVideo();
    ++m_loadGeneration;
    if (m_resource) {
        m_resource->setClient(nullptr);
        m_resource->shutdown();
        m_resource = nullptr;
    }
    setVideoRect({ });
    // This closes the file before it goes.
    m_backend = nullptr;
    m_backendClient = nullptr;
    holeChanged();
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
    m_positionErrorReported = false;
    m_boxes = Boxes::None;
    m_nextBox = 0;
    m_movieEnd = 0;
    m_boxHeaderLength = 0;
    m_loadDeferred = false;
    m_opened = false;
    m_hasAudio = false;
    m_seekPromise.reset();
    m_buffered = { };
    m_playing = false;
    m_ended = false;
    m_duration = 0;
    m_videoSize = { };
    if (hadVideo) {
        if (RefPtr player = m_player.get())
            player->repaint();
    }
}

void MediaPlayerPrivateSymbian::open()
{
    if (m_isDownload && m_backend->flushFile()) {
        didFailLoading();
        return;
    }
    m_opening = true;
    m_bytesAtOpen = m_bytesReceived;
    m_openedPartly = !m_complete;
    m_playing = false;
    m_backend->open(m_path.utf8().data());
}

void MediaPlayerPrivateSymbian::mediaPlayerOpened(int error, double duration, int videoWidth, int videoHeight, bool hasAudio)
{
    // The application calls back from its own event loop; WebCore expects
    // these from its run loop.
    RunLoop::mainSingleton().dispatch([weakThis = WeakPtr { *this }, generation = m_loadGeneration, error, duration, videoWidth, videoHeight, hasAudio] {
        if (!weakThis || !weakThis->isCurrentLoad(generation))
            return;
        Ref playerRef { *weakThis };
        auto& player = playerRef.get();
        RefPtr corePlayer = player.m_player.get();
        if (!corePlayer)
            return;
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
        if (!std::isfinite(duration) || duration < 0 || videoWidth < 0 || videoHeight < 0) {
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
        player.m_hasAudio = hasAudio;
        player.m_stalled = false;
        player.m_loadingProgressed = true;
        if (fullDuration != player.m_duration) {
            player.m_duration = fullDuration;
            corePlayer->durationChanged();
            if (!player.isCurrentLoad(generation))
                return;
        }
        FloatSize videoSize(videoWidth, videoHeight);
        if (videoSize != player.m_videoSize) {
            player.m_videoSize = videoSize;
            corePlayer->sizeChanged();
            if (!player.isCurrentLoad(generation))
                return;
        }
        player.holeChanged();
        if (!wasOpened)
            corePlayer->characteristicChanged();
        if (!player.isCurrentLoad(generation))
            return;
        player.updateVolume();
        if (!player.isCurrentLoad(generation) || !player.m_backend)
            return;
        if (player.m_resumePosition > 0 || player.m_seekPromise) {
            if (!player.handleNativeError(player.m_backend->setPosition(player.m_resumePosition)))
                return;
            player.m_lastPosition = player.m_resumePosition;
            player.m_resumePosition = 0;
        }
        player.setReadyState(player.m_complete ? WebCore::MediaPlayer::ReadyState::HaveEnoughData : WebCore::MediaPlayer::ReadyState::HaveFutureData);
        if (!player.isCurrentLoad(generation))
            return;
        if (player.m_complete)
            player.setNetworkState(WebCore::MediaPlayer::NetworkState::Loaded);
        if (!player.isCurrentLoad(generation) || !player.m_backend)
            return;
        // readyStateChanged may synchronously enter play() when the element
        // requested playback before metadata arrived. Do not start MMF twice.
        if (!player.m_paused && !player.m_playing) {
            if (!player.handleNativeError(player.m_backend->play()))
                return;
            player.m_playing = true;
        }
        if (auto seekPromise = std::exchange(player.m_seekPromise, std::nullopt))
            seekPromise->resolve(MediaTime::createWithDouble(player.m_lastPosition));
    });
}

void MediaPlayerPrivateSymbian::mediaPlayerEnded(int error)
{
    RunLoop::mainSingleton().dispatch([weakThis = WeakPtr { *this }, generation = m_loadGeneration, error] {
        if (!weakThis || !weakThis->isCurrentLoad(generation))
            return;
        Ref playerRef { *weakThis };
        auto& player = playerRef.get();
        RefPtr corePlayer = player.m_player.get();
        if (!corePlayer)
            return;
        player.m_playing = false;
        // A file opened before it was complete ends where it ended then:
        // wait for more and go on from there.
        if (player.m_openedPartly && (!player.m_complete || error || player.m_lastPosition < player.m_duration - 1)) {
            player.m_stalled = true;
            player.m_stallBytes = player.m_bytesReceived;
            player.m_resumePosition = player.m_lastPosition;
            player.setReadyState(WebCore::MediaPlayer::ReadyState::HaveCurrentData);
            if (!player.isCurrentLoad(generation))
                return;
            if (player.m_complete)
                player.setNetworkState(WebCore::MediaPlayer::NetworkState::Loading);
            if (!player.isCurrentLoad(generation))
                return;
            player.openIfReady();
            return;
        }
        if (error) {
            player.setNetworkState(WebCore::MediaPlayer::NetworkState::DecodeError);
            return;
        }
        // HTMLMediaElement sees the end in currentTime, and pauses or loops.
        player.m_ended = true;
        corePlayer->timeChanged();
    });
}

void MediaPlayerPrivateSymbian::play()
{
    m_paused = false;
    if (m_opened && !m_stalled && !m_opening && !m_playing) {
        if (!handleNativeError(m_backend->play()))
            return;
        m_playing = true;
    }
    if (RefPtr player = m_player.get())
        player->playbackStateChanged();
}

void MediaPlayerPrivateSymbian::pause()
{
    m_paused = true;
    if (m_playing) {
        if (!handleNativeError(m_backend->pause()))
            return;
        m_playing = false;
    }
    if (RefPtr player = m_player.get())
        player->playbackStateChanged();
}

MediaTime MediaPlayerPrivateSymbian::duration() const
{
    return MediaTime::createWithDouble(m_duration);
}

MediaTime MediaPlayerPrivateSymbian::currentTime() const
{
    if (!m_opened)
        return MediaTime::zeroTime();
    if (m_ended)
        return duration();
    if (m_stalled || m_opening)
        return MediaTime::createWithDouble(m_resumePosition ? m_resumePosition : m_lastPosition);
    double position = m_lastPosition;
    int error = m_backend->position(position);
    if (!error && std::isfinite(position))
        m_lastPosition = std::clamp(position, 0.0, m_duration);
    else if (!m_positionErrorReported) {
        m_positionErrorReported = true;
        RunLoop::mainSingleton().dispatch([weakThis = WeakPtr { const_cast<MediaPlayerPrivateSymbian&>(*this) }, generation = m_loadGeneration, error] {
            if (weakThis && weakThis->isCurrentLoad(generation))
                weakThis->handleNativeError(error ? error : -6);
        });
    }
    return MediaTime::createWithDouble(m_lastPosition);
}

Ref<MediaTimePromise> MediaPlayerPrivateSymbian::seekToTarget(const SeekTarget& target)
{
    if (!m_opened)
        return MediaTimePromise::createAndReject(PlatformMediaError::InvalidState);

    m_seekPromise.emplace(PlatformMediaError::Cancelled);
    Ref promise = m_seekPromise->promise();
    m_ended = false;
    double position = std::clamp(target.time.toDouble(), 0.0, m_duration);
    if (m_stalled || m_opening) {
        m_resumePosition = position;
        return promise;
    }
    if (!handleNativeError(m_backend->setPosition(position)))
        return promise;
    m_lastPosition = position;
    if (!m_paused && !m_playing) {
        if (!handleNativeError(m_backend->play()))
            return promise;
        m_playing = true;
    }
    if (auto seekPromise = std::exchange(m_seekPromise, std::nullopt))
        seekPromise->resolve(MediaTime::createWithDouble(position));
    return promise;
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
        handleNativeError(m_backend->setVolume(m_muted ? 0 : m_volume));
}

const PlatformTimeRanges& MediaPlayerPrivateSymbian::buffered() const
{
    m_buffered = { };
    if (!m_opened || !m_duration)
        return m_buffered;
    if (m_complete)
        m_buffered.add(MediaTime::zeroTime(), duration());
    else if (m_expectedLength) {
        // A byte-fraction estimate, not random access to bytes that have not
        // arrived. Native seeks still report their real errors.
        double fraction = std::min(1.0, static_cast<double>(m_bytesReceived) / m_expectedLength);
        m_buffered.add(MediaTime::zeroTime(), MediaTime::createWithDouble(m_duration * fraction));
    }
    return m_buffered;
}

MediaTime MediaPlayerPrivateSymbian::maxTimeSeekable() const
{
    if (m_complete)
        return duration();
    if (!m_expectedLength)
        return MediaTime::zeroTime();
    return MediaTime::createWithDouble(m_duration * std::min(1.0, static_cast<double>(m_bytesReceived) / m_expectedLength));
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
    if (state == WebCore::MediaPlayer::NetworkState::FormatError
        || state == WebCore::MediaPlayer::NetworkState::NetworkError
        || state == WebCore::MediaPlayer::NetworkState::DecodeError)
        m_seekPromise.reset();
    if (RefPtr player = m_player.get())
        player->networkStateChanged();
}

void MediaPlayerPrivateSymbian::setReadyState(WebCore::MediaPlayer::ReadyState state)
{
    if (m_readyState == state)
        return;
    m_readyState = state;
    if (RefPtr player = m_player.get())
        player->readyStateChanged();
}

void MediaPlayerPrivateSymbian::paint(GraphicsContext& context, const FloatRect& rect)
{
    if (context.paintingDisabled() || !hasVideo() || !m_pageVisible)
        return;

    // A hole for the video, which plays under the page.
    context.save();
    context.setCompositeOperation(CompositeOperator::Copy);
    context.fillRect(rect, Color::transparentBlack);
    context.restore();

    // The view paints with a transform from the page to view pixels.
    m_paintedVideo = true;
    setVideoRect(enclosingIntRect(context.getCTM().mapRect(rect)),
        enclosingIntRect(context.getCTM().mapRect(intersection(rect, FloatRect(context.clipBounds())))));
}

bool MediaPlayerPrivateSymbian::handleNativeError(int error)
{
    if (!error)
        return true;
    cancelLoad();
    setReadyState(WebCore::MediaPlayer::ReadyState::HaveNothing);
    setNetworkState(WebCore::MediaPlayer::NetworkState::DecodeError);
    return false;
}

void MediaPlayerPrivateSymbian::holeChanged()
{
#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
    if (m_holeLayer)
        m_holeLayer->setNeedsDisplay();
#endif
}

void MediaPlayerPrivateSymbian::setPageIsVisible(bool visible)
{
    m_pageVisible = visible;
    holeChanged();
    if (!visible)
        setVideoRect({ });
    if (RefPtr player = m_player.get())
        player->repaint();
}

void MediaPlayerPrivateSymbian::setVideoRect(const IntRect& rect, const IntRect& clip)
{
    if (rect == m_videoRect && clip == m_videoClip)
        return;
    m_videoRect = rect;
    m_videoClip = clip;
    if (!m_backend)
        return;
    Rect videoRect { rect.x(), rect.y(), rect.width(), rect.height() };
    Rect videoClip { clip.x(), clip.y(), clip.width(), clip.height() };
    // A display failure must reach HTMLMediaElement, not disappear in TRAP_IGNORE.
    int error = m_backend->setVideoRect(videoRect, videoClip);
    if (error && !rect.isEmpty())
        handleNativeError(error);
}

void MediaPlayerPrivateSymbian::willPaint()
{
    for (auto* player : players())
        player->m_paintedVideo = false;
}

bool MediaPlayerPrivateSymbian::requiresNativeVideoWindow()
{
    for (auto* player : players()) {
        if (player->showsHole())
            return true;
    }
    return false;
}

bool MediaPlayerPrivateSymbian::showsHole() const
{
    return m_backend && m_opened && m_pageVisible && hasVideo();
}

void MediaPlayerPrivateSymbian::setCompositedVideoEnabled(bool enabled)
{
    s_compositedVideoEnabled = enabled;
}

bool MediaPlayerPrivateSymbian::compositedVideoEnabled()
{
    return s_compositedVideoEnabled;
}

void MediaPlayerPrivateSymbian::setCompositingActive(bool active)
{
    if (s_compositingActive == active)
        return;
    bool wasAccelerated = s_compositedVideoEnabled && s_compositingActive;
    s_compositingActive = active;
    if (wasAccelerated == (s_compositedVideoEnabled && s_compositingActive))
        return;
    // Videos change between a composited layer and painting a hole.
    for (auto* player : players()) {
        player->setVideoRect({ });
        if (RefPtr webCorePlayer = player->m_player.get())
            webCorePlayer->renderingModeChanged();
    }
}

bool MediaPlayerPrivateSymbian::supportsAcceleratedRendering() const
{
#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
    return s_compositedVideoEnabled && s_compositingActive;
#else
    return false;
#endif
}

PlatformLayer* MediaPlayerPrivateSymbian::platformLayer() const
{
#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
    if (!supportsAcceleratedRendering())
        return nullptr;
    if (!m_holeLayer)
        m_holeLayer = makeUnique<VideoHoleLayer>(const_cast<MediaPlayerPrivateSymbian&>(*this));
    return m_holeLayer.get();
#else
    return nullptr;
#endif
}

void MediaPlayerPrivateSymbian::willComposite()
{
#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
    // Layers were flushed for this frame: none points at a retired one.
    retiredHoleLayers().clear();
    for (auto* player : players())
        player->m_compositedHole = std::nullopt;
#endif
}

void MediaPlayerPrivateSymbian::didPaintHole(const IntRect& rect)
{
#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
    // A video drawn twice (a reflection) keeps its first place.
    if (!m_compositedHole)
        m_compositedHole = rect;
#else
    UNUSED_PARAM(rect);
#endif
}

bool MediaPlayerPrivateSymbian::didComposite(const IntRect& viewBounds)
{
    bool anyHole = false;
#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
    for (auto* player : players()) {
        if (!player->supportsAcceleratedRendering())
            continue;
        IntRect clip = player->m_compositedHole ? intersection(*player->m_compositedHole, viewBounds) : IntRect();
        if (clip.isEmpty()) {
            player->setVideoRect({ });
            continue;
        }
        anyHole = true;
        // Only the hole is transparent, so content over the video covers it
        // without clipping the native video to it.
        player->setVideoRect(*player->m_compositedHole, clip);
    }
#else
    UNUSED_PARAM(viewBounds);
#endif
    return anyHole;
}

void MediaPlayerPrivateSymbian::didPaint(const IntRect& area)
{
    // A video in the repainted area that didn't paint is no longer there:
    // scrolled away, hidden or removed.
    for (auto* player : players()) {
        if (player->supportsAcceleratedRendering())
            continue;
        if (!player->m_paintedVideo && player->m_videoRect.intersects(area))
            player->setVideoRect({ });
    }
}

} // namespace WebKitSymbian
