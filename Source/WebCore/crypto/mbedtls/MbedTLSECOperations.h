/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "MbedTLSECKey.h"
#include <array>
#include <mbedtls/ecdh.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/md.h>
#include <mbedtls/platform_util.h>
#include <optional>

namespace WebCore::MbedTLS {

class ECMPI {
public:
    ECMPI() { mbedtls_mpi_init(&m_value); }
    ~ECMPI() { mbedtls_mpi_free(&m_value); }
    ECMPI(const ECMPI&) = delete;
    ECMPI& operator=(const ECMPI&) = delete;
    mbedtls_mpi* get() { return &m_value; }
private:
    mbedtls_mpi m_value;
};

inline std::optional<std::vector<uint8_t>> ecSign(const ECKeyData& source, mbedtls_md_type_t hash, std::span<const uint8_t> data, ECRandom random)
{
    auto info = mbedtls_md_info_from_type(hash);
    if (!info || !random)
        return std::nullopt;
    auto key = copyECKey(source, true);
    if (!key || mbedtls_ecp_check_privkey(&key->group, &key->d))
        return std::nullopt;
    std::array<uint8_t, MBEDTLS_MD_MAX_SIZE> digest;
    if (mbedtls_md(info, data.data(), data.size(), digest.data()))
        return std::nullopt;
    ECMPI r, s;
    // The scalar API avoids ASN.1 ECDSA signatures: WebCrypto uses r || s,
    // with each unsigned scalar padded to the curve field size, even P-521.
    if (mbedtls_ecdsa_sign(&key->group, r.get(), s.get(), &key->d, digest.data(), mbedtls_md_get_size(info), random, nullptr))
        return std::nullopt;
    size_t width = ecFieldBytes(*key);
    std::vector<uint8_t> result(2 * width);
    if (mbedtls_mpi_write_binary(r.get(), result.data(), width)
        || mbedtls_mpi_write_binary(s.get(), result.data() + width, width))
        return std::nullopt;
    return result;
}

inline std::optional<bool> ecVerify(const ECKeyData& source, mbedtls_md_type_t hash, std::span<const uint8_t> signature, std::span<const uint8_t> data)
{
    size_t width = ecFieldBytes(source);
    if (signature.size() != 2 * width)
        return false;
    auto info = mbedtls_md_info_from_type(hash);
    auto key = copyECKey(source, false);
    if (!info || !key)
        return std::nullopt;
    std::array<uint8_t, MBEDTLS_MD_MAX_SIZE> digest;
    ECMPI r, s;
    if (mbedtls_md(info, data.data(), data.size(), digest.data())
        || mbedtls_mpi_read_binary(r.get(), signature.data(), width)
        || mbedtls_mpi_read_binary(s.get(), signature.data() + width, width))
        return std::nullopt;
    int status = mbedtls_ecdsa_verify(&key->group, digest.data(), mbedtls_md_get_size(info), &key->Q, r.get(), s.get());
    if (!status)
        return true;
    if (status == MBEDTLS_ERR_ECP_VERIFY_FAILED)
        return false;
    return std::nullopt;
}

inline std::optional<std::vector<uint8_t>> ecDerive(const ECKeyData& source, const ECKeyData& peer, ECRandom random)
{
    if (!random || source.group.id != peer.group.id)
        return std::nullopt;
    auto key = copyECKey(source, true);
    ECMPI secret;
    if (!key || mbedtls_ecp_check_privkey(&key->group, &key->d)
        || mbedtls_ecp_check_pubkey(&key->group, &peer.Q)
        || mbedtls_ecdh_compute_shared(&key->group, secret.get(), &peer.Q, &key->d, random, nullptr))
        return std::nullopt;
    std::vector<uint8_t> result(ecFieldBytes(*key));
    if (mbedtls_mpi_write_binary(secret.get(), result.data(), result.size())) {
        mbedtls_platform_zeroize(result.data(), result.size());
        return std::nullopt;
    }
    return result;
}

} // namespace WebCore::MbedTLS
