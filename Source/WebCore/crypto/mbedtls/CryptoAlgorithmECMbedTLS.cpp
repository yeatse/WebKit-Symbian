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

#if ENABLE(WEB_CRYPTO) && USE(MBEDTLS)

#include "CryptoAlgorithmECDH.h"
#include "CryptoAlgorithmECDSA.h"
#include "CryptoAlgorithmEcdsaParams.h"
#include "CryptoKeyEC.h"
#include "MbedTLSCryptoUtilities.h"
#include <mbedtls/ecdh.h>
#include <mbedtls/ecdsa.h>

namespace WebCore {

namespace {

// The operations take a non-const group, which mbedTLS caches precomputed
// points in, so each one gets its own rather than racing on the key's.
class ECGroup {
    WTF_MAKE_NONCOPYABLE(ECGroup);
public:
    explicit ECGroup(const CryptoKeyEC& key)
    {
        mbedtls_ecp_group_init(&m_group);
        m_loaded = !mbedtls_ecp_group_load(&m_group, MbedTLS::ecGroup(MbedTLS::ecKeyPair(key.platformKey())).id);
    }
    ~ECGroup() { mbedtls_ecp_group_free(&m_group); }

    explicit operator bool() const { return m_loaded; }
    mbedtls_ecp_group* get() { return &m_group; }

private:
    mbedtls_ecp_group m_group;
    bool m_loaded { false };
};

} // namespace

static size_t ecFieldSize(const CryptoKeyEC& key)
{
    return (key.keySizeInBits() + 7) / 8;
}

Optional<Vector<uint8_t>> CryptoAlgorithmECDH::platformDeriveBits(const CryptoKeyEC& baseKey, const CryptoKeyEC& publicKey)
{
    if (baseKey.namedCurve() != publicKey.namedCurve())
        return WTF::nullopt;

    ECGroup group(baseKey);
    MbedTLS::Random random;
    if (!group || !random)
        return WTF::nullopt;

    MbedTLS::MPI sharedSecret;
    auto& privateKey = MbedTLS::ecKeyPair(baseKey.platformKey());
    auto& peerKey = MbedTLS::ecKeyPair(publicKey.platformKey());
    if (mbedtls_ecdh_compute_shared(group.get(), sharedSecret.get(), &MbedTLS::ecPublicPoint(peerKey), &MbedTLS::ecPrivateValue(privateKey), MbedTLS::Random::generate, random.context()))
        return WTF::nullopt;

    auto result = MbedTLS::mpiData(*sharedSecret.get(), ecFieldSize(baseKey));
    if (result.isEmpty())
        return WTF::nullopt;
    return result;
}

// Signatures are r and s, each zero-padded to the field size, concatenated.
ExceptionOr<Vector<uint8_t>> CryptoAlgorithmECDSA::platformSign(const CryptoAlgorithmEcdsaParams& parameters, const CryptoKeyEC& key, const Vector<uint8_t>& data)
{
    auto hash = MbedTLS::digest(parameters.hashIdentifier, data);
    if (!hash)
        return Exception { OperationError };

    ECGroup group(key);
    MbedTLS::Random random;
    if (!group || !random)
        return Exception { OperationError };

    MbedTLS::MPI r, s;
    auto& keyPair = MbedTLS::ecKeyPair(key.platformKey());
    if (mbedtls_ecdsa_sign(group.get(), r.get(), s.get(), &MbedTLS::ecPrivateValue(keyPair), hash->data(), hash->size(), MbedTLS::Random::generate, random.context()))
        return Exception { OperationError };

    size_t size = ecFieldSize(key);
    auto signature = MbedTLS::mpiData(*r.get(), size);
    auto sData = MbedTLS::mpiData(*s.get(), size);
    if (signature.isEmpty() || sData.isEmpty())
        return Exception { OperationError };
    signature.appendVector(sData);
    return signature;
}

ExceptionOr<bool> CryptoAlgorithmECDSA::platformVerify(const CryptoAlgorithmEcdsaParams& parameters, const CryptoKeyEC& key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
    size_t size = ecFieldSize(key);
    if (signature.size() != 2 * size)
        return false;

    auto hash = MbedTLS::digest(parameters.hashIdentifier, data);
    if (!hash)
        return Exception { OperationError };

    ECGroup group(key);
    if (!group)
        return Exception { OperationError };

    MbedTLS::MPI r, s;
    if (mbedtls_mpi_read_binary(r.get(), signature.data(), size) || mbedtls_mpi_read_binary(s.get(), signature.data() + size, size))
        return Exception { OperationError };

    auto& keyPair = MbedTLS::ecKeyPair(key.platformKey());
    return !mbedtls_ecdsa_verify(group.get(), hash->data(), hash->size(), &MbedTLS::ecPublicPoint(keyPair), r.get(), s.get());
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO) && USE(MBEDTLS)
