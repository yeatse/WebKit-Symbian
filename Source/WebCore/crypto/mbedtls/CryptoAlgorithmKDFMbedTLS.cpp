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
#include "ExceptionOr.h"

#if ENABLE(WEB_CRYPTO) && USE(MBEDTLS)

#include "CryptoAlgorithmHKDF.h"
#include "CryptoAlgorithmHkdfParams.h"
#include "CryptoAlgorithmPBKDF2.h"
#include "CryptoAlgorithmPbkdf2Params.h"
#include "CryptoKeyRaw.h"
#include "MbedTLSCryptoUtilities.h"
#include <mbedtls/hkdf.h>
#include <mbedtls/pkcs5.h>

namespace WebCore {

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmHKDF::platformDeriveBits(const CryptoAlgorithmHkdfParams& parameters, const CryptoKeyRaw& key, size_t length)
{
    auto type = MbedTLS::hashType(parameters.hashIdentifier);
    if (!type)
        return Exception { ExceptionCode::NotSupportedError };

    const auto* hash = mbedtls_md_info_from_type(*type);
    if (!hash)
        return Exception { ExceptionCode::NotSupportedError };
    if (length % 8 || length / 8 > 255 * static_cast<size_t>(mbedtls_md_get_size(hash)))
        return Exception { ExceptionCode::OperationError };
    if (!length)
        return Vector<uint8_t> { };

    const auto& salt = parameters.saltVector();
    const auto& info = parameters.infoVector();
    Vector<uint8_t> result;
    if (!result.tryGrow(length / 8))
        return Exception { ExceptionCode::OperationError };
    if (mbedtls_hkdf(hash, salt.span().data(), salt.size(), key.key().span().data(), key.key().size(),
        info.span().data(), info.size(), result.mutableSpan().data(), result.size()))
        return Exception { ExceptionCode::OperationError };
    return result;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmPBKDF2::platformDeriveBits(const CryptoAlgorithmPbkdf2Params& parameters, const CryptoKeyRaw& key, size_t length)
{
    auto type = MbedTLS::hashType(parameters.hashIdentifier);
    if (!type)
        return Exception { ExceptionCode::NotSupportedError };
    if (!parameters.iterations)
        return Exception { ExceptionCode::OperationError };

    const auto& salt = parameters.saltVector();
    Vector<uint8_t> result;
    if (!result.tryGrow(length / 8))
        return Exception { ExceptionCode::OperationError };
    if (mbedtls_pkcs5_pbkdf2_hmac_ext(*type, key.key().span().data(), key.key().size(), salt.span().data(), salt.size(),
        parameters.iterations, result.size(), result.mutableSpan().data()))
        return Exception { ExceptionCode::OperationError };
    return result;
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO) && USE(MBEDTLS)
