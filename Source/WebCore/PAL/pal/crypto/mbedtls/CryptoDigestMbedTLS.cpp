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
#include "CryptoDigest.h"

#include <mbedtls/md.h>

namespace PAL {

struct CryptoDigestContext {
    CryptoDigestContext() { mbedtls_md_init(&context); }
    ~CryptoDigestContext() { mbedtls_md_free(&context); }
    mbedtls_md_context_t context;
    size_t size { 0 };
};

CryptoDigest::CryptoDigest() = default;
CryptoDigest::~CryptoDigest() = default;

std::unique_ptr<CryptoDigest> CryptoDigest::create(Algorithm algorithm)
{
    mbedtls_md_type_t type;
    switch (algorithm) {
    case Algorithm::SHA_1: type = MBEDTLS_MD_SHA1; break;
    case Algorithm::DEPRECATED_SHA_224: type = MBEDTLS_MD_SHA224; break;
    case Algorithm::SHA_256: type = MBEDTLS_MD_SHA256; break;
    case Algorithm::SHA_384: type = MBEDTLS_MD_SHA384; break;
    case Algorithm::SHA_512: type = MBEDTLS_MD_SHA512; break;
    default: return nullptr;
    }
    auto* info = mbedtls_md_info_from_type(type);
    if (!info)
        return nullptr;
    auto digest = makeUnique<CryptoDigest>();
    digest->m_context = makeUnique<CryptoDigestContext>();
    digest->m_context->size = mbedtls_md_get_size(info);
    if (mbedtls_md_setup(&digest->m_context->context, info, 0)
        || mbedtls_md_starts(&digest->m_context->context))
        return nullptr;
    return digest;
}

void CryptoDigest::addBytes(std::span<const uint8_t> input)
{
    RELEASE_ASSERT(!mbedtls_md_update(&m_context->context, input.data(), input.size()));
}

Vector<uint8_t> CryptoDigest::computeHash()
{
    Vector<uint8_t> result(m_context->size);
    RELEASE_ASSERT(!mbedtls_md_finish(&m_context->context, result.data()));
    return result;
}

} // namespace PAL
