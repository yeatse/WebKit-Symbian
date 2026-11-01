#!/usr/bin/env python3
"""Apply checked, idempotent Belle network changes to curl 7.88.1."""

from pathlib import Path
import sys


def replace_once(path, before, after):
    text = path.read_text()
    if after in text:
        return
    if text.count(before) != 1:
        raise SystemExit(f"Unexpected curl source: {path}: {before[:70]!r}")
    path.write_text(text.replace(before, after, 1))


def patch(source):
    version = source / "include/curl/curlver.h"
    if '#define LIBCURL_VERSION "7.88.1"' not in version.read_text():
        raise SystemExit("This patch is checked against curl 7.88.1 only")

    # Open C's netinet/in.h needs u_short and related typedefs from sys/types.h.
    # Test the real declarations with their prerequisites, rather than forcing
    # HAVE_SOCKADDR_IN6_* results and hiding an ABI mismatch.
    for member, variable in [
        ("sin6_addr", "HAVE_SOCKADDR_IN6_SIN6_ADDR"),
        ("sin6_scope_id", "HAVE_SOCKADDR_IN6_SIN6_SCOPE_ID"),
    ]:
        before = (f'  check_struct_has_member("struct sockaddr_in6" {member} '
                  f'"netinet/in.h"\n                          {variable})')
        replace_once(source / "CMakeLists.txt", before,
                     before.replace('"netinet/in.h"',
                                    '"sys/types.h;sys/socket.h;netinet/in.h"'))

    resolver = source / "lib/curl_addrinfo.c"
    before = "  error = getaddrinfo(nodename, servname, hints, &aihead);"
    replace_once(resolver, before, r'''#if defined(__SYMBIAN32__) && defined(ENABLE_IPV6)
  /* Resolve each family explicitly for Open C's family-specific sockaddr
   * layout, inside curl's existing resolver worker. Keep both families
   * for Happy Eyeballs; an unavailable IPv6 resolver must not discard IPv4.
   */
  if(!hints || hints->ai_family == AF_UNSPEC) {
    struct addrinfo family_hints;
    struct Curl_addrinfo *v6 = NULL, *v4 = NULL, *tail;
    int error6, error4;
    memset(&family_hints, 0, sizeof(family_hints));
    if(hints)
      family_hints = *hints;
    family_hints.ai_family = AF_INET6;
    error6 = Curl_getaddrinfo_ex(nodename, servname, &family_hints, &v6);
    if(error6 == EAI_MEMORY)
      return error6;
    family_hints.ai_family = AF_INET;
    error4 = Curl_getaddrinfo_ex(nodename, servname, &family_hints, &v4);
    if(error4 == EAI_MEMORY) {
      Curl_freeaddrinfo(v6);
      return error4;
    }
    if(v6) {
      for(tail = v6; tail->ai_next; tail = tail->ai_next)
        ;
      tail->ai_next = v4;
      *result = v6;
      return 0;
    }
    if(v4) {
      *result = v4;
      return 0;
    }
    return error6 == EAI_AGAIN ? error6 : error4;
  }
#endif

  error = getaddrinfo(nodename, servname, hints, &aihead);''')

    h2 = source / "lib/http2.c"
    replace_once(h2,
                 "#define HTTP2_HUGE_WINDOW_SIZE (32 * 1024 * 1024) /* 32 MB */",
                 '''#ifdef __SYMBIAN32__
#include "curl-h2-budget.h"
/* Bound outstanding response data rather than advertising 32 MiB per stream. */
#define HTTP2_HUGE_WINDOW_SIZE (256 * 1024)
#else
#define HTTP2_HUGE_WINDOW_SIZE (32 * 1024 * 1024) /* 32 MB */
#endif''')
    replace_once(h2, "  nghttp2_session *h2;", '''  nghttp2_session *h2;
#ifdef __SYMBIAN32__
  struct Curl_symbian_h2_budget h2budget;
#endif''')
    replace_once(h2,
                 "  rc = nghttp2_session_client_new2(&ctx->h2, cbs, cf, o);",
                 '''#ifdef __SYMBIAN32__
  {
    nghttp2_mem mem = { &ctx->h2budget, symbian_h2_malloc, symbian_h2_free,
                       symbian_h2_calloc, symbian_h2_realloc };
    nghttp2_option_set_max_reserved_remote_streams(o, 0);
    nghttp2_option_set_max_send_header_block_length(o, 64 * 1024);
    nghttp2_option_set_max_deflate_dynamic_table_size(o, 4096);
    nghttp2_option_set_max_outbound_ack(o, 16);
    nghttp2_option_set_max_settings(o, 16);
    nghttp2_option_set_max_continuations(o, 4);
    rc = nghttp2_session_client_new3(&ctx->h2, cbs, cf, o, &mem);
  }
#else
  rc = nghttp2_session_client_new2(&ctx->h2, cbs, cf, o);
#endif''')
    replace_once(source / "lib/dynbuf.h",
                 "#define DYN_H2_HEADERS      (128*1024)\n"
                 "#define DYN_H2_TRAILERS     (128*1024)",
                 '''#ifdef __SYMBIAN32__
#define DYN_H2_HEADERS      (64*1024)
#define DYN_H2_TRAILERS     (16*1024)
#else
#define DYN_H2_HEADERS      (128*1024)
#define DYN_H2_TRAILERS     (128*1024)
#endif''')


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(f"Usage: {sys.argv[0]} CURL_SOURCE")
    patch(Path(sys.argv[1]))
