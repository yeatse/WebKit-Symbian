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
#include "MbedTLSHelper.h"

#if USE(CURL) && USE(MBEDTLS)
#include <mbedtls/oid.h>
#include <mbedtls/x509_crt.h>
#include <time.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace MbedTLS {

static String toString(const mbedtls_x509_buf& buffer)
{
    return String::fromUTF8(buffer.p, buffer.len);
}

static Seconds toSeconds(const mbedtls_x509_time& time)
{
    struct tm components { };
    components.tm_year = time.year - 1900;
    components.tm_mon = time.mon - 1;
    components.tm_mday = time.day;
    components.tm_hour = time.hour;
    components.tm_min = time.min;
    components.tm_sec = time.sec;
    return Seconds(static_cast<double>(timegm(&components)));
}

static String commonName(const mbedtls_x509_name* name)
{
    for (; name; name = name->next) {
        if (!MBEDTLS_OID_CMP(MBEDTLS_OID_AT_CN, &name->oid))
            return toString(name->val);
    }
    return String();
}

static String ipAddress(const mbedtls_x509_buf& address)
{
    const uint8_t* data = address.p;
    if (address.len == 4)
        return makeString(data[0], '.', data[1], '.', data[2], '.', data[3]);
    if (address.len == 16) {
        StringBuilder builder;
        for (int i = 0; i < 8; ++i, data += 2) {
            builder.append(hex(data[0] << 8 | data[1], 4));
            if (i != 7)
                builder.append(':');
        }
        return builder.toString();
    }
    return String();
}

Optional<WebCore::CertificateSummary> createSummaryInfo(const Vector<uint8_t>& der)
{
    mbedtls_x509_crt certificate;
    mbedtls_x509_crt_init(&certificate);
    if (mbedtls_x509_crt_parse_der(&certificate, der.data(), der.size())) {
        mbedtls_x509_crt_free(&certificate);
        return WTF::nullopt;
    }

    WebCore::CertificateSummary summary;
    summary.subject = commonName(&certificate.subject);
    if (summary.subject.isNull()) {
        char name[256];
        if (mbedtls_x509_dn_gets(name, sizeof(name), &certificate.subject) > 0)
            summary.subject = String::fromUTF8(name);
    }
    summary.validFrom = toSeconds(certificate.valid_from);
    summary.validUntil = toSeconds(certificate.valid_to);

    // mbedTLS 3.4 parses DNS names (and allocates nothing for them); other
    // types, IP addresses included, fail to parse and are skipped.
    for (auto* entry = &certificate.subject_alt_names; entry && entry->buf.p; entry = entry->next) {
        mbedtls_x509_subject_alternative_name name;
        if (mbedtls_x509_parse_subject_alt_name(&entry->buf, &name))
            continue;
        if (name.type == MBEDTLS_X509_SAN_DNS_NAME)
            summary.dnsNames.append(toString(name.san.unstructured_name));
        else if (name.type == MBEDTLS_X509_SAN_IP_ADDRESS) {
            auto address = ipAddress(name.san.unstructured_name);
            if (!address.isNull())
                summary.ipAddresses.append(WTFMove(address));
        }
    }

    mbedtls_x509_crt_free(&certificate);
    return summary;
}

String verificationErrorDescription(int flags)
{
    char description[512];
    if (mbedtls_x509_crt_verify_info(description, sizeof(description), "", static_cast<uint32_t>(flags)) < 0)
        return String();
    return String::fromUTF8(description).stripWhiteSpace();
}

} // namespace MbedTLS

#endif
