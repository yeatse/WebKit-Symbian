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

#include "CryptoAlgorithmRSAES_PKCS1_v1_5.h"
#include "CryptoAlgorithmRSASSA_PKCS1_v1_5.h"
#include "CryptoAlgorithmRSA_OAEP.h"
#include "CryptoAlgorithmRSA_PSS.h"
#include "CryptoAlgorithmRsaOaepParams.h"
#include "CryptoAlgorithmRsaPssParams.h"
#include "CryptoKeyRSA.h"
#include "MbedTLSCryptoUtilities.h"

namespace WebCore {

namespace {

// A copy of the key's context for one operation: setting the padding changes
// the context, and so do the blinding values of private-key operations.
class RSAOperation {
    WTF_MAKE_NONCOPYABLE(RSAOperation);
public:
    RSAOperation(const CryptoKeyRSA& key, int padding, mbedtls_md_type_t hash = MBEDTLS_MD_NONE)
    {
        mbedtls_rsa_init(&m_context);
        m_valid = !mbedtls_rsa_copy(&m_context, &MbedTLS::rsaContext(key.platformKey()))
            && !mbedtls_rsa_set_padding(&m_context, padding, hash)
            && m_random;
    }
    ~RSAOperation() { mbedtls_rsa_free(&m_context); }

