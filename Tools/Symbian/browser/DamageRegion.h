/* Copyright (C) 2026 The symbian-webkit authors. All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include <e32std.h>

// Damage kept as a few rectangles, so that separate changes (a strip a
// scroll exposed and a fixed element at the other edge) are not painted as
// one rectangle spanning the view. Rectangles merge when painting the extra
// pixels costs less than painting another rectangle; beyond KMaxRects the
// cheapest pair merges.
class TDamageRegion {
public:
    enum { KMaxRects = 4 };
    TBool IsEmpty() const { return !iCount; }
    TInt Count() const { return iCount; }
    const TRect& operator[](TInt aIndex) const { return iRects[aIndex]; }
    TRect Bounds() const
    {
        TRect bounds;
        for (TInt i = 0; i < iCount; ++i)
            bounds = Bounding(bounds, iRects[i]);
        return bounds;
    }
    void Clear() { iCount = 0; }
    void Reset(const TRect& aRect)
    {
        iCount = 0;
        Add(aRect);
    }
    void Add(const TRect& aRect)
    {
        if (aRect.IsEmpty())
            return;
        TRect rect = aRect;
        for (TInt i = 0; i < iCount;) {
            if (Intersect(iRects[i], rect) == rect)
                return;
            if (MergeWaste(iRects[i], rect) <= KMergeSlack) {
                rect = Bounding(iRects[i], rect);
                Remove(i);
                i = 0; // The larger rectangle may now reach others.
                continue;
            }
            ++i;
        }
        if (iCount == KMaxRects) {
            TInt best = 0;
            for (TInt i = 1; i < iCount; ++i) {
                if (MergeWaste(iRects[i], rect) < MergeWaste(iRects[best], rect))
                    best = i;
            }
            rect = Bounding(iRects[best], rect);
            Remove(best);
            Add(rect);
            return;
        }
        iRects[iCount++] = rect;
    }
    void Add(const TDamageRegion& aRegion)
    {
        for (TInt i = 0; i < aRegion.iCount; ++i)
            Add(aRegion.iRects[i]);
    }
    // The engine scrolled aArea's pixels by aDelta: the damaged pixels inside
    // it moved too. Their old positions stay damaged.
    void Scroll(const TRect& aArea, const TPoint& aDelta)
    {
        const TDamageRegion before = *this;
        for (TInt i = 0; i < before.iCount; ++i) {
            TRect inside = Intersect(before.iRects[i], aArea);
            if (inside.IsEmpty())
                continue;
            TRect moved(inside.iTl.iX + aDelta.iX, inside.iTl.iY + aDelta.iY,
                inside.iBr.iX + aDelta.iX, inside.iBr.iY + aDelta.iY);
            Add(Intersect(moved, aArea));
        }
    }

private:
    // Painting a rectangle has a fixed cost (layout check, clip, context).
    enum { KMergeSlack = 32 * 32 };
    static TRect Intersect(const TRect& aFirst, const TRect& aSecond)
    {
        TRect result(Max(aFirst.iTl.iX, aSecond.iTl.iX), Max(aFirst.iTl.iY, aSecond.iTl.iY),
            Min(aFirst.iBr.iX, aSecond.iBr.iX), Min(aFirst.iBr.iY, aSecond.iBr.iY));
        return result.IsEmpty() ? TRect() : result;
    }
    static TRect Bounding(const TRect& aFirst, const TRect& aSecond)
    {
        if (aFirst.IsEmpty())
            return aSecond;
        if (aSecond.IsEmpty())
            return aFirst;
        return TRect(Min(aFirst.iTl.iX, aSecond.iTl.iX), Min(aFirst.iTl.iY, aSecond.iTl.iY),
            Max(aFirst.iBr.iX, aSecond.iBr.iX), Max(aFirst.iBr.iY, aSecond.iBr.iY));
    }
    static TInt Area(const TRect& aRect) { return aRect.IsEmpty() ? 0 : aRect.Width() * aRect.Height(); }
    // Pixels a merged rectangle would paint that neither rectangle needs.
    static TInt MergeWaste(const TRect& aFirst, const TRect& aSecond)
    {
        return Area(Bounding(aFirst, aSecond)) - Area(aFirst) - Area(aSecond)
            + Area(Intersect(aFirst, aSecond));
    }
    void Remove(TInt aIndex)
    {
        for (TInt i = aIndex + 1; i < iCount; ++i)
            iRects[i - 1] = iRects[i];
        --iCount;
    }

    TRect iRects[KMaxRects];
    TInt iCount { 0 };
};
