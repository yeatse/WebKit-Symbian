/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <mbedtls/entropy.h>
#include <wtf/symbian/SymbianSystem.h>

namespace WebCore::MbedTLS {

inline int rsaRandom(void*, unsigned char* output, size_t size)
{
    // The bridge requires a complete SDK hardware-entropy fill and clears the
    // output on failure. This is also required for RSA private-key blinding.
    return WTF::SymbianSystem::randomBytes(output, size) ? 0 : MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
}

} // namespace WebCore::MbedTLS
