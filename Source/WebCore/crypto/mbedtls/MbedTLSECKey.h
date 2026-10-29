/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <cstdint>
#include <mbedtls/ecp.h>
#include <mbedtls/platform_util.h>
#include <memory>
#include <span>
#include <vector>

namespace WebCore::MbedTLS {

using ECRandom = int (*)(void*, unsigned char*, size_t);

// Each operation receives its own group, including mbedTLS's mutable cached
// multiplication tables. A CryptoKey never shares mutable mbedTLS state.
struct ECKeyData {
    ECKeyData()
    {
        mbedtls_ecp_group_init(&group);
        mbedtls_mpi_init(&d);
        mbedtls_ecp_point_init(&Q);
    }
    ~ECKeyData()
    {
        mbedtls_ecp_group_free(&group);
        mbedtls_mpi_free(&d);
        mbedtls_ecp_point_free(&Q);
    }
    ECKeyData(const ECKeyData&) = delete;
    ECKeyData& operator=(const ECKeyData&) = delete;
    mbedtls_ecp_group group;
    mbedtls_mpi d;
    mbedtls_ecp_point Q;
};

using ECKey = std::unique_ptr<ECKeyData>;

inline bool supportedECCurve(mbedtls_ecp_group_id id)
{
    return (id == MBEDTLS_ECP_DP_SECP256R1 || id == MBEDTLS_ECP_DP_SECP384R1 || id == MBEDTLS_ECP_DP_SECP521R1)
        && mbedtls_ecp_curve_info_from_grp_id(id);
}

inline ECKey makeECKey(mbedtls_ecp_group_id id)
{
    if (!supportedECCurve(id))
        return nullptr;
    auto result = std::make_unique<ECKeyData>();
    if (mbedtls_ecp_group_load(&result->group, id))
        return nullptr;
    return result;
}

inline size_t ecFieldBytes(const ECKeyData& key)
{
    return (key.group.pbits + 7) / 8;
}

inline ECKey copyECKey(const ECKeyData& source, bool includePrivate)
{
    auto result = makeECKey(source.group.id);
    if (!result || mbedtls_ecp_copy(&result->Q, &source.Q)
        || (includePrivate && mbedtls_mpi_copy(&result->d, &source.d)))
        return nullptr;
    return result;
}

inline ECKey importECPublic(mbedtls_ecp_group_id id, std::span<const uint8_t> raw)
{
    auto key = makeECKey(id);
    // WebCrypto exports the fixed-width, uncompressed SEC1 point format.
    if (!key || raw.size() != 1 + 2 * ecFieldBytes(*key) || raw[0] != 4
        || mbedtls_ecp_point_read_binary(&key->group, &key->Q, raw.data(), raw.size())
        || mbedtls_ecp_check_pubkey(&key->group, &key->Q))
        return nullptr;
    return key;
}

inline ECKey importECPrivate(mbedtls_ecp_group_id id, std::span<const uint8_t> d, std::span<const uint8_t> rawPublic, ECRandom random)
{
    auto key = makeECKey(id);
    if (!key || !random || d.size() != ecFieldBytes(*key)
        || mbedtls_mpi_read_binary(&key->d, d.data(), d.size())
        || mbedtls_ecp_check_privkey(&key->group, &key->d)
        || mbedtls_ecp_mul(&key->group, &key->Q, &key->d, &key->group.G, random, nullptr))
        return nullptr;
    if (!rawPublic.empty()) {
        auto supplied = importECPublic(id, rawPublic);
        if (!supplied || mbedtls_ecp_point_cmp(&key->Q, &supplied->Q))
            return nullptr;
    }
    return key;
}

inline ECKey generateECKey(mbedtls_ecp_group_id id, ECRandom random)
{
    auto key = makeECKey(id);
    if (!key || !random || mbedtls_ecp_gen_keypair(&key->group, &key->d, &key->Q, random, nullptr))
        return nullptr;
    return key;
}

inline std::vector<uint8_t> exportECPublic(const ECKeyData& key)
{
    std::vector<uint8_t> result(1 + 2 * ecFieldBytes(key));
    size_t length = 0;
    if (mbedtls_ecp_point_write_binary(&key.group, &key.Q, MBEDTLS_ECP_PF_UNCOMPRESSED, &length, result.data(), result.size()) || length != result.size())
        return { };
    return result;
}

inline std::vector<uint8_t> exportECPrivate(const ECKeyData& key)
{
    if (mbedtls_ecp_check_privkey(&key.group, &key.d))
        return { };
    std::vector<uint8_t> result(ecFieldBytes(key));
    if (mbedtls_mpi_write_binary(&key.d, result.data(), result.size())) {
        mbedtls_platform_zeroize(result.data(), result.size());
        return { };
    }
    return result;
}

} // namespace WebCore::MbedTLS
