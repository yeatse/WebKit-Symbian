/* Copyright (c) 2026 The symbian-webkit authors. SPDX-License-Identifier: BSD-2-Clause */
#pragma once

/* A host-only data-layout shim. The address-building algorithms come unchanged
 * from the supplied curl source. native-abi.c separately checks the real SDK. */
#include <assert.h>
#include <arpa/inet.h>
#include <curl/curl.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { host_ipv4_family = AF_INET };
#undef AF_INET
#define AF_INET 0x0800
#define sa_family_t uint8_t
#define sockaddr symbian_sockaddr
#define sockaddr_in symbian_sockaddr_in

struct symbian_sockaddr {
    unsigned short sa_family;
    unsigned short sa_port;
    char sa_data[24];
    unsigned char sa_len;
};

struct symbian_sockaddr_in {
    unsigned short sin_family;
    unsigned short sin_port;
    struct in_addr sin_addr;
    char sin_zero[20];
};

#define HEADER_CURL_SETUP_H
#define HEADER_CURL_WARNLESS_H
#define HEADER_CURL_PRINTF_H
#define HEADER_CURL_MEMORY_H
#define HEADER_CURL_MEMDEBUG_H
#define HEADER_CURL_INET_PTON_H
#define DEBUGASSERT(value) assert(value)
#ifndef CURL_SA_FAMILY_T
#define CURL_SA_FAMILY_T sa_family_t
#endif
int Curl_inet_pton(int, const char*, void*);
