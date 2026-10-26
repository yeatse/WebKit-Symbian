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

#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>
#include <mbedtls/sha512.h>

namespace {

// mbedTLS covers SHA-224 and SHA-384 with the SHA-256 and SHA-512 contexts;
// the flag passed to starts() selects the truncated variant.
template <typename Context, void (*init)(Context*), void (*release)(Context*), int (*update)(Context*, const unsigned char*, size_t), int (*finish)(Context*, unsigned char*)>
struct MbedTLSFunctions {
    using ContextType = Context;
    static void initialize(Context* context) { init(context); }
    static void free(Context* context) { release(context); }
    static void add(Context* context, const void* input, size_t length) { update(context, static_cast<const unsigned char*>(input), length); }
    static void finalize(Context* context, uint8_t* output) { finish(context, output); }
};

using MbedSHA1 = MbedTLSFunctions<mbedtls_sha1_context, mbedtls_sha1_init, mbedtls_sha1_free, mbedtls_sha1_update, mbedtls_sha1_finish>;
using MbedSHA256 = MbedTLSFunctions<mbedtls_sha256_context, mbedtls_sha256_init, mbedtls_sha256_free, mbedtls_sha256_update, mbedtls_sha256_finish>;
using MbedSHA512 = MbedTLSFunctions<mbedtls_sha512_context, mbedtls_sha512_init, mbedtls_sha512_free, mbedtls_sha512_update, mbedtls_sha512_finish>;

int startSHA1(mbedtls_sha1_context* context, int) { return mbedtls_sha1_starts(context); }

}

namespace PAL {

struct CryptoDigestContext {
    virtual ~CryptoDigestContext() = default;
    virtual void addBytes(const void* input, size_t length) = 0;
    virtual Vector<uint8_t> computeHash() = 0;
};

template <typename Functions, int (*start)(typename Functions::ContextType*, int), int variant, size_t digestLength>
struct CryptoDigestContextImpl : public CryptoDigestContext {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    CryptoDigestContextImpl()
    {
        Functions::initialize(&m_context);
        start(&m_context, variant);
    }

    ~CryptoDigestContextImpl()
    {
        Functions::free(&m_context);
    }

    void addBytes(const void* input, size_t length) override
    {
        Functions::add(&m_context, input, length);
    }

    Vector<uint8_t> computeHash() override
    {
        // mbedTLS always writes the untruncated SHA-256 / SHA-512 size.
        uint8_t output[64];
        Functions::finalize(&m_context, output);
        Vector<uint8_t> result;
        result.append(output, digestLength);
        return result;
    }

private:
    typename Functions::ContextType m_context;
};

CryptoDigest::CryptoDigest()
{
}

CryptoDigest::~CryptoDigest()
{
}

std::unique_ptr<CryptoDigest> CryptoDigest::create(CryptoDigest::Algorithm algorithm)
{
    std::unique_ptr<CryptoDigest> digest(new CryptoDigest);

    switch (algorithm) {
    case CryptoDigest::Algorithm::SHA_1:
        digest->m_context = makeUnique<CryptoDigestContextImpl<MbedSHA1, startSHA1, 0, 20>>();
        return digest;
    case CryptoDigest::Algorithm::SHA_224:
        digest->m_context = makeUnique<CryptoDigestContextImpl<MbedSHA256, mbedtls_sha256_starts, 1, 28>>();
        return digest;
    case CryptoDigest::Algorithm::SHA_256:
        digest->m_context = makeUnique<CryptoDigestContextImpl<MbedSHA256, mbedtls_sha256_starts, 0, 32>>();
        return digest;
    case CryptoDigest::Algorithm::SHA_384:
        digest->m_context = makeUnique<CryptoDigestContextImpl<MbedSHA512, mbedtls_sha512_starts, 1, 48>>();
        return digest;
    case CryptoDigest::Algorithm::SHA_512:
        digest->m_context = makeUnique<CryptoDigestContextImpl<MbedSHA512, mbedtls_sha512_starts, 0, 64>>();
        return digest;
    }

    return nullptr;
}

void CryptoDigest::addBytes(const void* input, size_t length)
{
    ASSERT(m_context);
    m_context->addBytes(input, length);
}

Vector<uint8_t> CryptoDigest::computeHash()
{
    ASSERT(m_context);
    return m_context->computeHash();
}

} // namespace PAL
