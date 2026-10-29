/* Copyright (c) 2026 The symbian-webkit authors. SPDX-License-Identifier: BSD-2-Clause */
/* Include the actual SDK helper; do not duplicate its parsing algorithm. */
#include "symbian_x509_ip.h"
#include <arpa/inet.h>
#include <stdio.h>

int main(void)
{
    char input[256];
    unsigned cases = 0, failures = 0;
    while (fgets(input, sizeof(input), stdin)) {
        input[strcspn(input, "\r\n")] = 0;
        unsigned char actual[18] = { 0 }, reference[16] = { 0 };
        actual[0] = actual[17] = 0xa5;
        size_t size = symbian_x509_parse_ip(input, actual + 1);
        int family = strchr(input, ':') ? AF_INET6 : AF_INET;
        int valid = inet_pton(family, input, reference) == 1;
        size_t expected = valid ? (family == AF_INET6 ? 16 : 4) : 0;
        ++cases;
        if (size != expected || (size && memcmp(actual + 1, reference, size))
            || actual[0] != 0xa5 || actual[17] != 0xa5) {
            ++failures;
            if (failures < 20)
                printf("FAIL %s actual=%zu expected=%zu\n", input, size, expected);
        }
    }
    printf("PARSER_RESULT %u cases %u mismatches\n", cases, failures);
    return failures ? 1 : 0;
}
