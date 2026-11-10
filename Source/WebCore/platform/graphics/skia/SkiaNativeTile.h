/* Copyright (C) 2026 The symbian-webkit authors. All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#if OS(SYMBIAN) && USE(SKIA)
#include "BitmapTexture.h"
#include "FloatRect.h"
#include <wtf/RefPtr.h>

namespace WebCore {
struct SkiaNativeTile {
    RefPtr<BitmapTexture> texture;
    FloatRect rect;
};
}
#endif
