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
#include "CurlSSLVerifier.h"

#if OS(SYMBIAN)
#include "CurlContext.h"
#endif

#if USE(CURL) && USE(MBEDTLS)
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

namespace WebCore {

static int convertToSSLCertificateFlags(uint32_t flags)
{
    unsigned result = 0;
    if (flags & MBEDTLS_X509_BADCERT_NOT_TRUSTED)
        result |= static_cast<unsigned>(CurlSSLVerifier::SSLCertificateFlags::SSL_CERTIFICATE_UNKNOWN_CA);
    if (flags & MBEDTLS_X509_BADCERT_CN_MISMATCH)
        result |= static_cast<unsigned>(CurlSSLVerifier::SSLCertificateFlags::SSL_CERTIFICATE_BAD_IDENTITY);
    if (flags & MBEDTLS_X509_BADCERT_FUTURE)
        result |= static_cast<unsigned>(CurlSSLVerifier::SSLCertificateFlags::SSL_CERTIFICATE_NOT_ACTIVATED);
    if (flags & MBEDTLS_X509_BADCERT_EXPIRED)
        result |= static_cast<unsigned>(CurlSSLVerifier::SSLCertificateFlags::SSL_CERTIFICATE_EXPIRED);
    if (flags & MBEDTLS_X509_BADCERT_REVOKED)
        result |= static_cast<unsigned>(CurlSSLVerifier::SSLCertificateFlags::SSL_CERTIFICATE_REVOKED);
    if (flags & (MBEDTLS_X509_BADCERT_BAD_MD | MBEDTLS_X509_BADCERT_BAD_PK | MBEDTLS_X509_BADCERT_BAD_KEY))
        result |= static_cast<unsigned>(CurlSSLVerifier::SSLCertificateFlags::SSL_CERTIFICATE_INSECURE);
    if (flags && !result)
        result |= static_cast<unsigned>(CurlSSLVerifier::SSLCertificateFlags::SSL_CERTIFICATE_GENERIC_ERROR);
    return static_cast<int>(result);
}

// curl's mbedTLS backend hands its mbedtls_ssl_config to the SSL_CTX callback
// and leaves the verification callback to us. mbedTLS calls it for each
// certificate of the chain, from the top down to the server's (depth 0).
CurlSSLVerifier::CurlSSLVerifier(void* sslCtx)
{
    auto* config = static_cast<mbedtls_ssl_config*>(sslCtx);
    mbedtls_ssl_conf_verify(config, verifyCallback, this);
}

int CurlSSLVerifier::verifyCallback(void* data, mbedtls_x509_crt* certificate, int depth, uint32_t* flags)
{
    auto* verifier = static_cast<CurlSSLVerifier*>(data);
#if OS(SYMBIAN)
    CurlContext::singleton().trace("TLS certificate at depth %d, flags %x", depth, static_cast<unsigned>(*flags));
#endif
    if (depth < 0)
        return 0;
    if (verifier->m_chain.size() <= static_cast<size_t>(depth))
        verifier->m_chain.grow(depth + 1);
    verifier->m_chain[depth] = CertificateInfo::makeCertificate(certificate->raw.p, certificate->raw.len);
    verifier->m_chainErrors |= *flags;

    if (!depth)
        verifier->finishChain();
    // Leave *flags as mbedTLS computed them, so curl still fails the handshake
    // unless verification is disabled.
    return 0;
}

void CurlSSLVerifier::finishChain()
{
    m_certificateInfo = CertificateInfo(static_cast<int>(m_chainErrors), WTFMove(m_chain));
    m_sslErrors = convertToSSLCertificateFlags(m_chainErrors);
    m_chain = { };
    m_chainErrors = 0;
}

} // namespace WebCore

#endif
