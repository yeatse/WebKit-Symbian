/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "MbedTLSECKey.h"
#include "MbedTLSRSADER.h"
#include <mbedtls/platform_util.h>

namespace WebCore::MbedTLS::ECDER {

// Share the strict, bounded DER reader and PKCS8 attribute validation with RSA.
using RSADER::Reader;
using RSADER::wrap;
constexpr size_t maximumDERSize = 4096;
constexpr std::array<uint8_t, 7> ecPublicKeyOID { 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01 };
constexpr std::array<uint8_t, 8> p256OID { 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07 };
constexpr std::array<uint8_t, 5> p384OID { 0x2b, 0x81, 0x04, 0x00, 0x22 };
constexpr std::array<uint8_t, 5> p521OID { 0x2b, 0x81, 0x04, 0x00, 0x23 };

inline std::span<const uint8_t> curveOID(mbedtls_ecp_group_id id)
{
    switch (id) {
    case MBEDTLS_ECP_DP_SECP256R1: return p256OID;
    case MBEDTLS_ECP_DP_SECP384R1: return p384OID;
    case MBEDTLS_ECP_DP_SECP521R1: return p521OID;
    default: return { };
    }
}

inline bool namedCurve(Reader& fields, mbedtls_ecp_group_id id)
{
    auto oid = fields.read(0x06);
    return oid && !curveOID(id).empty() && std::ranges::equal(*oid, curveOID(id));
}

inline bool algorithm(Reader& parent, mbedtls_ecp_group_id id)
{
    auto sequence = parent.read(0x30);
    if (!sequence)
        return false;
    Reader fields(*sequence);
    auto oid = fields.read(0x06);
    return oid && std::ranges::equal(*oid, ecPublicKeyOID) && namedCurve(fields, id) && fields.empty();
}

inline std::optional<std::span<const uint8_t>> publicPoint(Reader& fields)
{
    auto bits = fields.read(0x03);
    if (!bits || bits->empty() || (*bits)[0])
        return std::nullopt;
    return bits->subspan(1);
}

inline ECKey importKey(mbedtls_ecp_group_id id, std::span<const uint8_t> input, bool isPrivate, ECRandom random)
{
    if (input.size() > maximumDERSize || !supportedECCurve(id))
        return nullptr;
    Reader outer(input);
    auto sequence = outer.read(0x30);
    if (!sequence || !outer.empty())
        return nullptr;
    Reader fields(*sequence);
    if (isPrivate) {
        auto version = fields.read(0x02);
        if (!version || version->size() != 1 || (*version)[0])
            return nullptr;
    }
    if (!algorithm(fields, id))
        return nullptr;
    if (!isPrivate) {
        auto point = publicPoint(fields);
        return point && fields.empty() ? importECPublic(id, *point) : nullptr;
    }
    auto octets = fields.read(0x04);
    if (!octets || (!fields.empty() && !RSADER::attributes(fields)) || !fields.empty())
        return nullptr;
    Reader inner(*octets);
    auto privateSequence = inner.read(0x30);
    if (!privateSequence || !inner.empty())
        return nullptr;
    Reader privateFields(*privateSequence);
    auto version = privateFields.read(0x02);
    auto d = privateFields.read(0x04);
    if (!version || version->size() != 1 || (*version)[0] != 1 || !d)
        return nullptr;
    if (auto parameters = privateFields.read(0xa0)) {
        Reader parameterFields(*parameters);
        if (!namedCurve(parameterFields, id) || !parameterFields.empty())
            return nullptr;
    }
    std::span<const uint8_t> point;
    if (auto publicKey = privateFields.read(0xa1)) {
        Reader publicFields(*publicKey);
        auto bits = publicPoint(publicFields);
        if (!bits || bits->empty() || !publicFields.empty())
            return nullptr;
        point = *bits;
    }
    if (!privateFields.empty())
        return nullptr;
    return importECPrivate(id, *d, point, random);
}

class EraseBytes {
public:
    explicit EraseBytes(std::vector<uint8_t>& bytes) : m_bytes(bytes) { }
    ~EraseBytes() { mbedtls_platform_zeroize(m_bytes.data(), m_bytes.size()); }
private:
    std::vector<uint8_t>& m_bytes;
};

inline std::vector<uint8_t> exportKey(const ECKeyData& key, bool isPrivate)
{
    auto curve = curveOID(key.group.id);
    auto point = exportECPublic(key);
    if (curve.empty() || point.empty())
        return { };
    auto oid = wrap(0x06, ecPublicKeyOID);
    auto parameters = wrap(0x06, curve);
    oid.insert(oid.end(), parameters.begin(), parameters.end());
    auto algorithmIdentifier = wrap(0x30, oid);
    point.insert(point.begin(), 0); // BIT STRING has no unused bits.
    auto publicKey = wrap(0x03, point);
    if (!isPrivate) {
        algorithmIdentifier.insert(algorithmIdentifier.end(), publicKey.begin(), publicKey.end());
        return wrap(0x30, algorithmIdentifier);
    }
    auto d = exportECPrivate(key);
    EraseBytes eraseD(d);
    if (d.empty())
        return { };
    auto privateOctets = wrap(0x04, d);
    EraseBytes eraseOctets(privateOctets);
    auto publicField = wrap(0xa1, publicKey);
    std::vector<uint8_t> privateFields { 0x02, 0x01, 0x01 };
    EraseBytes erasePrivateFields(privateFields);
    privateFields.reserve(privateFields.size() + privateOctets.size() + publicField.size());
    privateFields.insert(privateFields.end(), privateOctets.begin(), privateOctets.end());
    privateFields.insert(privateFields.end(), publicField.begin(), publicField.end());
    auto privateSequence = wrap(0x30, privateFields);
    EraseBytes erasePrivateSequence(privateSequence);
    auto privateKey = wrap(0x04, privateSequence);
    EraseBytes erasePrivateKey(privateKey);
    std::vector<uint8_t> fields { 0x02, 0x01, 0x00 };
    EraseBytes eraseFields(fields);
    fields.reserve(fields.size() + algorithmIdentifier.size() + privateKey.size());
    fields.insert(fields.end(), algorithmIdentifier.begin(), algorithmIdentifier.end());
    fields.insert(fields.end(), privateKey.begin(), privateKey.end());
    return wrap(0x30, fields);
}

} // namespace WebCore::MbedTLS::ECDER
