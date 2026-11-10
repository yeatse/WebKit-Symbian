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
#include <wtf/TZoneMallocInlines.h>

#if OS(SYMBIAN)
#include "CurlContext.h"
#endif

#if USE(CURL) && USE(MBEDTLS)
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CurlSSLVerifier);

CurlSSLVerifier::CurlSSLVerifier(void* sslCtx)
{
    mbedtls_ssl_conf_verify(static_cast<mbedtls_ssl_config*>(sslCtx), verifyCallback, this);
}

int CurlSSLVerifier::verifyCallback(void* data, mbedtls_x509_crt* certificate, int depth, uint32_t* flags)
{
    auto& verifier = *static_cast<CurlSSLVerifier*>(data);
    if (depth >= 0) {
        if (verifier.m_certificateChain.size() <= static_cast<size_t>(depth))
            verifier.m_certificateChain.grow(depth + 1);
        verifier.m_certificateChain[depth] = CertificateInfo::makeCertificate({ certificate->raw.p, certificate->raw.len });
    }
    verifier.m_verificationErrors |= *flags;
    // Preserve mbedTLS's verification flags, including the hostname check.
    return 0;
}

std::unique_ptr<CertificateInfo> CurlSSLVerifier::createCertificateInfo(std::optional<long>&& result)
{
    if (!result || m_certificateChain.isEmpty())
        return nullptr;
    return makeUnique<CertificateInfo>(m_verificationErrors | *result, WTF::move(m_certificateChain));
}

} // namespace WebCore
#endif
