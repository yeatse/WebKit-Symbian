/* Assemble curl-owned addresses; no Open C hostname-address conversion. */
#include "curl_setup.h"
#include "curl_addrinfo.h"
#include "curl-symbian-resolver.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include "curl_memory.h"
#include "memdebug.h"
int Curl_symbian_getaddrinfo(const char *host, const char *service,
    const struct addrinfo *hints, struct Curl_addrinfo **result)
{
  struct SymbianCurlAddresses records;
  struct Curl_addrinfo *first = NULL, *tail = NULL;
  unsigned port = 0, i, pass;
  int family = hints ? hints->ai_family : AF_UNSPEC;
  int type = hints && hints->ai_socktype ? hints->ai_socktype : SOCK_STREAM;
  int protocol = type == SOCK_STREAM ? IPPROTO_TCP : IPPROTO_UDP;
  int flags = hints ? hints->ai_flags : 0;
  int status;
  *result = NULL;
  if(family != AF_UNSPEC && family != AF_INET && family != AF_INET6)
    return EAI_FAMILY;
  if(type != SOCK_STREAM && type != SOCK_DGRAM)
    return EAI_SOCKTYPE;
  if(hints && hints->ai_protocol && hints->ai_protocol != protocol)
    return EAI_SOCKTYPE;
  if(flags & ~(AI_CANONNAME | AI_NUMERICHOST | AI_NUMERICSERV))
    return EAI_BADFLAGS;
  if(service) {
    const unsigned char *p = (const unsigned char *)service;
    if(!*p) return EAI_SERVICE;
    for(; *p; ++p) {
      if(*p < '0' || *p > '9' || port > (65535U - (*p - '0')) / 10)
        return EAI_SERVICE;
      port = port * 10 + *p - '0';
    }
  }
  status = symbian_curl_resolve_records(host, family,
                                       !!(flags & AI_NUMERICHOST), &records);
  switch(status) {
  case SYMBIAN_CURL_OK: break;
  case SYMBIAN_CURL_NONAME: return EAI_NONAME;
  case SYMBIAN_CURL_TIMEOUT: return EAI_AGAIN;
  case SYMBIAN_CURL_NOMEM: return EAI_MEMORY;
  case SYMBIAN_CURL_INVALID: return EAI_NONAME;
  /* The native resolver itself failed: EAI_SYSTEM lets curl_addrinfo.c
     resolve through Open C instead. */
  case SYMBIAN_CURL_FAILED: return EAI_SYSTEM;
  default: return EAI_FAIL;
  }
  if(!memchr(records.canonical, 0, sizeof(records.canonical)))
    return EAI_FAIL;
  if(!records.count || records.count > SYMBIAN_CURL_MAX_ADDRESSES)
    return EAI_FAIL;
  /* Preserve IPv6-first Happy Eyeballs candidates, followed by IPv4. */
  for(pass = 0; pass < 2; ++pass) for(i = 0; i < records.count; ++i) {
    const struct SymbianCurlAddress *record = &records.addresses[i];
    struct Curl_addrinfo *node;
    size_t bytes, canon = 0;
    int wanted = pass ? AF_INET : AF_INET6;
    if(record->family != (unsigned)wanted) continue;
    if(family != AF_UNSPEC && family != wanted) continue;
    bytes = wanted == AF_INET6 ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
    if(!first && (flags & AI_CANONNAME) && records.canonical[0])
      canon = strlen(records.canonical) + 1;
    node = calloc(1, sizeof(*node) + bytes + canon);
    if(!node) { Curl_freeaddrinfo(first); return EAI_MEMORY; }
    node->ai_flags = flags;
    node->ai_family = wanted;
    node->ai_socktype = type;
    node->ai_protocol = protocol;
    node->ai_addrlen = (curl_socklen_t)bytes;
    node->ai_addr = (void *)(node + 1);
    if(canon) {
      node->ai_canonname = (char *)node->ai_addr + bytes;
      memcpy(node->ai_canonname, records.canonical, canon);
    }
    if(wanted == AF_INET6) {
      struct sockaddr_in6 *address = (void *)node->ai_addr;
      address->sin6_family = AF_INET6;
      address->sin6_port = htons((unsigned short)port);
      memcpy(&address->sin6_addr, record->bytes, 16);
      address->sin6_scope_id = record->scope;
#ifdef __SYMBIAN32__
      address->sin6_len = sizeof(*address);
#endif
    }
    else {
      struct sockaddr_in *address = (void *)node->ai_addr;
      address->sin_family = AF_INET;
      address->sin_port = htons((unsigned short)port);
      memcpy(&address->sin_addr, record->bytes, 4);
    }
    if(tail) tail->ai_next = node;
    else first = node;
    tail = node;
  }
  *result = first;
  return first ? 0 : EAI_NONAME;
}
