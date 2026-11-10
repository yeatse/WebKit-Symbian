/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "config.h"
#include "CryptoKeyEC.h"

#if ENABLE(WEB_CRYPTO) && USE(MBEDTLS)

#include "JsonWebKey.h"
#include "MbedTLSECDER.h"
#include "MbedTLSECRandom.h"
#include <wtf/Scope.h>
#include <wtf/text/Base64.h>

namespace WebCore {

static mbedtls_ecp_group_id groupID(CryptoKeyEC::NamedCurve curve)
{
    switch (curve) {
    case CryptoKeyEC::NamedCurve::P256: return MBEDTLS_ECP_DP_SECP256R1;
    case CryptoKeyEC::NamedCurve::P384: return MBEDTLS_ECP_DP_SECP384R1;
    case CryptoKeyEC::NamedCurve::P521: return MBEDTLS_ECP_DP_SECP521R1;
    }
    return MBEDTLS_ECP_DP_NONE;
}

static CryptoKeyUsageBitmap allowedUsages(CryptoAlgorithmIdentifier identifier, bool isPrivate)
{
    if (identifier == CryptoAlgorithmIdentifier::ECDSA)
        return isPrivate ? CryptoKeyUsageSign : CryptoKeyUsageVerify;
    return isPrivate ? CryptoKeyUsageDeriveKey | CryptoKeyUsageDeriveBits : 0;
}

static bool validUsages(CryptoAlgorithmIdentifier identifier, bool isPrivate, CryptoKeyUsageBitmap usages)
{
    return CryptoKeyEC::isValidECAlgorithm(identifier) && !(usages & ~allowedUsages(identifier, isPrivate));
}

bool CryptoKeyEC::platformSupportedCurve(NamedCurve curve)
{
    return MbedTLS::supportedECCurve(groupID(curve));
}

size_t CryptoKeyEC::keySizeInBits() const
{
    return m_platformKey->group.pbits;
}

std::optional<CryptoKeyPair> CryptoKeyEC::platformGeneratePair(CryptoAlgorithmIdentifier identifier, NamedCurve curve, bool extractable, CryptoKeyUsageBitmap usages)
{
    if (!isValidECAlgorithm(identifier) || (usages & ~(allowedUsages(identifier, true) | allowedUsages(identifier, false))))
        return std::nullopt;
    auto privateKey = MbedTLS::generateECKey(groupID(curve), MbedTLS::ecRandom);
    if (!privateKey)
        return std::nullopt;
    auto publicKey = MbedTLS::copyECKey(*privateKey, false);
    if (!publicKey)
        return std::nullopt;
    auto publicResult = create(identifier, curve, CryptoKeyType::Public, WTF::move(publicKey), true, usages & allowedUsages(identifier, false));
    auto privateResult = create(identifier, curve, CryptoKeyType::Private, WTF::move(privateKey), extractable, usages & allowedUsages(identifier, true));
    return CryptoKeyPair { WTF::move(publicResult), WTF::move(privateResult) };
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportRaw(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& data, bool extractable, CryptoKeyUsageBitmap usages)
{
    if (!validUsages(identifier, false, usages))
        return nullptr;
    auto key = MbedTLS::importECPublic(groupID(curve), data.span());
    if (!key)
        return nullptr;
    return create(identifier, curve, CryptoKeyType::Public, WTF::move(key), extractable, usages);
}

static Vector<uint8_t> rawCoordinates(CryptoKeyEC::NamedCurve curve, const Vector<uint8_t>& x, const Vector<uint8_t>& y)
{
    auto info = mbedtls_ecp_curve_info_from_grp_id(groupID(curve));
    if (!info || x.size() != (info->bit_size + 7) / 8 || y.size() != x.size())
        return { };
    Vector<uint8_t> result;
    result.reserveInitialCapacity(1 + x.size() + y.size());
    result.append(4);
    result.appendVector(x);
    result.appendVector(y);
    return result;
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportJWKPublic(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& x, Vector<uint8_t>&& y, bool extractable, CryptoKeyUsageBitmap usages)
{
    return platformImportRaw(identifier, curve, rawCoordinates(curve, x, y), extractable, usages);
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportJWKPrivate(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& x, Vector<uint8_t>&& y, Vector<uint8_t>&& d, bool extractable, CryptoKeyUsageBitmap usages)
{
    auto eraseD = makeScopeExit([&] { mbedtls_platform_zeroize(d.mutableSpan().data(), d.size()); });
    if (!validUsages(identifier, true, usages))
        return nullptr;
    auto point = rawCoordinates(curve, x, y);
    if (point.isEmpty())
        return nullptr;
    auto key = MbedTLS::importECPrivate(groupID(curve), d.span(), point.span(), MbedTLS::ecRandom);
    if (!key)
        return nullptr;
    return create(identifier, curve, CryptoKeyType::Private, WTF::move(key), extractable, usages);
}

static RefPtr<CryptoKeyEC> importDER(CryptoAlgorithmIdentifier identifier, CryptoKeyEC::NamedCurve curve, std::span<const uint8_t> data, bool isPrivate, bool extractable, CryptoKeyUsageBitmap usages)
{
    if (!validUsages(identifier, isPrivate, usages))
        return nullptr;
    auto key = MbedTLS::ECDER::importKey(groupID(curve), data, isPrivate, MbedTLS::ecRandom);
    if (!key)
        return nullptr;
    return CryptoKeyEC::create(identifier, curve, isPrivate ? CryptoKeyType::Private : CryptoKeyType::Public, WTF::move(key), extractable, usages);
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportSpki(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& data, bool extractable, CryptoKeyUsageBitmap usages)
{
    return importDER(identifier, curve, data.span(), false, extractable, usages);
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportPkcs8(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& data, bool extractable, CryptoKeyUsageBitmap usages)
{
    auto eraseData = makeScopeExit([&] { mbedtls_platform_zeroize(data.mutableSpan().data(), data.size()); });
    return importDER(identifier, curve, data.span(), true, extractable, usages);
}

Vector<uint8_t> CryptoKeyEC::platformExportRaw() const
{
    auto result = MbedTLS::exportECPublic(*m_platformKey);
    return Vector<uint8_t>(std::span<const uint8_t>(result));
}

bool CryptoKeyEC::platformAddFieldElements(JsonWebKey& jwk) const
{
    auto point = platformExportRaw();
    size_t width = keySizeInBytes();
    if (point.size() != 1 + 2 * width)
        return false;
    jwk.x = base64URLEncodeToString(point.span().subspan(1, width));
    jwk.y = base64URLEncodeToString(point.span().subspan(1 + width, width));
    if (type() == CryptoKeyType::Private) {
        auto d = MbedTLS::exportECPrivate(*m_platformKey);
        MbedTLS::ECDER::EraseBytes eraseD(d);
        if (d.size() != width)
            return false;
        jwk.d = base64URLEncodeToString(std::span<const uint8_t>(d));
    }
    return true;
}

Vector<uint8_t> CryptoKeyEC::platformExportSpki() const
{
    auto result = MbedTLS::ECDER::exportKey(*m_platformKey, false);
    return Vector<uint8_t>(std::span<const uint8_t>(result));
}

Vector<uint8_t> CryptoKeyEC::platformExportPkcs8() const
{
    auto result = MbedTLS::ECDER::exportKey(*m_platformKey, true);
    MbedTLS::ECDER::EraseBytes eraseResult(result);
    return Vector<uint8_t>(std::span<const uint8_t>(result));
}

} // namespace WebCore

#endif
