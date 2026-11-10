/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace WebCore::SymbianJPEG {

// The phone's own JPEG decoder (ICL). Every call belongs to the one thread
// that created the session: it installs that thread's active scheduler and
// font and bitmap server session. Native descriptors and leaves stay inside.
void* createSession(int& error);

// Decodes a complete JPEG of |width| x |height| into a bitmap the session
// keeps until copyPixels() or discardBitmap(). Returns a Symbian error code.
int decode(void* session, const uint8_t* data, size_t size, int width, int height);

// Writes the decoded bitmap as opaque 0xAARRGGBB words, |stride| words a row,
// and discards it.
void copyPixels(void* session, uint32_t* pixels, size_t stride);
void discardBitmap(void* session);

} // namespace WebCore::SymbianJPEG
