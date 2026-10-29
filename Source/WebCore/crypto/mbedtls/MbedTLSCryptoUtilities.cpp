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
#include "MbedTLSCryptoUtilities.h"

#if ENABLE(WEB_CRYPTO) && USE(MBEDTLS)

#include <mbedtls/asn1write.h>
#include <mbedtls/oid.h>

namespace WebCore {

void MbedTLSPKContextDeleter::operator()(mbedtls_pk_context* context) const
{
    mbedtls_pk_free(context);
    delete context;
}

namespace MbedTLS {

Optional<mbedtls_md_type_t> hashType(CryptoAlgorithmIdentifier identifier)
{
    switch (identifier) {
    case CryptoAlgorithmIdentifier::SHA_1:
        return MBEDTLS_MD_SHA1;
    case CryptoAlgorithmIdentifier::SHA_224:
        return MBEDTLS_MD_SHA224;
    case CryptoAlgorithmIdentifier::SHA_256:
        return MBEDTLS_MD_SHA256;
    case CryptoAlgorithmIdentifier::SHA_384:
        return MBEDTLS_MD_SHA384;
    case CryptoAlgorithmIdentifier::SHA_512:
        return MBEDTLS_MD_SHA512;
    default:
        return WTF::nullopt;
    }
}

Optional<Vector<uint8_t>> digest(CryptoAlgorithmIdentifier identifier, const Vector<uint8_t>& data)
{
    auto type = hashType(identifier);
    if (!type)
        return WTF::nullopt;
    auto* info = mbedtls_md_info_from_type(*type);
    if (!info)
        return WTF::nullopt;

    Vector<uint8_t> result(mbedtls_md_get_size(info));
    if (mbedtls_md(info, data.data(), data.size(), result.data()))
        return WTF::nullopt;
    return result;
}

Vector<uint8_t> mpiData(const mbedtls_mpi& mpi, size_t size)
{
    size_t length = mbedtls_mpi_size(&mpi);
    if (!size)
        size = length;
    if (length > size)
        return { };

    Vector<uint8_t> result(size);
    if (mbedtls_mpi_write_binary(&mpi, result.data(), result.size()))
        return { };
    return result;
}

Random::Random()
{
    mbedtls_entropy_init(&m_entropy);
    mbedtls_ctr_drbg_init(&m_drbg);
    m_seeded = !mbedtls_ctr_drbg_seed(&m_drbg, mbedtls_entropy_func, &m_entropy, nullptr, 0);
}

Random::~Random()
{
    mbedtls_ctr_drbg_free(&m_drbg);
    mbedtls_entropy_free(&m_entropy);
}

int Random::generate(void* context, unsigned char* output, size_t length)
{
    return mbedtls_ctr_drbg_random(context, output, length);
}

MbedTLSPKContext createPKContext(mbedtls_pk_type_t type)
{
    MbedTLSPKContext context(new mbedtls_pk_context);
    mbedtls_pk_init(context.get());
    if (mbedtls_pk_setup(context.get(), mbedtls_pk_info_from_type(type)))
        return nullptr;
    return context;
}

MbedTLSPKContext parseSubjectPublicKeyInfo(const Vector<uint8_t>& data)
{
    MbedTLSPKContext context(new mbedtls_pk_context);
    mbedtls_pk_init(context.get());
    if (data.isEmpty() || mbedtls_pk_parse_public_key(context.get(), data.data(), data.size()))
        return nullptr;
    return context;
}

// mbedtls_pk_parse_key() also takes PKCS #1 and SEC 1 keys, which Web Crypto's
// "pkcs8" format doesn't. A PrivateKeyInfo is a SEQUENCE of a version (0, or 1
// for RFC 5958) and then the AlgorithmIdentifier SEQUENCE; the other two have
// an INTEGER or an OCTET STRING there.
static bool isPrivateKeyInfo(const Vector<uint8_t>& data)
{
    auto* p = const_cast<unsigned char*>(data.data());
    const auto* end = p + data.size();
    size_t length;
    if (mbedtls_asn1_get_tag(&p, end, &length, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE) || p + length != end)
        return false;
    int version;
    if (mbedtls_asn1_get_int(&p, end, &version) || (version != 0 && version != 1))
        return false;
    return p < end && *p == (MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
}

MbedTLSPKContext parsePrivateKeyInfo(const Vector<uint8_t>& data)
{
    if (data.isEmpty() || !isPrivateKeyInfo(data))
        return nullptr;

    // An EC key without its public point needs the RNG to compute it.
    Random random;
    if (!random)
        return nullptr;

    MbedTLSPKContext context(new mbedtls_pk_context);
    mbedtls_pk_init(context.get());
    if (mbedtls_pk_parse_key(context.get(), data.data(), data.size(), nullptr, 0, Random::generate, random.context()))
        return nullptr;
    return context;
}

// Large enough for an 8192-bit RSA private key, the most MBEDTLS_MPI_MAX_SIZE allows.
static constexpr size_t maximumKeyDERSize = 8192;

Vector<uint8_t> subjectPublicKeyInfo(mbedtls_pk_context* context)
{
    // The mbedtls_pk_write_*() functions write backwards from the end of the buffer.
    Vector<uint8_t> buffer(maximumKeyDERSize);
    int length = mbedtls_pk_write_pubkey_der(context, buffer.data(), buffer.size());
    if (length <= 0)
        return { };
    return Vector<uint8_t>(buffer.data() + buffer.size() - length, length);
}

Vector<uint8_t> privateKeyInfo(mbedtls_pk_context* context)
{
    // PrivateKeyInfo ::= SEQUENCE {
    //     version INTEGER (0),
    //     privateKeyAlgorithm AlgorithmIdentifier,
    //     privateKey OCTET STRING -- RSAPrivateKey (PKCS #1) or ECPrivateKey (SEC 1)
    // }
    // written backwards, as mbedTLS's ASN.1 writer does.
    Vector<uint8_t> buffer(maximumKeyDERSize);
    unsigned char* start = buffer.data();
    int written = mbedtls_pk_write_key_der(context, start, buffer.size());
    if (written <= 0)
        return { };
    unsigned char* p = start + buffer.size() - written;
    size_t length = written;

    auto add = [&](int result) {
        if (result < 0)
            return false;
        length += result;
        return true;
    };

    if (!add(mbedtls_asn1_write_len(&p, start, length)) || !add(mbedtls_asn1_write_tag(&p, start, MBEDTLS_ASN1_OCTET_STRING)))
        return { };

    auto type = mbedtls_pk_get_type(context);
    size_t parametersLength = 0;
    if (type == MBEDTLS_PK_ECKEY) {
        // The namedCurve parameters.
        const char* curveOID;
        size_t curveOIDLength;
        if (mbedtls_oid_get_oid_by_ec_grp(ecGroup(ecKeyPair(context)).id, &curveOID, &curveOIDLength))
            return { };
        int result = mbedtls_asn1_write_oid(&p, start, curveOID, curveOIDLength);
        if (result < 0)
            return { };
        parametersLength = result;
    }
    const char* oid;
    size_t oidLength;
    if (mbedtls_oid_get_oid_by_pk_alg(type, &oid, &oidLength))
        return { };
    // This counts the parameters already written; with none, it writes NULL.
    if (!add(mbedtls_asn1_write_algorithm_identifier(&p, start, oid, oidLength, parametersLength)))
        return { };

    if (!add(mbedtls_asn1_write_int(&p, start, 0))
        || !add(mbedtls_asn1_write_len(&p, start, length))
        || !add(mbedtls_asn1_write_tag(&p, start, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)))
        return { };

    return Vector<uint8_t>(p, length);
}

} // namespace MbedTLS
} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO) && USE(MBEDTLS)
