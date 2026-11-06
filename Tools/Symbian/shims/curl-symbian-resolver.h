/* Private, fixed-size C ABI between curl's allocator and the native resolver. */
#ifndef CURL_SYMBIAN_RESOLVER_H
#define CURL_SYMBIAN_RESOLVER_H
#include <stdint.h>
#define SYMBIAN_CURL_MAX_ADDRESSES 16
#define SYMBIAN_CURL_MAX_HOSTNAME 253
enum SymbianCurlResolveStatus { SYMBIAN_CURL_OK, SYMBIAN_CURL_NONAME,
  SYMBIAN_CURL_TIMEOUT, SYMBIAN_CURL_NOMEM, SYMBIAN_CURL_INVALID,
  SYMBIAN_CURL_FAILED, SYMBIAN_CURL_LIMIT };
struct SymbianCurlAddress { uint32_t family; unsigned char bytes[16]; uint32_t scope; };
struct SymbianCurlAddresses { uint32_t count; struct SymbianCurlAddress addresses[SYMBIAN_CURL_MAX_ADDRESSES]; char canonical[256]; };
#ifdef __cplusplus
extern "C" {
#endif
int symbian_curl_resolve_records(const char *, int, int, struct SymbianCurlAddresses *);
/* The started connection (access point and network ids) lookups attach to.
   Zero ids detach: lookups then use the socket server's implicit connection. */
void symbian_curl_set_connection(uint32_t iap, uint32_t network);
struct addrinfo;
struct Curl_addrinfo;
int Curl_symbian_getaddrinfo(const char *, const char *, const struct addrinfo *, struct Curl_addrinfo **);
#ifdef __cplusplus
}
#endif
#endif
