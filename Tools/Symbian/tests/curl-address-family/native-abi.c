/* Copyright (c) 2026 The symbian-webkit authors. SPDX-License-Identifier: BSD-2-Clause */
/* Compile using curl's actual Symbian configuration and SDK headers. */
#include "curl_setup.h"
#include <netinet/in.h>
#include <sys/socket.h>

_Static_assert(sizeof(CURL_SA_FAMILY_T) == sizeof(((struct sockaddr*)0)->sa_family),
    "curl's family cast must match the native socket field width");
_Static_assert(sizeof(CURL_SA_FAMILY_T) == sizeof(((struct sockaddr_in*)0)->sin_family),
    "curl's family cast must match the native IPv4 field width");
_Static_assert((CURL_SA_FAMILY_T)AF_INET == AF_INET,
    "curl must preserve the complete Symbian IPv4 address family");
_Static_assert((CURL_SA_FAMILY_T)AF_INET6 == AF_INET6,
    "the cast must also preserve IPv6 even though this build disables IPv6");
