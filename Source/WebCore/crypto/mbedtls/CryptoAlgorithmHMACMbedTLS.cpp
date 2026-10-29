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
#include "CryptoAlgorithmHMAC.h"

#if ENABLE(WEB_CRYPTO) && USE(MBEDTLS)

#include "CryptoKeyHMAC.h"
#include "MbedTLSCryptoUtilities.h"
#include <wtf/CryptographicUtilities.h>

namespace WebCore {

static std::optional<Vector<uint8_t>> calculateHMAC(const CryptoKeyHMAC& key, const Vector<uint8_t>& data)
{
    auto type = MbedTLS::hashType(key.hashAlgorithmIdentifier());
    if (!type)
        return std::nullopt;
    auto* info = mbedtls_md_info_from_type(*type);
    if (!info)
        return std::nullopt;

    Vector<uint8_t> result(mbedtls_md_get_size(info));
    if (mbedtls_md_hmac(info, key.key().data(), key.key().size(), data.data(), data.size(), result.data()))
        return std::nullopt;
    return result;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmHMAC::platformSign(const CryptoKeyHMAC& key, const Vector<uint8_t>& data)
{
    auto result = calculateHMAC(key, data);
    if (!result)
        return Exception { ExceptionCode::OperationError };
    return WTF::move(*result);
}

ExceptionOr<bool> CryptoAlgorithmHMAC::platformVerify(const CryptoKeyHMAC& key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
    auto expected = calculateHMAC(key, data);
    if (!expected)
        return Exception { ExceptionCode::OperationError };
    return signature.size() == expected->size() && !constantTimeMemcmp(expected->span(), signature.span());
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO) && USE(MBEDTLS)
