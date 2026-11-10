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

#include "config.h"
#include "PlatformScreen.h"

#include "FloatRect.h"
#include "DestinationColorSpace.h"
#include "ContentsFormat.h"
#include <wtf/symbian/SymbianSystem.h>

namespace WebCore {

// Symbian^3 phones have 16.7M-colour displays.
int screenDepth(Widget*)
{
    return 24;
}

int screenDepthPerComponent(Widget*)
{
    return 8;
}

bool screenIsMonochrome(Widget*)
{
    return false;
}

bool screenHasInvertedColors()
{
    return false;
}

FloatRect screenRect(Widget*)
{
    int width = 360;
    int height = 640;
    WTF::SymbianSystem::displaySize(width, height);
    return { 0, 0, static_cast<float>(width), static_cast<float>(height) };
}

FloatRect screenAvailableRect(Widget* widget)
{
    return screenRect(widget);
}

bool screenSupportsExtendedColor(Widget*)
{
    return false;
}

DestinationColorSpace screenColorSpace(Widget*)
{
    return DestinationColorSpace::SRGB();
}

OptionSet<ContentsFormat> screenContentsFormats(Widget*)
{
    return ContentsFormat::RGBA8;
}

} // namespace WebCore
