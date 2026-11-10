/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <algorithm>
#include <array>
#include <mbedtls/bignum.h>
#include <mbedtls/rsa.h>
#include <memory>
#include <span>

namespace WebCore::MbedTLS {

struct RSADeleter {
    void operator()(mbedtls_rsa_context* key) const
    {
        mbedtls_rsa_free(key);
        delete key;
    }
};

using RSAKey = std::unique_ptr<mbedtls_rsa_context, RSADeleter>;

inline RSAKey makeRSAKey()
{
    RSAKey result(new mbedtls_rsa_context);
    mbedtls_rsa_init(result.get());
    return result;
}

inline RSAKey copyRSAKey(const mbedtls_rsa_context* source)
{
    auto result = makeRSAKey();
    if (mbedtls_rsa_copy(result.get(), source))
        return nullptr;
    return result;
}

class MPI {
public:
    MPI() { mbedtls_mpi_init(&m_value); }
    ~MPI() { mbedtls_mpi_free(&m_value); }
    MPI(const MPI&) = delete;
    MPI& operator=(const MPI&) = delete;
    mbedtls_mpi* get() { return &m_value; }
    const mbedtls_mpi* get() const { return &m_value; }
private:
    mbedtls_mpi m_value;
};

// Borrowed, unsigned big-endian components; the caller retains the input bytes.
struct RSAParameters {
    enum Field : unsigned { N, E, D, P, Q, DP, DQ, QI, Count };
    std::array<std::span<const uint8_t>, Count> values;
    bool isPrivate { false };
    bool hasCRT { false };
};

inline bool equalInteger(const mbedtls_mpi* value, std::span<const uint8_t> bytes)
{
    MPI supplied;
    return !mbedtls_mpi_read_binary(supplied.get(), bytes.data(), bytes.size()) && !mbedtls_mpi_cmp_mpi(value, supplied.get());
}

inline RSAKey importRSA(const RSAParameters& parameters)
{
    using P = RSAParameters;
    unsigned count = parameters.isPrivate ? (parameters.hasCRT ? P::Count : P::P) : P::D;
    for (unsigned index = 0; index < count; ++index) {
        auto bytes = parameters.values[index];
        if (bytes.empty() || bytes.size() > MBEDTLS_MPI_MAX_SIZE || std::ranges::all_of(bytes, [](auto byte) { return !byte; }))
            return nullptr;
    }
    auto key = makeRSAKey();
    auto n = parameters.values[P::N];
    auto e = parameters.values[P::E];
    auto d = parameters.isPrivate ? parameters.values[P::D] : std::span<const uint8_t> { };
    auto p = parameters.hasCRT ? parameters.values[P::P] : std::span<const uint8_t> { };
    auto q = parameters.hasCRT ? parameters.values[P::Q] : std::span<const uint8_t> { };
    if (mbedtls_rsa_import_raw(key.get(), n.data(), n.size(), p.data(), p.size(), q.data(), q.size(), d.data(), d.size(), e.data(), e.size())
        || mbedtls_rsa_complete(key.get())
        || mbedtls_rsa_check_pubkey(key.get())
        || (parameters.isPrivate && mbedtls_rsa_check_privkey(key.get())))
        return nullptr;
    if (parameters.hasCRT) {
        MPI dp, dq, qi;
        if (mbedtls_rsa_export_crt(key.get(), dp.get(), dq.get(), qi.get())
            || !equalInteger(dp.get(), parameters.values[P::DP])
            || !equalInteger(dq.get(), parameters.values[P::DQ])
            || !equalInteger(qi.get(), parameters.values[P::QI]))
            return nullptr;
    }
    return key;
}

inline size_t rsaModulusBits(const mbedtls_rsa_context* key)
{
    MPI modulus;
    if (mbedtls_rsa_export(key, modulus.get(), nullptr, nullptr, nullptr, nullptr))
        return 0;
    return mbedtls_mpi_bitlen(modulus.get());
}

} // namespace WebCore::MbedTLS
