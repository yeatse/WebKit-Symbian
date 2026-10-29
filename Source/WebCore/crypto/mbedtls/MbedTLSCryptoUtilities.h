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

#pragma once

#if ENABLE(WEB_CRYPTO) && USE(MBEDTLS)

#include "CryptoAlgorithmIdentifier.h"
#include "MbedTLSPKContext.h"
#include <mbedtls/bignum.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <wtf/Optional.h>
#include <wtf/Vector.h>

namespace WebCore {
namespace MbedTLS {

Optional<mbedtls_md_type_t> hashType(CryptoAlgorithmIdentifier);
Optional<Vector<uint8_t>> digest(CryptoAlgorithmIdentifier, const Vector<uint8_t>&);

// The big-endian value of an MPI, left-padded with zeros to `size` bytes when
// `size` is given. Empty on failure.
Vector<uint8_t> mpiData(const mbedtls_mpi&, size_t size = 0);

class MPI {
    WTF_MAKE_NONCOPYABLE(MPI);
public:
    MPI() { mbedtls_mpi_init(&m_mpi); }
    ~MPI() { mbedtls_mpi_free(&m_mpi); }
    mbedtls_mpi* get() { return &m_mpi; }

private:
    mbedtls_mpi m_mpi;
};

// A CTR_DRBG seeded from the platform entropy source. mbedTLS is built without
// MBEDTLS_THREADING_C and Web Crypto runs on work queues, so each operation that
// needs randomness gets its own.
class Random {
    WTF_MAKE_NONCOPYABLE(Random);
public:
    Random();
    ~Random();

    explicit operator bool() const { return m_seeded; }
    void* context() { return &m_drbg; }
    static int generate(void* context, unsigned char* output, size_t length);

private:
    mbedtls_entropy_context m_entropy;
    mbedtls_ctr_drbg_context m_drbg;
    bool m_seeded { false };
};

// An owned mbedtls_pk_context, as kept in PlatformECKeyContainer and PlatformRSAKeyContainer.
MbedTLSPKContext createPKContext(mbedtls_pk_type_t);

// DER SubjectPublicKeyInfo and PKCS #8 PrivateKeyInfo. Parsing returns null on
// malformed input; writing returns an empty vector on failure.
MbedTLSPKContext parseSubjectPublicKeyInfo(const Vector<uint8_t>&);
MbedTLSPKContext parsePrivateKeyInfo(const Vector<uint8_t>&);
Vector<uint8_t> subjectPublicKeyInfo(mbedtls_pk_context*);
Vector<uint8_t> privateKeyInfo(mbedtls_pk_context*);

inline mbedtls_ecp_keypair& ecKeyPair(mbedtls_pk_context* context) { return *mbedtls_pk_ec(*context); }
inline mbedtls_ecp_group& ecGroup(mbedtls_ecp_keypair& key) { return key.MBEDTLS_PRIVATE(grp); }
inline mbedtls_mpi& ecPrivateValue(mbedtls_ecp_keypair& key) { return key.MBEDTLS_PRIVATE(d); }
inline mbedtls_ecp_point& ecPublicPoint(mbedtls_ecp_keypair& key) { return key.MBEDTLS_PRIVATE(Q); }
inline mbedtls_rsa_context& rsaContext(mbedtls_pk_context* context) { return *mbedtls_pk_rsa(*context); }

} // namespace MbedTLS
} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO) && USE(MBEDTLS)
