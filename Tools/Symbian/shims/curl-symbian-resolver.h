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
/* Called, from curl's resolver thread, with the Symbian error of each
   distinct native resolver failure; lookups then go through Open C. */
void symbian_curl_set_resolver_observer(void (*)(int));
struct addrinfo;
struct Curl_addrinfo;
int Curl_symbian_getaddrinfo(const char *, const char *, const struct addrinfo *, struct Curl_addrinfo **);
#ifdef __cplusplus
}
#endif
#endif
