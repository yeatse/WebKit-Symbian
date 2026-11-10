/* Copyright (c) 2026 The symbian-webkit authors. SPDX-License-Identifier: BSD-2-Clause */
#include "symbian-abi.h"
#include "curl_addrinfo.h"

int Curl_inet_pton(int family, const char* text, void* bytes)
{
    return family == AF_INET ? inet_pton(host_ipv4_family, text, bytes) : -1;
}

int main(void)
{
    const char* addresses[] = { "127.0.0.1", "192.0.2.123", "255.255.255.255" };
    const int ports[] = { 8791, 443, 65535 };
    unsigned failed = 0;
    for (unsigned i = 0; i < sizeof(addresses) / sizeof(addresses[0]); ++i) {
        struct in_addr binary;
        assert(Curl_inet_pton(AF_INET, addresses[i], &binary) == 1);
        struct Curl_addrinfo* result = Curl_ip2addr(AF_INET, &binary, addresses[i], ports[i]);
        assert(result);
        const struct sockaddr_in* address = (const void*)result->ai_addr;
        bool valid = result->ai_family == AF_INET && address->sin_family == AF_INET
            && ntohs(address->sin_port) == ports[i]
            && !memcmp(&address->sin_addr, &binary, sizeof(binary))
            && !strcmp(result->ai_canonname, addresses[i]);
        printf("%s %s:%d ai_family=0x%x socket_family=0x%x\n",
            valid ? "PASS" : "FAIL", addresses[i], ports[i], result->ai_family, address->sin_family);
        failed += !valid;
        Curl_freeaddrinfo(result);
    }
    return failed ? 1 : 0;
}
