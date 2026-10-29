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

#include "CryptoAlgorithmAESCBC.h"
#include "CryptoAlgorithmAESCFB.h"
#include "CryptoAlgorithmAESGCM.h"
#include "CryptoAlgorithmAESKW.h"
#include "CryptoAlgorithmAesCbcCfbParams.h"
#include "CryptoAlgorithmAesGcmParams.h"
#include "CryptoKeyAES.h"
#include <mbedtls/aes.h>
#include <mbedtls/gcm.h>
#include <mbedtls/nist_kw.h>
#include <limits>

namespace WebCore {

namespace {

constexpr size_t aesBlockSize = 16;

class AESContext {
    WTF_MAKE_NONCOPYABLE(AESContext);
public:
    AESContext() { mbedtls_aes_init(&m_context); }
    ~AESContext() { mbedtls_aes_free(&m_context); }

    bool setKey(const Vector<uint8_t>& key, bool encrypt)
    {
        if (encrypt)
            return !mbedtls_aes_setkey_enc(&m_context, key.data(), key.size() * 8);
        return !mbedtls_aes_setkey_dec(&m_context, key.data(), key.size() * 8);
    }
    mbedtls_aes_context* get() { return &m_context; }

private:
    mbedtls_aes_context m_context;
};

} // namespace

static ExceptionOr<Vector<uint8_t>> aesCBC(int mode, const Vector<uint8_t>& key, const Vector<uint8_t>& iv, const Vector<uint8_t>& input, CryptoAlgorithmAESCBC::Padding padding)
{
    if (iv.size() != aesBlockSize)
        return Exception { ExceptionCode::OperationError };

    AESContext context;
    if (!context.setKey(key, mode == MBEDTLS_AES_ENCRYPT))
        return Exception { ExceptionCode::OperationError };

    Vector<uint8_t> data = input;
    if (mode == MBEDTLS_AES_ENCRYPT && padding == CryptoAlgorithmAESCBC::Padding::Yes) {
        // PKCS #7
        size_t padLength = aesBlockSize - data.size() % aesBlockSize;
        if (data.size() > std::numeric_limits<size_t>::max() - padLength)
            return Exception { ExceptionCode::OperationError };
        data.grow(data.size() + padLength);
        memset(data.data() + data.size() - padLength, padLength, padLength);
    }
    if (data.size() % aesBlockSize)
        return Exception { ExceptionCode::OperationError };

    uint8_t ivBuffer[aesBlockSize];
    memcpy(ivBuffer, iv.data(), aesBlockSize);
    Vector<uint8_t> output(data.size());
    if (mbedtls_aes_crypt_cbc(context.get(), mode, data.size(), ivBuffer, data.data(), output.data()))
        return Exception { ExceptionCode::OperationError };

    if (mode == MBEDTLS_AES_DECRYPT && padding == CryptoAlgorithmAESCBC::Padding::Yes) {
        if (output.isEmpty())
            return Exception { ExceptionCode::OperationError };
        size_t padLength = output.last();
        if (!padLength || padLength > aesBlockSize || padLength > output.size())
            return Exception { ExceptionCode::OperationError };
        for (size_t i = output.size() - padLength; i < output.size(); ++i) {
            if (output[i] != padLength)
                return Exception { ExceptionCode::OperationError };
        }
        output.shrink(output.size() - padLength);
    }
    return output;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESCBC::platformEncrypt(const CryptoAlgorithmAesCbcCfbParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& plainText, Padding padding)
{
    return aesCBC(MBEDTLS_AES_ENCRYPT, key.key(), parameters.ivVector(), plainText, padding);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESCBC::platformDecrypt(const CryptoAlgorithmAesCbcCfbParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& cipherText, Padding padding)
{
    return aesCBC(MBEDTLS_AES_DECRYPT, key.key(), parameters.ivVector(), cipherText, padding);
}

// Web Crypto's AES-CFB is CFB-8.
static ExceptionOr<Vector<uint8_t>> aesCFB8(int mode, const Vector<uint8_t>& key, const Vector<uint8_t>& iv, const Vector<uint8_t>& input)
{
    if (iv.size() != aesBlockSize)
        return Exception { ExceptionCode::OperationError };

    // CFB uses the forward cipher in both directions.
    AESContext context;
    if (!context.setKey(key, true))
        return Exception { ExceptionCode::OperationError };

    uint8_t ivBuffer[aesBlockSize];
    memcpy(ivBuffer, iv.data(), aesBlockSize);
    Vector<uint8_t> output(input.size());
    if (mbedtls_aes_crypt_cfb8(context.get(), mode, input.size(), ivBuffer, input.data(), output.data()))
        return Exception { ExceptionCode::OperationError };
    return output;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESCFB::platformEncrypt(const CryptoAlgorithmAesCbcCfbParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& plainText)
{
    return aesCFB8(MBEDTLS_AES_ENCRYPT, key.key(), parameters.ivVector(), plainText);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESCFB::platformDecrypt(const CryptoAlgorithmAesCbcCfbParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& cipherText)
{
    return aesCFB8(MBEDTLS_AES_DECRYPT, key.key(), parameters.ivVector(), cipherText);
}

namespace {

class GCMContext {
    WTF_MAKE_NONCOPYABLE(GCMContext);
public:
    GCMContext() { mbedtls_gcm_init(&m_context); }
    ~GCMContext() { mbedtls_gcm_free(&m_context); }
    mbedtls_gcm_context* get() { return &m_context; }

private:
    mbedtls_gcm_context m_context;
};

} // namespace

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESGCM::platformEncrypt(const CryptoAlgorithmAesGcmParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& plainText)
{
    const auto& iv = parameters.ivVector();
    const auto& additionalData = parameters.additionalDataVector();
    size_t tagLength = parameters.tagLength.value_or(128) / 8;
    if (iv.isEmpty())
        return Exception { ExceptionCode::OperationError };

    GCMContext context;
    if (mbedtls_gcm_setkey(context.get(), MBEDTLS_CIPHER_ID_AES, key.key().data(), key.key().size() * 8))
        return Exception { ExceptionCode::OperationError };

    // The tag follows the ciphertext.
    if (plainText.size() > std::numeric_limits<size_t>::max() - tagLength)
        return Exception { ExceptionCode::OperationError };
    Vector<uint8_t> output(plainText.size() + tagLength);
    if (mbedtls_gcm_crypt_and_tag(context.get(), MBEDTLS_GCM_ENCRYPT, plainText.size(), iv.data(), iv.size(),
        additionalData.data(), additionalData.size(), plainText.data(), output.data(), tagLength, output.data() + plainText.size()))
        return Exception { ExceptionCode::OperationError };
    return output;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESGCM::platformDecrypt(const CryptoAlgorithmAesGcmParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& cipherText)
{
    const auto& iv = parameters.ivVector();
    const auto& additionalData = parameters.additionalDataVector();
    size_t tagLength = parameters.tagLength.value_or(128) / 8;
    if (iv.isEmpty() || cipherText.size() < tagLength)
        return Exception { ExceptionCode::OperationError };

    GCMContext context;
    if (mbedtls_gcm_setkey(context.get(), MBEDTLS_CIPHER_ID_AES, key.key().data(), key.key().size() * 8))
        return Exception { ExceptionCode::OperationError };

    size_t length = cipherText.size() - tagLength;
    Vector<uint8_t> output(length);
    if (mbedtls_gcm_auth_decrypt(context.get(), length, iv.data(), iv.size(), additionalData.data(), additionalData.size(),
        cipherText.data() + length, tagLength, cipherText.data(), output.data()))
        return Exception { ExceptionCode::OperationError };
    return output;
}

namespace {

class KWContext {
    WTF_MAKE_NONCOPYABLE(KWContext);
public:
    KWContext() { mbedtls_nist_kw_init(&m_context); }
    ~KWContext() { mbedtls_nist_kw_free(&m_context); }
    mbedtls_nist_kw_context* get() { return &m_context; }

private:
    mbedtls_nist_kw_context m_context;
};

} // namespace

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESKW::platformWrapKey(const CryptoKeyAES& key, const Vector<uint8_t>& data)
{
    if (data.size() < 16 || data.size() % 8 || data.size() > std::numeric_limits<size_t>::max() - 8)
        return Exception { ExceptionCode::OperationError };

    KWContext context;
    if (mbedtls_nist_kw_setkey(context.get(), MBEDTLS_CIPHER_ID_AES, key.key().data(), key.key().size() * 8, 1))
        return Exception { ExceptionCode::OperationError };

    Vector<uint8_t> output(data.size() + 8);
    size_t outputLength = 0;
    if (mbedtls_nist_kw_wrap(context.get(), MBEDTLS_KW_MODE_KW, data.data(), data.size(), output.data(), &outputLength, output.size()))
        return Exception { ExceptionCode::OperationError };
    output.shrink(outputLength);
    return output;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESKW::platformUnwrapKey(const CryptoKeyAES& key, const Vector<uint8_t>& data)
{
    if (data.size() < 24 || data.size() % 8)
        return Exception { ExceptionCode::OperationError };

    KWContext context;
    if (mbedtls_nist_kw_setkey(context.get(), MBEDTLS_CIPHER_ID_AES, key.key().data(), key.key().size() * 8, 0))
        return Exception { ExceptionCode::OperationError };

    Vector<uint8_t> output(data.size() - 8);
    size_t outputLength = 0;
    if (mbedtls_nist_kw_unwrap(context.get(), MBEDTLS_KW_MODE_KW, data.data(), data.size(), output.data(), &outputLength, output.size()))
        return Exception { ExceptionCode::OperationError };
    output.shrink(outputLength);
    return output;
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO) && USE(MBEDTLS)
