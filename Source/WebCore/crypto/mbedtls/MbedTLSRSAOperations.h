/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "MbedTLSRSAKey.h"
#include <climits>
#include <mbedtls/md.h>
#include <mbedtls/platform_util.h>
#include <optional>
#include <vector>

namespace WebCore::MbedTLS {

using RandomFunction = int (*)(void*, unsigned char*, size_t);

inline const uint8_t* readableBytes(std::span<const uint8_t> bytes)
{
    static constexpr uint8_t empty = 0;
    return bytes.empty() ? &empty : bytes.data();
}

inline std::optional<std::vector<uint8_t>> rsaDigest(mbedtls_md_type_t hash, std::span<const uint8_t> data)
{
    auto* info = mbedtls_md_info_from_type(hash);
    if (!info)
        return std::nullopt;
    std::vector<uint8_t> result(mbedtls_md_get_size(info));
    if (mbedtls_md(info, readableBytes(data), data.size(), result.data()))
        return std::nullopt;
    return result;
}

inline bool validPSSSalt(const mbedtls_rsa_context* key, size_t hashLength, size_t saltLength)
{
    size_t bits = rsaModulusBits(key);
    if (!bits || saltLength > INT_MAX)
        return false;
    // emBits = modBits - 1; round up without allowing size_t wrap.
    size_t encodedLength = (bits - 1 + 7) / 8;
    return encodedLength >= hashLength + 2 && saltLength <= encodedLength - hashLength - 2;
}

inline std::optional<std::vector<uint8_t>> rsaSign(const mbedtls_rsa_context* original, mbedtls_md_type_t hash, std::span<const uint8_t> data, std::optional<size_t> saltLength, RandomFunction random, void* randomContext = nullptr)
{
    auto digest = rsaDigest(hash, data);
    if (!digest || !random || (saltLength && !validPSSSalt(original, digest->size(), *saltLength)))
        return std::nullopt;
    auto key = copyRSAKey(original);
    if (!key || mbedtls_rsa_set_padding(key.get(), saltLength ? MBEDTLS_RSA_PKCS_V21 : MBEDTLS_RSA_PKCS_V15, hash))
        return std::nullopt;
    std::vector<uint8_t> result(mbedtls_rsa_get_len(key.get()));
    int error;
    if (saltLength)
        error = mbedtls_rsa_rsassa_pss_sign_ext(key.get(), random, randomContext, hash, digest->size(), digest->data(), static_cast<int>(*saltLength), result.data());
    else
        error = mbedtls_rsa_rsassa_pkcs1_v15_sign(key.get(), random, randomContext, hash, digest->size(), digest->data(), result.data());
    if (error)
        return std::nullopt;
    return result;
}

inline std::optional<bool> rsaVerify(const mbedtls_rsa_context* original, mbedtls_md_type_t hash, std::span<const uint8_t> signature, std::span<const uint8_t> data, std::optional<size_t> saltLength)
{
    if (signature.size() != mbedtls_rsa_get_len(original))
        return false;
    auto digest = rsaDigest(hash, data);
    if (!digest)
        return std::nullopt;
    if (saltLength && !validPSSSalt(original, digest->size(), *saltLength))
        return false;
    auto key = copyRSAKey(original);
    if (!key || mbedtls_rsa_set_padding(key.get(), saltLength ? MBEDTLS_RSA_PKCS_V21 : MBEDTLS_RSA_PKCS_V15, hash))
        return std::nullopt;
    int error;
    if (saltLength)
        error = mbedtls_rsa_rsassa_pss_verify_ext(key.get(), hash, digest->size(), digest->data(), hash, static_cast<int>(*saltLength), signature.data());
    else
        error = mbedtls_rsa_rsassa_pkcs1_v15_verify(key.get(), hash, digest->size(), digest->data(), signature.data());
    if (!error)
        return true;
    if (error == MBEDTLS_ERR_RSA_VERIFY_FAILED || error == MBEDTLS_ERR_RSA_INVALID_PADDING
        || error == MBEDTLS_ERR_RSA_BAD_INPUT_DATA
        || error == MBEDTLS_ERR_RSA_PUBLIC_FAILED + MBEDTLS_ERR_MPI_BAD_INPUT_DATA)
        return false;
    return std::nullopt;
}

inline std::optional<std::vector<uint8_t>> rsaOAEP(const mbedtls_rsa_context* original, mbedtls_md_type_t hash, std::span<const uint8_t> data, std::span<const uint8_t> label, bool decrypt, RandomFunction random, void* randomContext = nullptr)
{
    auto* info = mbedtls_md_info_from_type(hash);
    size_t length = mbedtls_rsa_get_len(original);
    if (!info || !random || !length || length > MBEDTLS_MPI_MAX_SIZE)
        return std::nullopt;
    size_t hashLength = mbedtls_md_get_size(info);
    if (length < 2 * hashLength + 2 || (decrypt ? data.size() != length : data.size() > length - 2 * hashLength - 2))
        return std::nullopt;
    auto key = copyRSAKey(original);
    if (!key || mbedtls_rsa_set_padding(key.get(), MBEDTLS_RSA_PKCS_V21, hash))
        return std::nullopt;
    std::vector<uint8_t> result(length);
    int error;
    if (decrypt)
        error = mbedtls_rsa_rsaes_oaep_decrypt(key.get(), random, randomContext, readableBytes(label), label.size(), &length, data.data(), result.data(), result.size());
    else
        error = mbedtls_rsa_rsaes_oaep_encrypt(key.get(), random, randomContext, readableBytes(label), label.size(), data.size(), readableBytes(data), result.data());
    if (error) {
        mbedtls_platform_zeroize(result.data(), result.size());
        return std::nullopt;
    }
    result.resize(length);
    return result;
}

} // namespace WebCore::MbedTLS
