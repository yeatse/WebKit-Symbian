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
#include "CryptoKeyEC.h"

#if ENABLE(WEB_CRYPTO) && USE(MBEDTLS)

#include "CryptoKeyPair.h"
#include "JsonWebKey.h"
#include "MbedTLSCryptoUtilities.h"
#include <wtf/text/Base64.h>

namespace WebCore {

static mbedtls_ecp_group_id groupIdentifier(CryptoKeyEC::NamedCurve curve)
{
    switch (curve) {
    case CryptoKeyEC::NamedCurve::P256:
        return MBEDTLS_ECP_DP_SECP256R1;
    case CryptoKeyEC::NamedCurve::P384:
        return MBEDTLS_ECP_DP_SECP384R1;
    case CryptoKeyEC::NamedCurve::P521:
        return MBEDTLS_ECP_DP_SECP521R1;
    }
    ASSERT_NOT_REACHED();
    return MBEDTLS_ECP_DP_NONE;
}

static size_t curveBits(CryptoKeyEC::NamedCurve curve)
{
    switch (curve) {
    case CryptoKeyEC::NamedCurve::P256:
        return 256;
    case CryptoKeyEC::NamedCurve::P384:
        return 384;
    case CryptoKeyEC::NamedCurve::P521:
        return 521;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

static size_t fieldElementSize(CryptoKeyEC::NamedCurve curve)
{
    return (curveBits(curve) + 7) / 8;
}

static size_t uncompressedPointSize(CryptoKeyEC::NamedCurve curve)
{
    return 1 + 2 * fieldElementSize(curve);
}

// A key on `curve` with nothing set but the group.
static PlatformECKeyContainer createECKey(CryptoKeyEC::NamedCurve curve)
{
    auto context = MbedTLS::createPKContext(MBEDTLS_PK_ECKEY);
    if (!context || mbedtls_ecp_group_load(&MbedTLS::ecGroup(MbedTLS::ecKeyPair(context.get())), groupIdentifier(curve)))
        return nullptr;
    return context;
}

// Copies an EC key parsed from SPKI or PKCS #8 into an id-ecPublicKey one, so that
// keys imported with the id-ecDH identifier export as the other EC keys do.
static PlatformECKeyContainer copyECKey(CryptoKeyEC::NamedCurve curve, mbedtls_pk_context* source, bool includePrivate)
{
    auto type = mbedtls_pk_get_type(source);
    if (type != MBEDTLS_PK_ECKEY && type != MBEDTLS_PK_ECKEY_DH)
        return nullptr;
    auto& sourceKey = MbedTLS::ecKeyPair(source);
    if (MbedTLS::ecGroup(sourceKey).id != groupIdentifier(curve))
        return nullptr;

    auto context = createECKey(curve);
    if (!context)
        return nullptr;
    auto& key = MbedTLS::ecKeyPair(context.get());
    if (mbedtls_ecp_copy(&MbedTLS::ecPublicPoint(key), &MbedTLS::ecPublicPoint(sourceKey)))
        return nullptr;
    if (includePrivate && mbedtls_mpi_copy(&MbedTLS::ecPrivateValue(key), &MbedTLS::ecPrivateValue(sourceKey)))
        return nullptr;
    return context;
}

static bool readPublicPoint(mbedtls_ecp_keypair& key, const uint8_t* data, size_t size)
{
    auto& group = MbedTLS::ecGroup(key);
    auto& point = MbedTLS::ecPublicPoint(key);
    return !mbedtls_ecp_point_read_binary(&group, &point, data, size) && !mbedtls_ecp_check_pubkey(&group, &point);
}

size_t CryptoKeyEC::keySizeInBits() const
{
    return curveBits(m_curve);
}

bool CryptoKeyEC::platformSupportedCurve(NamedCurve)
{
    // P-256, P-384 and P-521 are all in the SDK's mbedTLS.
    return true;
}

Optional<CryptoKeyPair> CryptoKeyEC::platformGeneratePair(CryptoAlgorithmIdentifier identifier, NamedCurve curve, bool extractable, CryptoKeyUsageBitmap usages)
{
    MbedTLS::Random random;
    if (!random)
        return WTF::nullopt;

    auto privateContext = MbedTLS::createPKContext(MBEDTLS_PK_ECKEY);
    if (!privateContext)
        return WTF::nullopt;
    if (mbedtls_ecp_gen_key(groupIdentifier(curve), &MbedTLS::ecKeyPair(privateContext.get()), MbedTLS::Random::generate, random.context()))
        return WTF::nullopt;

    auto publicContext = copyECKey(curve, privateContext.get(), false);
    if (!publicContext)
        return WTF::nullopt;

    auto publicKey = CryptoKeyEC::create(identifier, curve, CryptoKeyType::Public, WTFMove(publicContext), true, usages);
    auto privateKey = CryptoKeyEC::create(identifier, curve, CryptoKeyType::Private, WTFMove(privateContext), extractable, usages);
    return CryptoKeyPair { WTFMove(publicKey), WTFMove(privateKey) };
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportRaw(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    if (keyData.size() != uncompressedPointSize(curve))
        return nullptr;

    auto context = createECKey(curve);
    if (!context || !readPublicPoint(MbedTLS::ecKeyPair(context.get()), keyData.data(), keyData.size()))
        return nullptr;

    return create(identifier, curve, CryptoKeyType::Public, WTFMove(context), extractable, usages);
}

static Optional<Vector<uint8_t>> uncompressedPoint(CryptoKeyEC::NamedCurve curve, const Vector<uint8_t>& x, const Vector<uint8_t>& y)
{
    size_t size = fieldElementSize(curve);
    if (x.size() != size || y.size() != size)
        return WTF::nullopt;

    Vector<uint8_t> point;
    point.reserveInitialCapacity(1 + 2 * size);
    point.append(0x04);
    point.appendVector(x);
    point.appendVector(y);
    return point;
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportJWKPublic(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& x, Vector<uint8_t>&& y, bool extractable, CryptoKeyUsageBitmap usages)
{
    auto point = uncompressedPoint(curve, x, y);
    if (!point)
        return nullptr;

    return platformImportRaw(identifier, curve, WTFMove(*point), extractable, usages);
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportJWKPrivate(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& x, Vector<uint8_t>&& y, Vector<uint8_t>&& d, bool extractable, CryptoKeyUsageBitmap usages)
{
    auto point = uncompressedPoint(curve, x, y);
    if (!point || d.size() != fieldElementSize(curve))
        return nullptr;

    auto context = MbedTLS::createPKContext(MBEDTLS_PK_ECKEY);
    if (!context)
        return nullptr;
    auto& key = MbedTLS::ecKeyPair(context.get());
    // This loads the group and checks that d is in range.
    if (mbedtls_ecp_read_key(groupIdentifier(curve), &key, d.data(), d.size()))
        return nullptr;
    if (!readPublicPoint(key, point->data(), point->size()))
        return nullptr;

    // The public point has to be the one d gives.
    MbedTLS::Random random;
    if (!random || mbedtls_ecp_check_pub_priv(&key, &key, MbedTLS::Random::generate, random.context()))
        return nullptr;

    return create(identifier, curve, CryptoKeyType::Private, WTFMove(context), extractable, usages);
}

// ECDSA keys must use id-ecPublicKey; ECDH keys may also use id-ecDH.
static bool supportedKeyType(CryptoAlgorithmIdentifier identifier, mbedtls_pk_context* context)
{
    switch (mbedtls_pk_get_type(context)) {
    case MBEDTLS_PK_ECKEY:
        return true;
    case MBEDTLS_PK_ECKEY_DH:
        return identifier == CryptoAlgorithmIdentifier::ECDH;
    default:
        return false;
    }
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportSpki(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    auto parsed = MbedTLS::parseSubjectPublicKeyInfo(keyData);
    if (!parsed || !supportedKeyType(identifier, parsed.get()))
        return nullptr;

    auto context = copyECKey(curve, parsed.get(), false);
    if (!context)
        return nullptr;

    return create(identifier, curve, CryptoKeyType::Public, WTFMove(context), extractable, usages);
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportPkcs8(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    auto parsed = MbedTLS::parsePrivateKeyInfo(keyData);
    if (!parsed || !supportedKeyType(identifier, parsed.get()))
        return nullptr;

    auto context = copyECKey(curve, parsed.get(), true);
    if (!context)
        return nullptr;

    return create(identifier, curve, CryptoKeyType::Private, WTFMove(context), extractable, usages);
}

Vector<uint8_t> CryptoKeyEC::platformExportRaw() const
{
    auto& key = MbedTLS::ecKeyPair(m_platformKey.get());
    Vector<uint8_t> result(uncompressedPointSize(m_curve));
    size_t length = 0;
    if (mbedtls_ecp_point_write_binary(&MbedTLS::ecGroup(key), &MbedTLS::ecPublicPoint(key), MBEDTLS_ECP_PF_UNCOMPRESSED, &length, result.data(), result.size())
        || length != result.size())
        return { };
    return result;
}

bool CryptoKeyEC::platformAddFieldElements(JsonWebKey& jwk) const
{
    auto point = platformExportRaw();
    if (point.isEmpty())
        return false;

    size_t size = fieldElementSize(m_curve);
    jwk.x = WTF::base64URLEncode(point.data() + 1, size);
    jwk.y = WTF::base64URLEncode(point.data() + 1 + size, size);

    if (type() == Type::Private) {
        auto d = MbedTLS::mpiData(MbedTLS::ecPrivateValue(MbedTLS::ecKeyPair(m_platformKey.get())), size);
        if (d.isEmpty())
            return false;
        jwk.d = WTF::base64URLEncode(d);
    }
    return true;
}

Vector<uint8_t> CryptoKeyEC::platformExportSpki() const
{
    return MbedTLS::subjectPublicKeyInfo(m_platformKey.get());
}

Vector<uint8_t> CryptoKeyEC::platformExportPkcs8() const
{
    return MbedTLS::privateKeyInfo(m_platformKey.get());
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO) && USE(MBEDTLS)
