/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "config.h"
#include "ExceptionOr.h"

#if ENABLE(WEB_CRYPTO) && USE(MBEDTLS)

#include "CryptoAlgorithmRSASSA_PKCS1_v1_5.h"
#include "CryptoAlgorithmRSA_OAEP.h"
#include "CryptoAlgorithmRSA_PSS.h"
#include "CryptoAlgorithmRsaOaepParams.h"
#include "CryptoAlgorithmRsaPssParams.h"
#include "CryptoKeyRSA.h"
#include "MbedTLSCryptoUtilities.h"
#include "MbedTLSRSAOperations.h"
#include "MbedTLSRSARandom.h"

namespace WebCore {

static ExceptionOr<Vector<uint8_t>> signRSA(const CryptoKeyRSA& key, const Vector<uint8_t>& data, std::optional<size_t> salt)
{
    auto hash = MbedTLS::hashType(key.hashAlgorithmIdentifier());
    if (!hash)
        return Exception { ExceptionCode::NotSupportedError };
    auto result = MbedTLS::rsaSign(key.platformKey(), *hash, data.span(), salt, MbedTLS::rsaRandom);
    if (!result)
        return Exception { ExceptionCode::OperationError };
    return Vector<uint8_t>(std::span<const uint8_t>(*result));
}

static ExceptionOr<bool> verifyRSA(const CryptoKeyRSA& key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data, std::optional<size_t> salt)
{
    auto hash = MbedTLS::hashType(key.hashAlgorithmIdentifier());
    if (!hash)
        return Exception { ExceptionCode::NotSupportedError };
    auto result = MbedTLS::rsaVerify(key.platformKey(), *hash, signature.span(), data.span(), salt);
    if (!result)
        return Exception { ExceptionCode::OperationError };
    return *result;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSASSA_PKCS1_v1_5::platformSign(const CryptoKeyRSA& key, const Vector<uint8_t>& data)
{
    return signRSA(key, data, std::nullopt);
}

ExceptionOr<bool> CryptoAlgorithmRSASSA_PKCS1_v1_5::platformVerify(const CryptoKeyRSA& key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
    return verifyRSA(key, signature, data, std::nullopt);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSA_PSS::platformSign(const CryptoAlgorithmRsaPssParams& parameters, const CryptoKeyRSA& key, const Vector<uint8_t>& data)
{
    return signRSA(key, data, parameters.saltLength);
}

ExceptionOr<bool> CryptoAlgorithmRSA_PSS::platformVerify(const CryptoAlgorithmRsaPssParams& parameters, const CryptoKeyRSA& key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
    return verifyRSA(key, signature, data, parameters.saltLength);
}

static ExceptionOr<Vector<uint8_t>> performOAEP(const CryptoAlgorithmRsaOaepParams& parameters, const CryptoKeyRSA& key, const Vector<uint8_t>& data, bool decrypt)
{
    auto hash = MbedTLS::hashType(key.hashAlgorithmIdentifier());
    if (!hash)
        return Exception { ExceptionCode::NotSupportedError };
    auto result = MbedTLS::rsaOAEP(key.platformKey(), *hash, data.span(), parameters.labelVector().span(), decrypt, MbedTLS::rsaRandom);
    if (!result)
        return Exception { ExceptionCode::OperationError };
    auto output = Vector<uint8_t>(std::span<const uint8_t>(*result));
    mbedtls_platform_zeroize(result->data(), result->size());
    return output;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSA_OAEP::platformEncrypt(const CryptoAlgorithmRsaOaepParams& parameters, const CryptoKeyRSA& key, const Vector<uint8_t>& data)
{
    return performOAEP(parameters, key, data, false);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSA_OAEP::platformDecrypt(const CryptoAlgorithmRsaOaepParams& parameters, const CryptoKeyRSA& key, const Vector<uint8_t>& data)
{
    return performOAEP(parameters, key, data, true);
}

} // namespace WebCore

#endif
