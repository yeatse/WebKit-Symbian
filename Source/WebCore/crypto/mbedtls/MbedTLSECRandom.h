/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <mbedtls/entropy.h>
#include <wtf/symbian/SymbianSystem.h>

namespace WebCore::MbedTLS {

inline int ecRandom(void*, unsigned char* output, size_t size)
{
    // Key generation, ECDSA nonces and scalar blinding require the complete
    // native entropy fill; the bridge clears output and fails closed otherwise.
    return WTF::SymbianSystem::randomBytes(output, size) ? 0 : MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
}

} // namespace WebCore::MbedTLS
