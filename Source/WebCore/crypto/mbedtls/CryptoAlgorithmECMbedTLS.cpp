/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "config.h"
#include "ExceptionOr.h"

#if ENABLE(WEB_CRYPTO) && USE(MBEDTLS)

#include "CryptoAlgorithmECDH.h"
#include "CryptoAlgorithmECDSA.h"
#include "CryptoAlgorithmEcdsaParams.h"
#include "CryptoKeyEC.h"
#include "MbedTLSCryptoUtilities.h"
#include "MbedTLSECOperations.h"
#include "MbedTLSECRandom.h"

namespace WebCore {

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmECDSA::platformSign(const CryptoAlgorithmEcdsaParams& parameters, const CryptoKeyEC& key, const Vector<uint8_t>& data)
{
    auto hash = MbedTLS::hashType(parameters.hashIdentifier);
    if (!hash)
        return Exception { ExceptionCode::NotSupportedError };
    auto result = MbedTLS::ecSign(*key.platformKey(), *hash, data.span(), MbedTLS::ecRandom);
    if (!result)
        return Exception { ExceptionCode::OperationError };
    return Vector<uint8_t>(std::span<const uint8_t>(*result));
}

ExceptionOr<bool> CryptoAlgorithmECDSA::platformVerify(const CryptoAlgorithmEcdsaParams& parameters, const CryptoKeyEC& key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
    auto hash = MbedTLS::hashType(parameters.hashIdentifier);
    if (!hash)
        return Exception { ExceptionCode::NotSupportedError };
    auto result = MbedTLS::ecVerify(*key.platformKey(), *hash, signature.span(), data.span());
    if (!result)
        return Exception { ExceptionCode::OperationError };
    return *result;
}

std::optional<Vector<uint8_t>> CryptoAlgorithmECDH::platformDeriveBits(const CryptoKeyEC& baseKey, const CryptoKeyEC& publicKey)
{
    auto result = MbedTLS::ecDerive(*baseKey.platformKey(), *publicKey.platformKey(), MbedTLS::ecRandom);
    if (!result)
        return std::nullopt;
    auto output = Vector<uint8_t>(std::span<const uint8_t>(*result));
    mbedtls_platform_zeroize(result->data(), result->size());
    return output;
}

} // namespace WebCore

#endif