    explicit operator bool() const { return m_valid; }
    mbedtls_rsa_context* get() { return &m_context; }
    size_t length() const { return mbedtls_rsa_get_len(&m_context); }
    void* random() { return m_random.context(); }

private:
    mbedtls_rsa_context m_context;
    MbedTLS::Random m_random;
    bool m_valid { false };
};

} // namespace

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSAES_PKCS1_v1_5::platformEncrypt(const CryptoKeyRSA& key, const Vector<uint8_t>& plainText)
{
    RSAOperation operation(key, MBEDTLS_RSA_PKCS_V15);
    if (!operation)
        return Exception { OperationError };

    Vector<uint8_t> output(operation.length());
    if (mbedtls_rsa_rsaes_pkcs1_v15_encrypt(operation.get(), MbedTLS::Random::generate, operation.random(), plainText.size(), plainText.data(), output.data()))
        return Exception { OperationError };
    return output;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSAES_PKCS1_v1_5::platformDecrypt(const CryptoKeyRSA& key, const Vector<uint8_t>& cipherText)
{
    RSAOperation operation(key, MBEDTLS_RSA_PKCS_V15);
    if (!operation || cipherText.size() != operation.length())
        return Exception { OperationError };

    Vector<uint8_t> output(operation.length());
    size_t outputLength = 0;
    if (mbedtls_rsa_rsaes_pkcs1_v15_decrypt(operation.get(), MbedTLS::Random::generate, operation.random(), &outputLength, cipherText.data(), output.data(), output.size()))
        return Exception { OperationError };
    output.shrink(outputLength);
    return output;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSA_OAEP::platformEncrypt(const CryptoAlgorithmRsaOaepParams& parameters, const CryptoKeyRSA& key, const Vector<uint8_t>& plainText)
{
    auto hash = MbedTLS::hashType(key.hashAlgorithmIdentifier());
    if (!hash)
        return Exception { NotSupportedError };
    RSAOperation operation(key, MBEDTLS_RSA_PKCS_V21, *hash);
    if (!operation)
        return Exception { OperationError };

    const auto& label = parameters.labelVector();
    Vector<uint8_t> output(operation.length());
    if (mbedtls_rsa_rsaes_oaep_encrypt(operation.get(), MbedTLS::Random::generate, operation.random(), label.data(), label.size(), plainText.size(), plainText.data(), output.data()))
        return Exception { OperationError };
    return output;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSA_OAEP::platformDecrypt(const CryptoAlgorithmRsaOaepParams& parameters, const CryptoKeyRSA& key, const Vector<uint8_t>& cipherText)
{
    auto hash = MbedTLS::hashType(key.hashAlgorithmIdentifier());
    if (!hash)
        return Exception { NotSupportedError };
    RSAOperation operation(key, MBEDTLS_RSA_PKCS_V21, *hash);
    if (!operation || cipherText.size() != operation.length())
        return Exception { OperationError };

    const auto& label = parameters.labelVector();
    Vector<uint8_t> output(operation.length());
    size_t outputLength = 0;
    if (mbedtls_rsa_rsaes_oaep_decrypt(operation.get(), MbedTLS::Random::generate, operation.random(), label.data(), label.size(), &outputLength, cipherText.data(), output.data(), output.size()))
        return Exception { OperationError };
    output.shrink(outputLength);
    return output;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSASSA_PKCS1_v1_5::platformSign(const CryptoKeyRSA& key, const Vector<uint8_t>& data)
{
    auto hashType = MbedTLS::hashType(key.hashAlgorithmIdentifier());
    auto hash = MbedTLS::digest(key.hashAlgorithmIdentifier(), data);
    if (!hashType || !hash)
        return Exception { OperationError };
    RSAOperation operation(key, MBEDTLS_RSA_PKCS_V15);
    if (!operation)
        return Exception { OperationError };

    Vector<uint8_t> signature(operation.length());
    if (mbedtls_rsa_rsassa_pkcs1_v15_sign(operation.get(), MbedTLS::Random::generate, operation.random(), *hashType, hash->size(), hash->data(), signature.data()))
        return Exception { OperationError };
    return signature;
}

ExceptionOr<bool> CryptoAlgorithmRSASSA_PKCS1_v1_5::platformVerify(const CryptoKeyRSA& key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
    auto hashType = MbedTLS::hashType(key.hashAlgorithmIdentifier());
    auto hash = MbedTLS::digest(key.hashAlgorithmIdentifier(), data);
    if (!hashType || !hash)
        return Exception { OperationError };
    RSAOperation operation(key, MBEDTLS_RSA_PKCS_V15);
    if (!operation)
        return Exception { OperationError };
    if (signature.size() != operation.length())
        return false;

    return !mbedtls_rsa_rsassa_pkcs1_v15_verify(operation.get(), *hashType, hash->size(), hash->data(), signature.data());
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSA_PSS::platformSign(const CryptoAlgorithmRsaPssParams& parameters, const CryptoKeyRSA& key, const Vector<uint8_t>& data)
{
    auto hashType = MbedTLS::hashType(key.hashAlgorithmIdentifier());
    auto hash = MbedTLS::digest(key.hashAlgorithmIdentifier(), data);
    if (!hashType || !hash)
        return Exception { OperationError };
    RSAOperation operation(key, MBEDTLS_RSA_PKCS_V21, *hashType);
    if (!operation || parameters.saltLength > static_cast<size_t>(std::numeric_limits<int>::max()))
        return Exception { OperationError };

    Vector<uint8_t> signature(operation.length());
    if (mbedtls_rsa_rsassa_pss_sign_ext(operation.get(), MbedTLS::Random::generate, operation.random(), *hashType, hash->size(), hash->data(), parameters.saltLength, signature.data()))
        return Exception { OperationError };
    return signature;
}

ExceptionOr<bool> CryptoAlgorithmRSA_PSS::platformVerify(const CryptoAlgorithmRsaPssParams& parameters, const CryptoKeyRSA& key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
    auto hashType = MbedTLS::hashType(key.hashAlgorithmIdentifier());
    auto hash = MbedTLS::digest(key.hashAlgorithmIdentifier(), data);
    if (!hashType || !hash)
        return Exception { OperationError };
    RSAOperation operation(key, MBEDTLS_RSA_PKCS_V21, *hashType);
    if (!operation || parameters.saltLength > static_cast<size_t>(std::numeric_limits<int>::max()))
        return Exception { OperationError };
    if (signature.size() != operation.length())
        return false;

    return !mbedtls_rsa_rsassa_pss_verify_ext(operation.get(), *hashType, hash->size(), hash->data(), *hashType, parameters.saltLength, signature.data());
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO) && USE(MBEDTLS)
