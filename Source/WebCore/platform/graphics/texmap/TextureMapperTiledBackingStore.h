/*
 Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies)

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

#ifndef TextureMapperTiledBackingStore_h
#define TextureMapperTiledBackingStore_h

#include "FloatRect.h"
#include "Image.h"
#include "IntRect.h"
#include "TextureMapperBackingStore.h"
#include "TextureMapperTile.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>
#include <utility>

namespace WebCore {

class TextureMapper;

class TextureMapperTiledBackingStore : public RefCounted<TextureMapperTiledBackingStore>, public TextureMapperBackingStore {
public:
    static Ref<TextureMapperTiledBackingStore> create() { return adoptRef(*new TextureMapperTiledBackingStore); }

    void paintToTextureMapper(TextureMapper&, const FloatRect&, const TransformationMatrix&, float) override;
    void drawBorder(TextureMapper&, const Color&, float borderWidth, const FloatRect&, const TransformationMatrix&) override;
    void drawRepaintCounter(TextureMapper&, int repaintCount, const Color&, const FloatRect&, const TransformationMatrix&) override;
#if OS(SYMBIAN) && USE(SKIA)
    bool updateSkiaBackingStore(SkiaBackingStore&, TextureMapper&) override;
    Vector<IntRect> takeSkiaDamage() override { return std::exchange(m_skiaDamage, { }); }
    sk_sp<SkImage> skiaImage() override;
#endif

    void updateContentsScale(float);
    void updateContents(TextureMapper&, Image*, const FloatSize&, const IntRect&);
    bool updateContents(TextureMapper&, GraphicsLayer*, const FloatSize&, const IntRect&);

    void setContentsToImage(Image*);

    // Coverage mode keeps tiles only around the part of the layer that is on
    // screen (visibleRect, in contents-scaled layer coordinates), instead of
    // tiling and painting the whole layer: a long page's document layer then
    // costs a few screens of textures. Tiles that come into view are painted
    // in the same update; the margin around them is painted a few tiles per
    // update (morePending asks for another one). Textures kept for margins
    // stay within coverageBudgetBytes across all coverage stores.
    bool updateContentsWithCoverage(TextureMapper&, GraphicsLayer*, const FloatSize&, const IntRect& dirtyRect, const IntRect& visibleRect, bool& morePending);
    static constexpr size_t coverageBudgetBytes = 32 * 1024 * 1024;
    WEBCORE_EXPORT static size_t coverageBytes();

    // Painting done by coverage stores, summed until reset. The margin tile
    // allowance is shared by all stores and given back by beginCoverageFrame,
    // so a frame with many layers does not paint margins for each.
    struct CoverageStats {
        unsigned frames { 0 };
        unsigned newTiles { 0 }; // On screen, painted whole.
        unsigned damagedTiles { 0 }; // On screen, part repainted.
        unsigned prefetchTiles { 0 };
        unsigned prefetchSkipped { 0 }; // Frames that left margins for later.
        uint64_t newPixels { 0 };
        uint64_t damagedPixels { 0 };
        uint64_t prefetchPixels { 0 };
        double visibleMs { 0 };
        double prefetchMs { 0 };
        double worstMs { 0 };
        String worstLayer;
    };
    WEBCORE_EXPORT static CoverageStats& coverageStats();
    WEBCORE_EXPORT static void beginCoverageFrame();

    ~TextureMapperTiledBackingStore();

private:
    TextureMapperTiledBackingStore() = default;

    struct CoverageTile {
        TextureMapperTile tile;
        bool valid { false };
    };
    void clearCoverageTiles();
    void removeCoverageTile(size_t index);
    struct CoveragePaint {
        size_t index;
        IntRect rect;
    };
    bool paintCoverageTiles(const Vector<CoveragePaint>&, GraphicsLayer*);
#if OS(SYMBIAN) && USE(SKIA)
    void addSkiaDamage(const IntRect&);
    Vector<IntRect> m_skiaDamage;
    FloatSize m_skiaTileGridSize;
    sk_sp<SkImage> m_skiaContentsImage;
#endif
    Vector<CoverageTile> m_coverageTiles;
    size_t m_coverageBytes { 0 };
    bool m_coverageMode { false };

    void createOrDestroyTilesIfNeeded(const FloatSize& backingStoreSize, const IntSize& tileSize, bool hasAlpha);
    void updateContentsFromImageIfNeeded(TextureMapper&);
    TransformationMatrix adjustedTransformForRect(const FloatRect&);
    inline FloatRect rect() const
    {
        FloatRect rect(FloatPoint::zero(), m_size);
        rect.scale(m_contentsScale);
        return rect;
    }

    Vector<TextureMapperTile> m_tiles;
    FloatSize m_size;
    RefPtr<Image> m_image;
    float m_contentsScale { 1 };
    bool m_isScaleDirty { false };
};

} // namespace WebCore

#endif
