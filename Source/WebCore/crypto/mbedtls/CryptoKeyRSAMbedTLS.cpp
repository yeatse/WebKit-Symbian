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
#include "CryptoKeyRSA.h"

#if ENABLE(WEB_CRYPTO) && USE(MBEDTLS)

#include "CryptoAlgorithmRegistry.h"
#include "CryptoKeyPair.h"
#include "CryptoKeyRSAComponents.h"
#include "MbedTLSCryptoUtilities.h"
#include "ScriptExecutionContext.h"
#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSGenericTypedArrayViewInlines.h>
#include <wtf/Threading.h>

namespace WebCore {

static const uint8_t* dataOrNull(const Vector<uint8_t>& data)
{
    return data.isEmpty() ? nullptr : data.data();
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::create(CryptoAlgorithmIdentifier identifier, CryptoAlgorithmIdentifier hash, bool hasHash, const CryptoKeyRSAComponents& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    auto context = MbedTLS::createPKContext(MBEDTLS_PK_RSA);
    if (!context)
        return nullptr;
    auto& rsa = MbedTLS::rsaContext(context.get());

    const auto& modulus = keyData.modulus();
    const auto& exponent = keyData.exponent();
    if (modulus.isEmpty() || exponent.isEmpty())
        return nullptr;

    CryptoKeyType keyType;
    switch (keyData.type()) {
    case CryptoKeyRSAComponents::Type::Public:
        keyType = CryptoKeyType::Public;
        if (mbedtls_rsa_import_raw(&rsa, modulus.data(), modulus.size(), nullptr, 0, nullptr, 0, nullptr, 0, exponent.data(), exponent.size())
            || mbedtls_rsa_complete(&rsa) || mbedtls_rsa_check_pubkey(&rsa))
            return nullptr;
        break;
    case CryptoKeyRSAComponents::Type::Private: {
        keyType = CryptoKeyType::Private;
        const auto& privateExponent = keyData.privateExponent();
        if (privateExponent.isEmpty())
            return nullptr;
        // Without the primes, mbedtls_rsa_complete() finds them from n, e and d.
        // The CRT values are always recomputed.
        const auto& p = keyData.hasAdditionalPrivateKeyParameters() ? keyData.firstPrimeInfo().primeFactor : Vector<uint8_t> { };
        const auto& q = keyData.hasAdditionalPrivateKeyParameters() ? keyData.secondPrimeInfo().primeFactor : Vector<uint8_t> { };
        if (mbedtls_rsa_import_raw(&rsa, modulus.data(), modulus.size(), dataOrNull(p), p.size(), dataOrNull(q), q.size(),
            privateExponent.data(), privateExponent.size(), exponent.data(), exponent.size())
            || mbedtls_rsa_complete(&rsa) || mbedtls_rsa_check_privkey(&rsa))
            return nullptr;
        break;
    }
    default:
        return nullptr;
    }

    return adoptRef(new CryptoKeyRSA(identifier, hash, hasHash, keyType, WTFMove(context), extractable, usages));
}

CryptoKeyRSA::CryptoKeyRSA(CryptoAlgorithmIdentifier identifier, CryptoAlgorithmIdentifier hash, bool hasHash, CryptoKeyType type, PlatformRSAKeyContainer&& platformKey, bool extractable, CryptoKeyUsageBitmap usage)
    : CryptoKey(identifier, type, extractable, usage)
    , m_platformKey(WTFMove(platformKey))
    , m_restrictedToSpecificHash(hasHash)
    , m_hash(hash)
{
}

bool CryptoKeyRSA::isRestrictedToHash(CryptoAlgorithmIdentifier& identifier) const
{
    if (!m_restrictedToSpecificHash)
        return false;

    identifier = m_hash;
    return true;
}

size_t CryptoKeyRSA::keySizeInBits() const
{
    return mbedtls_pk_get_bitlen(m_platformKey.get());
}

// Convert the exponent vector to an int, as mbedtls_rsa_gen_key() takes, if possible.
static Optional<int> exponentVectorToInt(const Vector<uint8_t>& exponent)
{
    uint64_t result = 0;
    for (auto byte : exponent) {
        result = (result << 8) | byte;
        if (result > static_cast<uint64_t>(std::numeric_limits<int>::max()))
            return WTF::nullopt;
    }
    return static_cast<int>(result);
}

static PlatformRSAKeyContainer publicKeyFrom(const mbedtls_rsa_context& privateKey)
{
    MbedTLS::MPI n, e;
    if (mbedtls_rsa_export(&privateKey, n.get(), nullptr, nullptr, nullptr, e.get()))
        return nullptr;

    auto context = MbedTLS::createPKContext(MBEDTLS_PK_RSA);
    if (!context)
        return nullptr;
    auto& rsa = MbedTLS::rsaContext(context.get());
    if (mbedtls_rsa_import(&rsa, n.get(), nullptr, nullptr, nullptr, e.get()) || mbedtls_rsa_complete(&rsa))
        return nullptr;
    return context;
}

void CryptoKeyRSA::generatePair(CryptoAlgorithmIdentifier algorithm, CryptoAlgorithmIdentifier hash, bool hasHash, unsigned modulusLength, const Vector<uint8_t>& publicExponent, bool extractable, CryptoKeyUsageBitmap usage, KeyPairCallback&& callback, VoidCallback&& failureCallback, ScriptExecutionContext* context)
{
    // mbedtls_rsa_gen_key() wants an odd exponent of at least 3 and an even
    // modulus length of at least 128 bits.
    auto e = exponentVectorToInt(publicExponent);
    if (!e || *e < 3 || !(*e & 1) || modulusLength < 128 || modulusLength % 2 || modulusLength > MBEDTLS_MPI_MAX_BITS) {
        failureCallback();
        return;
    }

    // A 2048-bit key takes many seconds on an ARM11, so don't block the page's thread.
    Thread::create("RSA key generation", [algorithm, hash, hasHash, modulusLength, exponent = *e, extractable, usage,
        callback = WTFMove(callback), failureCallback = WTFMove(failureCallback), contextIdentifier = context->contextIdentifier()]() mutable {
        PlatformRSAKeyContainer privateKey;
        PlatformRSAKeyContainer publicKey;
        {
            MbedTLS::Random random;
            auto generated = MbedTLS::createPKContext(MBEDTLS_PK_RSA);
            if (random && generated && !mbedtls_rsa_gen_key(&MbedTLS::rsaContext(generated.get()), MbedTLS::Random::generate, random.context(), modulusLength, exponent)) {
                publicKey = publicKeyFrom(MbedTLS::rsaContext(generated.get()));
                if (publicKey)
                    privateKey = WTFMove(generated);
            }
        }

        ScriptExecutionContext::postTaskTo(contextIdentifier, [algorithm, hash, hasHash, extractable, usage, privateKey = WTFMove(privateKey), publicKey = WTFMove(publicKey),
            callback = WTFMove(callback), failureCallback = WTFMove(failureCallback)](auto&) mutable {
            if (!privateKey) {
                failureCallback();
                return;
            }
            auto publicCryptoKey = CryptoKeyRSA::create(algorithm, hash, hasHash, CryptoKeyType::Public, WTFMove(publicKey), true, usage);
            auto privateCryptoKey = CryptoKeyRSA::create(algorithm, hash, hasHash, CryptoKeyType::Private, WTFMove(privateKey), extractable, usage);
            callback(CryptoKeyPair { WTFMove(publicCryptoKey), WTFMove(privateCryptoKey) });
        });
    })->detach();
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::importSpki(CryptoAlgorithmIdentifier identifier, Optional<CryptoAlgorithmIdentifier> hash, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    // Only rsaEncryption keys, as with libgcrypt; mbedTLS doesn't parse the
    // id-RSASSA-PSS and id-RSAES-OAEP identifiers.
    auto context = MbedTLS::parseSubjectPublicKeyInfo(keyData);
    if (!context || mbedtls_pk_get_type(context.get()) != MBEDTLS_PK_RSA)
        return nullptr;

    return adoptRef(new CryptoKeyRSA(identifier, hash.valueOr(CryptoAlgorithmIdentifier::SHA_1), !!hash, CryptoKeyType::Public, WTFMove(context), extractable, usages));
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::importPkcs8(CryptoAlgorithmIdentifier identifier, Optional<CryptoAlgorithmIdentifier> hash, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    auto context = MbedTLS::parsePrivateKeyInfo(keyData);
    if (!context || mbedtls_pk_get_type(context.get()) != MBEDTLS_PK_RSA)
        return nullptr;

    return adoptRef(new CryptoKeyRSA(identifier, hash.valueOr(CryptoAlgorithmIdentifier::SHA_1), !!hash, CryptoKeyType::Private, WTFMove(context), extractable, usages));
}

ExceptionOr<Vector<uint8_t>> CryptoKeyRSA::exportSpki() const
{
    if (type() != CryptoKeyType::Public)
        return Exception { InvalidAccessError };

    auto result = MbedTLS::subjectPublicKeyInfo(m_platformKey.get());
    if (result.isEmpty())
        return Exception { OperationError };
    return result;
}

ExceptionOr<Vector<uint8_t>> CryptoKeyRSA::exportPkcs8() const
{
    if (type() != CryptoKeyType::Private)
        return Exception { InvalidAccessError };

    auto result = MbedTLS::privateKeyInfo(m_platformKey.get());
    if (result.isEmpty())
        return Exception { OperationError };
    return result;
}

static Vector<uint8_t> rsaPublicExponent(mbedtls_pk_context* context)
{
    MbedTLS::MPI e;
    if (mbedtls_rsa_export(&MbedTLS::rsaContext(context), nullptr, nullptr, nullptr, nullptr, e.get()))
        return { };
    return MbedTLS::mpiData(*e.get());
}

auto CryptoKeyRSA::algorithm() const -> KeyAlgorithm
{
    auto modulusLength = keySizeInBits();
    auto publicExponent = rsaPublicExponent(m_platformKey.get());

    if (m_restrictedToSpecificHash) {
        CryptoRsaHashedKeyAlgorithm result;
        result.name = CryptoAlgorithmRegistry::singleton().name(algorithmIdentifier());
        result.modulusLength = modulusLength;
        result.publicExponent = Uint8Array::tryCreate(publicExponent.data(), publicExponent.size());
        result.hash.name = CryptoAlgorithmRegistry::singleton().name(m_hash);
        return result;
    }

    CryptoRsaKeyAlgorithm result;
    result.name = CryptoAlgorithmRegistry::singleton().name(algorithmIdentifier());
    result.modulusLength = modulusLength;
    result.publicExponent = Uint8Array::tryCreate(publicExponent.data(), publicExponent.size());
    return result;
}

std::unique_ptr<CryptoKeyRSAComponents> CryptoKeyRSA::exportData() const
{
    auto& rsa = MbedTLS::rsaContext(m_platformKey.get());
    MbedTLS::MPI n, p, q, d, e;

    switch (type()) {
    case CryptoKeyType::Public:
        if (mbedtls_rsa_export(&rsa, n.get(), nullptr, nullptr, nullptr, e.get()))
            return nullptr;
        return CryptoKeyRSAComponents::createPublic(MbedTLS::mpiData(*n.get()), MbedTLS::mpiData(*e.get()));
    case CryptoKeyType::Private: {
        MbedTLS::MPI dp, dq, qi;
        if (mbedtls_rsa_export(&rsa, n.get(), p.get(), q.get(), d.get(), e.get()) || mbedtls_rsa_export_crt(&rsa, dp.get(), dq.get(), qi.get()))
            return nullptr;

        CryptoKeyRSAComponents::PrimeInfo firstPrimeInfo;
        firstPrimeInfo.primeFactor = MbedTLS::mpiData(*p.get());
        firstPrimeInfo.factorCRTExponent = MbedTLS::mpiData(*dp.get());

        // qi is q^(-1) mod p.
        CryptoKeyRSAComponents::PrimeInfo secondPrimeInfo;
        secondPrimeInfo.primeFactor = MbedTLS::mpiData(*q.get());
        secondPrimeInfo.factorCRTExponent = MbedTLS::mpiData(*dq.get());
        secondPrimeInfo.factorCRTCoefficient = MbedTLS::mpiData(*qi.get());

        return CryptoKeyRSAComponents::createPrivateWithAdditionalData(
            MbedTLS::mpiData(*n.get()), MbedTLS::mpiData(*e.get()), MbedTLS::mpiData(*d.get()),
            WTFMove(firstPrimeInfo), WTFMove(secondPrimeInfo), Vector<CryptoKeyRSAComponents::PrimeInfo> { });
    }
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO) && USE(MBEDTLS)
