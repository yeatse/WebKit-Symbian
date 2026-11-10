/*
 Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "TextureMapperTiledBackingStore.h"

#include "BitmapTexture.h"
#include "BitmapTexturePool.h"
#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "ImageBuffer.h"
#include "NativeImage.h"
#include "ImageObserver.h"
#include "TextureMapper.h"
#if OS(SYMBIAN) && USE(SKIA)
#include "GLContext.h"
#include "GraphicsContextSkia.h"
#include "PlatformDisplay.h"
#include "SkiaBackingStore.h"
#include "SkiaNativeTile.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkPictureRecorder.h>
#include <skia/core/SkPicture.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#endif
#include <wtf/MonotonicTime.h>
#include <wtf/NeverDestroyed.h>
#include <algorithm>

namespace WebCore {

static size_t s_coverageBytes;
static constexpr int coverageTileSize = 256;
// Tiles are as wide as the layer up to this: a repaint across the view then
// covers whole tile rows, which upload straight from the painted buffer.
// Without EXT_unpack_subimage a part of a row is copied out first.
static constexpr int coverageMaxTileWidth = 1024;
// Margin tiles painted per frame across all layers, so a frame that scrolls
// far does not wait for a whole new screen of them. Each tile is a raster
// paint of up to 1024x256 pixels and its upload, tens of milliseconds on
// ARM11, so a frame with many layers must not paint two for each.
static constexpr unsigned coveragePrefetchPerFrame = 2;
// A frame whose on-screen tiles took this long already misses its vsync:
// margins wait for the next one.
static constexpr Seconds coveragePrefetchSkipAfter = 16_ms;
static unsigned s_prefetchLeft = coveragePrefetchPerFrame;
static Seconds s_frameVisibleTime;
static bool s_frameSkippedPrefetch;
// Frames in a row that skipped margins: a page that repaints slowly every
// frame still gets them, after a short wait.
static unsigned s_skippedFrameRun;
static constexpr unsigned coveragePrefetchMaxSkippedFrames = 2;

TextureMapperTiledBackingStore::CoverageStats& TextureMapperTiledBackingStore::coverageStats()
{
    static NeverDestroyed<CoverageStats> stats;
    return stats;
}

void TextureMapperTiledBackingStore::beginCoverageFrame()
{
    ++coverageStats().frames;
    s_prefetchLeft = coveragePrefetchPerFrame;
    s_skippedFrameRun = s_frameSkippedPrefetch ? s_skippedFrameRun + 1 : 0;
    s_frameVisibleTime = { };
    s_frameSkippedPrefetch = false;
}

static size_t coverageTextureBytes(const IntSize& size)
{
    return static_cast<size_t>(size.width()) * size.height() * 4;
}

TextureMapperTiledBackingStore::~TextureMapperTiledBackingStore()
{
    clearCoverageTiles();
}

void TextureMapperTiledBackingStore::setContentsToImage(Image* image)
{
    m_image = image;
#if OS(SYMBIAN) && USE(SKIA)
    auto nativeImage = image ? image->currentNativeImage() : nullptr;
    m_skiaContentsImage = nativeImage ? nativeImage->platformImage() : nullptr;
#endif
}

size_t TextureMapperTiledBackingStore::coverageBytes()
{
    return s_coverageBytes;
}

void TextureMapperTiledBackingStore::updateContentsFromImageIfNeeded(TextureMapper& textureMapper)
{
    if (!m_image)
        return;

    updateContents(textureMapper, m_image.get(), m_image->size(), enclosingIntRect(m_image->rect()));

    if (auto observer = m_image->imageObserver())
        observer->didDraw(*m_image);
    m_image = nullptr;
}

TransformationMatrix TextureMapperTiledBackingStore::adjustedTransformForRect(const FloatRect& targetRect)
{
    return TransformationMatrix::rectToRect(rect(), targetRect);
}

void TextureMapperTiledBackingStore::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& transform, float opacity)
{
    updateContentsFromImageIfNeeded(textureMapper);
    TransformationMatrix adjustedTransform = transform * adjustedTransformForRect(targetRect);
    for (auto& tile : m_tiles)
        tile.paint(textureMapper, adjustedTransform, opacity, allTileEdgesExposed(rect(), tile.rect()));
    for (auto& coverageTile : m_coverageTiles)
        coverageTile.tile.paint(textureMapper, adjustedTransform, opacity, allTileEdgesExposed(rect(), coverageTile.tile.rect()));
}

#if OS(SYMBIAN) && USE(SKIA)
sk_sp<SkImage> TextureMapperTiledBackingStore::skiaImage()
{
    if (m_image) {
        if (auto observer = m_image->imageObserver())
            observer->didDraw(*m_image);
    }
    return m_skiaContentsImage;
}

bool TextureMapperTiledBackingStore::updateSkiaBackingStore(SkiaBackingStore& backingStore, TextureMapper& textureMapper)
{
    updateContentsFromImageIfNeeded(textureMapper);
    Vector<SkiaNativeTile> tiles;
    auto appendTile = [&](const TextureMapperTile& tile) {
        auto texture = tile.texture();
        if (!texture)
            return;
        auto rect = tile.rect();
        rect.scale(1 / m_contentsScale);
        tiles.append({ WTF::move(texture), rect });
    };
    auto tileSize = textureMapper.maxTextureSize();
    for (auto& tile : m_tiles) {
        auto tileRect = tile.rect();
        if (tileRect.x() >= m_skiaTileGridSize.width() || tileRect.y() >= m_skiaTileGridSize.height())
            continue;
        FloatRect activeRect(tileRect.x(), tileRect.y(), tileSize.width(), tileSize.height());
        activeRect.intersect(rect());
        if (tileRect == activeRect)
            appendTile(tile);
    }
    for (auto& tile : m_coverageTiles) {
        if (tile.valid)
            appendTile(tile.tile);
    }
    backingStore.setNativeTiles(m_size, m_contentsScale, WTF::move(tiles));
    return true;
}

void TextureMapperTiledBackingStore::addSkiaDamage(const IntRect& scaledRect)
{
    if (scaledRect.isEmpty())
        return;
    FloatRect rect(scaledRect);
    rect.scale(1 / m_contentsScale);
    m_skiaDamage.append(enclosingIntRect(rect));
}
#endif

void TextureMapperTiledBackingStore::drawBorder(TextureMapper& textureMapper, const Color& borderColor, float borderWidth, const FloatRect& targetRect, const TransformationMatrix& transform)
{
    TransformationMatrix adjustedTransform = transform * adjustedTransformForRect(targetRect);
    for (auto& tile : m_tiles)
        textureMapper.drawBorder(borderColor, borderWidth, tile.rect(), adjustedTransform);
    for (auto& coverageTile : m_coverageTiles)
        textureMapper.drawBorder(borderColor, borderWidth, coverageTile.tile.rect(), adjustedTransform);
}

void TextureMapperTiledBackingStore::drawRepaintCounter(TextureMapper& textureMapper, int repaintCount, const Color& borderColor, const FloatRect& targetRect, const TransformationMatrix& transform)
{
    TransformationMatrix adjustedTransform = transform * adjustedTransformForRect(targetRect);
    for (auto& tile : m_tiles)
        textureMapper.drawNumber(repaintCount, borderColor, tile.rect().location(), adjustedTransform);
    for (auto& coverageTile : m_coverageTiles)
        textureMapper.drawNumber(repaintCount, borderColor, coverageTile.tile.rect().location(), adjustedTransform);
}

void TextureMapperTiledBackingStore::updateContentsScale(float scale)
{
    if (m_contentsScale == scale)
        return;

    m_isScaleDirty = true;
    m_contentsScale = scale;
}

void TextureMapperTiledBackingStore::createOrDestroyTilesIfNeeded(const FloatSize& size, const IntSize& tileSize, bool hasAlpha)
{
    if (size == m_size && !m_isScaleDirty)
        return;

    m_size = size;
    m_isScaleDirty = false;

    FloatSize scaledSize(m_size);
    if (!m_image)
        scaledSize.scale(m_contentsScale);
#if OS(SYMBIAN) && USE(SKIA)
    m_skiaTileGridSize = scaledSize;
#endif

    Vector<FloatRect> tileRectsToAdd;
    Vector<int> tileIndicesToRemove;
    static const size_t TileEraseThreshold = 6;

    // This method recycles tiles. We check which tiles we need to add, which to remove, and use as many
    // removable tiles as replacement for new tiles when possible.
    for (float y = 0; y < scaledSize.height(); y += tileSize.height()) {
        for (float x = 0; x < scaledSize.width(); x += tileSize.width()) {
            FloatRect tileRect(x, y, tileSize.width(), tileSize.height());
            tileRect.intersect(rect());
            tileRectsToAdd.append(tileRect);
        }
    }

    // Check which tiles need to be removed, and which already exist.
    for (int i = m_tiles.size() - 1; i >= 0; --i) {
        FloatRect oldTile = m_tiles[i].rect();
        bool existsAlready = false;

        for (int j = tileRectsToAdd.size() - 1; j >= 0; --j) {
            FloatRect newTile = tileRectsToAdd[j];
            if (oldTile != newTile)
                continue;

            // A tile that we want to add already exists, no need to add or remove it.
            existsAlready = true;
            tileRectsToAdd.removeAt(j);
            break;
        }

        // This tile is not needed.
        if (!existsAlready)
            tileIndicesToRemove.append(i);
    }

    // Recycle removable tiles to be used for newly requested tiles.
    for (auto& rect : tileRectsToAdd) {
        if (!tileIndicesToRemove.isEmpty()) {
            // We recycle an existing tile for usage with a new tile rect.
            TextureMapperTile& tile = m_tiles[tileIndicesToRemove.last()];
            tileIndicesToRemove.removeLast();
            tile.setRect(rect);

            if (tile.texture()) {
                OptionSet<BitmapTexture::Flags> flags;
                if (hasAlpha)
                    flags.add(BitmapTexture::Flags::SupportsAlpha);
                tile.texture()->reset(enclosingIntRect(tile.rect()).size(), flags);
            }
            continue;
        }

        m_tiles.append(TextureMapperTile(rect));
    }

    // Remove unnecessary tiles, if they weren't recycled.
    // We use a threshold to make sure we don't create/destroy tiles too eagerly.
    for (auto& index : tileIndicesToRemove) {
        if (m_tiles.size() <= TileEraseThreshold)
            break;
        m_tiles.removeAt(index);
    }
}

void TextureMapperTiledBackingStore::updateContents(TextureMapper& textureMapper, Image* image, const FloatSize& totalSize, const IntRect& dirtyRect)
{
    createOrDestroyTilesIfNeeded(totalSize, textureMapper.maxTextureSize(), !image->currentFrameKnownToBeOpaque());
    for (auto& tile : m_tiles)
        tile.updateContents(image, dirtyRect);
#if OS(SYMBIAN) && USE(SKIA)
    addSkiaDamage(dirtyRect);
#endif
}

bool TextureMapperTiledBackingStore::updateContents(TextureMapper& textureMapper, GraphicsLayer* sourceLayer, const FloatSize& totalSize, const IntRect& dirtyRect)
{
    if (m_coverageMode) {
        // The whole layer is tiled again and painted: the caller passes its
        // whole rect as dirty when it leaves coverage mode.
        clearCoverageTiles();
        m_coverageMode = false;
        m_isScaleDirty = true;
    }
    createOrDestroyTilesIfNeeded(totalSize, textureMapper.maxTextureSize(), true);
    for (auto& tile : m_tiles) {
        if (!tile.updateContents(sourceLayer, dirtyRect, m_contentsScale))
            return false;
    }
#if OS(SYMBIAN) && USE(SKIA)
    addSkiaDamage(dirtyRect);
#endif
    return true;
}

void TextureMapperTiledBackingStore::clearCoverageTiles()
{
    s_coverageBytes -= m_coverageBytes;
    m_coverageBytes = 0;
    m_coverageTiles.clear();
}

void TextureMapperTiledBackingStore::removeCoverageTile(size_t index)
{
    auto& coverageTile = m_coverageTiles[index];
    if (coverageTile.tile.texture()) {
        size_t bytes = coverageTextureBytes(enclosingIntRect(coverageTile.tile.rect()).size());
        m_coverageBytes -= bytes;
        s_coverageBytes -= bytes;
    }
    m_coverageTiles.removeAt(index);
}

bool TextureMapperTiledBackingStore::paintCoverageTiles(const Vector<CoveragePaint>& paints, GraphicsLayer* sourceLayer)
{
    // Paint the union of the rects once and upload each tile's part, rather
    // than painting the layer once per tile: every tile a text line crosses
    // would shape and draw it again.
    if (paints.isEmpty())
        return true;
    IntRect area;
    uint64_t paintArea = 0;
    for (auto& entry : paints) {
        area.unite(entry.rect);
        paintArea += entry.rect.area().value();
    }
    if (paints.size() > 1 && area.area().value() > 2 * paintArea) {
        // Damage far apart (a caret and a clock): paint each part alone.
        for (auto& entry : paints) {
            if (!paintCoverageTiles({ entry }, sourceLayer))
                return false;
        }
        return true;
    }
    for (auto& entry : paints) {
        auto& coverageTile = m_coverageTiles[entry.index];
        if (!coverageTile.tile.texture()) {
            IntSize size = enclosingIntRect(coverageTile.tile.rect()).size();
            auto texture = BitmapTexturePool::singleton().acquireTexture(size, { BitmapTexture::Flags::SupportsAlpha });
            coverageTile.tile.setTexture(texture.ptr());
            size_t bytes = coverageTextureBytes(size);
            m_coverageBytes += bytes;
            s_coverageBytes += bytes;
        }
    }

#if OS(SYMBIAN) && USE(SKIA)
    bool canRasterize = std::ranges::all_of(paints, [&](auto& entry) {
        auto& tile = m_coverageTiles[entry.index].tile;
        auto target = entry.rect;
        auto tileRect = enclosingIntRect(tile.rect());
        target.move(-tileRect.x(), -tileRect.y());
        return tile.texture()->canRasterizeWithSkia(target);
    });
    if (canRasterize) {
        SkPictureRecorder recorder;
        auto* skiaContext = PlatformDisplay::sharedDisplay().skiaGLContext();
        sk_sp<SkPicture> picture;
        {
            GLContext::ScopedGLContextCurrent current(*skiaContext);
            {
                GraphicsContextSkia context(*recorder.beginRecording(area.width(), area.height()), RenderingMode::Accelerated, RenderingPurpose::DOM);
                context.setTextDrawingMode(TextDrawingMode::Fill);
                IntRect sourceRect(area);
                sourceRect.scale(1 / m_contentsScale);
                context.applyDeviceScaleFactor(m_contentsScale);
                context.translate(-sourceRect.x(), -sourceRect.y());
                sourceLayer->paintGraphicsLayerContents(context, sourceRect);
            }
            picture = recorder.finishRecordingAsPicture();
        }
        bool painted = true;
        for (auto& entry : paints) {
            auto& tile = m_coverageTiles[entry.index].tile;
            auto target = entry.rect;
            auto tileRect = enclosingIntRect(tile.rect());
            target.move(-tileRect.x(), -tileRect.y());
            if (!tile.texture()->updateContents(*picture, target, IntPoint(entry.rect.x() - area.x(), entry.rect.y() - area.y()))) {
                painted = false;
                break;
            }
        }
        if (painted) {
            for (auto& entry : paints) {
                m_coverageTiles[entry.index].valid = true;
                addSkiaDamage(entry.rect);
            }
            static bool logged;
            if (!logged) {
                WTFLogAlways("Symbian Skia: Ganesh GLES2 tile rasterization");
                logged = true;
            }
            return true;
        }
    }
#endif

    auto imageBuffer = ImageBuffer::create(area.size(), RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!imageBuffer)
        return false;
    GraphicsContext& context = imageBuffer->context();
    context.setTextDrawingMode(TextDrawingMode::Fill);
    IntRect sourceRect(area);
    sourceRect.scale(1 / m_contentsScale);
    context.applyDeviceScaleFactor(m_contentsScale);
    context.translate(-sourceRect.x(), -sourceRect.y());

    auto paint = [&] {
        sourceLayer->paintGraphicsLayerContents(context, sourceRect);
    };
    auto upload = [&] {
        auto image = ImageBuffer::sinkIntoNativeImage(WTF::move(imageBuffer));
        if (!image)
            return false;
        for (auto& entry : paints) {
            auto& coverageTile = m_coverageTiles[entry.index];
            IntRect tileRect = enclosingIntRect(coverageTile.tile.rect());
            IntRect target = entry.rect;
            target.move(-tileRect.x(), -tileRect.y());
            coverageTile.tile.texture()->updateContents(image.get(), target, IntPoint(entry.rect.x() - area.x(), entry.rect.y() - area.y()));
            coverageTile.valid = true;
#if OS(SYMBIAN) && USE(SKIA)
            addSkiaDamage(entry.rect);
#endif
        }
        return true;
    };
#if OS(SYMBIAN)
    return m_coverageTiles[paints[0].index].tile.texture()->resourceState()->paintAndUpload(paint, upload);
#else
    paint();
    return upload();
#endif
}

static int coverageDistance(const IntRect& tileRect, const IntRect& visibleRect)
{
    int dx = std::max({ 0, visibleRect.x() - tileRect.maxX(), tileRect.x() - visibleRect.maxX() });
    int dy = std::max({ 0, visibleRect.y() - tileRect.maxY(), tileRect.y() - visibleRect.maxY() });
    return dx + dy;
}

bool TextureMapperTiledBackingStore::updateContentsWithCoverage(TextureMapper& textureMapper, GraphicsLayer* sourceLayer, const FloatSize& totalSize, const IntRect& dirtyRect, const IntRect& visibleRect, bool& morePending)
{
    if (!m_coverageMode) {
        m_tiles.clear();
        m_coverageMode = true;
        m_isScaleDirty = true;
    }
    if (totalSize != m_size || m_isScaleDirty) {
        // Tile rects are clipped to the layer: start again on a new size.
        clearCoverageTiles();
        m_size = totalSize;
        m_isScaleDirty = false;
    }

    IntRect bounds = enclosingIntRect(rect());
    IntRect visible = intersection(visibleRect, bounds);
    // Painted ahead: half a screen sideways, a screen above and below. Tiles
    // already painted are kept out to twice that, for scrolling back.
    IntRect cover = visible;
    IntRect keep = visible;
    if (!visible.isEmpty()) {
        cover.inflateX(visible.width() / 2);
        cover.inflateY(visible.height());
        keep.inflateX(visible.width());
        keep.inflateY(visible.height() * 2);
    }
    cover.intersect(bounds);
    keep.intersect(bounds);

    for (size_t i = m_coverageTiles.size(); i--;) {
        if (!keep.intersects(enclosingIntRect(m_coverageTiles[i].tile.rect())))
            removeCoverageTile(i);
    }

    // On-screen tiles take their damage now; the others are painted again
    // whole when they are next needed.
    Vector<CoveragePaint> paints;
    for (size_t index = 0; index < m_coverageTiles.size(); ++index) {
        auto& coverageTile = m_coverageTiles[index];
        IntRect tileRect = enclosingIntRect(coverageTile.tile.rect());
        if (!coverageTile.valid || !tileRect.intersects(dirtyRect))
            continue;
        if (!tileRect.intersects(visible)) {
            coverageTile.valid = false;
            continue;
        }
        paints.append({ index, intersection(tileRect, dirtyRect) });
    }

    if (!cover.isEmpty()) {
        // Columns of equal width, so a layer slightly wider than the limit
        // does not end in a sliver.
        int maxTileWidth = std::max(coverageTileSize, std::min(coverageMaxTileWidth, textureMapper.maxTextureSize().width()));
        int columns = std::max(1, (bounds.width() + maxTileWidth - 1) / maxTileWidth);
        int tileWidth = std::max(1, (bounds.width() + columns - 1) / columns);
        int firstX = bounds.x() + (cover.x() - bounds.x()) / tileWidth * tileWidth;
        int firstY = cover.y() / coverageTileSize * coverageTileSize;
        for (int y = firstY; y < cover.maxY(); y += coverageTileSize) {
            for (int x = firstX; x < cover.maxX(); x += tileWidth) {
                IntRect tileRect = intersection(IntRect(x, y, tileWidth, coverageTileSize), bounds);
                if (tileRect.isEmpty())
                    continue;
                bool exists = false;
                for (auto& coverageTile : m_coverageTiles) {
                    if (enclosingIntRect(coverageTile.tile.rect()) == tileRect) {
                        exists = true;
                        break;
                    }
                }
                if (!exists)
                    m_coverageTiles.append({ TextureMapperTile(FloatRect(tileRect)), false });
            }
        }
    }

    for (size_t i = 0; i < m_coverageTiles.size(); ++i) {
        IntRect tileRect = enclosingIntRect(m_coverageTiles[i].tile.rect());
        if (!m_coverageTiles[i].valid && tileRect.intersects(visible))
            paints.append({ i, tileRect });
    }
    uint64_t paintedArea = 0;
    auto& stats = coverageStats();
    for (auto& entry : paints) {
        uint64_t area = entry.rect.area().value();
        paintedArea += area;
        if (entry.rect == enclosingIntRect(m_coverageTiles[entry.index].tile.rect()) && !m_coverageTiles[entry.index].valid) {
            ++stats.newTiles;
            stats.newPixels += area;
        } else {
            ++stats.damagedTiles;
            stats.damagedPixels += area;
        }
    }
    if (!paints.isEmpty()) {
        MonotonicTime start = MonotonicTime::now();
        if (!paintCoverageTiles(paints, sourceLayer))
            return false;
        Seconds took = MonotonicTime::now() - start;
        s_frameVisibleTime += took;
        stats.visibleMs += took.milliseconds();
        if (took.milliseconds() > stats.worstMs) {
            stats.worstMs = took.milliseconds();
            stats.worstLayer = sourceLayer->name();
        }
    }
    static bool loggedOverBudget;
    if (s_coverageBytes > coverageBudgetBytes && !loggedOverBudget) {
        loggedOverBudget = true;
        WTFLogAlways("TextureMapper: on-screen tiles need %zu KB, over the %zu KB budget", s_coverageBytes / 1024, coverageBudgetBytes / 1024);
    }

    // Margin tiles, nearest first, within the budget. Painted tiles outside
    // the margin give way to them.
    for (unsigned painted = 0; ; ++painted) {
        size_t best = notFound;
        int bestDistance = 0;
        for (size_t i = 0; i < m_coverageTiles.size(); ++i) {
            auto& coverageTile = m_coverageTiles[i];
            IntRect tileRect = enclosingIntRect(coverageTile.tile.rect());
            if (coverageTile.valid || !tileRect.intersects(cover))
                continue;
            int distance = coverageDistance(tileRect, visible);
            if (best == notFound || distance < bestDistance) {
                best = i;
                bestDistance = distance;
            }
        }
        if (best == notFound)
            return true;
        // An update that repainted much of the screen leaves the tiles ahead
        // to the next one, which runs after pending timers: a page repainting
        // itself every frame damages them again first.
        if (!painted && paintedArea * 4 > visible.area().value()) {
            morePending = true;
            return true;
        }
        // The allowance is for the whole frame; a slow frame leaves the rest
        // to the next one.
        bool slowFrame = s_frameVisibleTime > coveragePrefetchSkipAfter && s_skippedFrameRun < coveragePrefetchMaxSkippedFrames;
        if (!s_prefetchLeft || slowFrame) {
            if (s_prefetchLeft && !s_frameSkippedPrefetch) {
                s_frameSkippedPrefetch = true;
                ++stats.prefetchSkipped;
            }
            morePending = true;
            return true;
        }
        size_t bytes = m_coverageTiles[best].tile.texture() ? 0 : coverageTextureBytes(enclosingIntRect(m_coverageTiles[best].tile.rect()).size());
        while (s_coverageBytes + bytes > coverageBudgetBytes) {
            size_t farthest = notFound;
            int farthestDistance = bestDistance;
            for (size_t i = 0; i < m_coverageTiles.size(); ++i) {
                IntRect tileRect = enclosingIntRect(m_coverageTiles[i].tile.rect());
                if (!m_coverageTiles[i].tile.texture() || tileRect.intersects(cover))
                    continue;
                int distance = coverageDistance(tileRect, visible);
                if (distance > farthestDistance) {
                    farthest = i;
                    farthestDistance = distance;
                }
            }
            if (farthest == notFound)
                return true;
            removeCoverageTile(farthest);
            if (farthest < best)
                --best;
        }
        IntRect bestRect = enclosingIntRect(m_coverageTiles[best].tile.rect());
        MonotonicTime start = MonotonicTime::now();
        if (!paintCoverageTiles({ { best, bestRect } }, sourceLayer))
            return false;
        --s_prefetchLeft;
        ++stats.prefetchTiles;
        stats.prefetchPixels += bestRect.area().value();
        stats.prefetchMs += (MonotonicTime::now() - start).milliseconds();
    }
}

} // namespace WebCore
