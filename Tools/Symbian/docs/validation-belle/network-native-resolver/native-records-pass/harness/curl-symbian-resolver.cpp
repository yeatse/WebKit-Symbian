/* Native DNS records only: no allocation, scheduler, thread, logging or ROM shim. */
#include "curl-symbian-resolver.h"
#include <absl/base/nullability.h>
#include <e32base.h>
#include <es_sock.h>
#include <in_sock.h>
#include <string.h>
static_assert(sizeof(SymbianCurlAddress) == 24);
static_assert(sizeof(SymbianCurlAddresses) == 644);
namespace {
int classify(TInt error) {
  if(error == KErrNoMemory) return SYMBIAN_CURL_NOMEM;
  if(error == KErrTimedOut) return SYMBIAN_CURL_TIMEOUT;
  if(error == KErrNotFound || error == KErrEof) return SYMBIAN_CURL_NONAME;
  return SYMBIAN_CURL_FAILED;
}
void copy_name(const TDesC& source, char* absl_nonnull dest) {
  if(source.Length() > SYMBIAN_CURL_MAX_HOSTNAME) return;
  for(TInt i = 0; i < source.Length(); ++i)
    if(source[i] > 127) return;
  for(TInt i = 0; i < source.Length(); ++i) dest[i] = static_cast<char>(source[i]);
  dest[source.Length()] = 0;
}
int append(const TInetAddr& native, int family, SymbianCurlAddresses* absl_nonnull out) {
  SymbianCurlAddress value{};
  if(native.Family() == KAfInet || (native.Family() == KAfInet6 && native.IsV4Mapped())) {
    value.family = KAfInet;
    TUint32 address = native.Address();
    for(unsigned i = 0; i < 4; ++i) value.bytes[i] = static_cast<unsigned char>(address >> (24 - 8 * i));
  }
  else if(native.Family() == KAfInet6) {
    value.family = KAfInet6;
    memcpy(value.bytes, native.Ip6Address().u.iAddr8, 16);
    value.scope = native.Scope();
  }
  else return SYMBIAN_CURL_INVALID;
  if(family && value.family != static_cast<unsigned>(family)) return SYMBIAN_CURL_OK;
  for(unsigned i = 0; i < out->count; ++i)
    if(!memcmp(&out->addresses[i], &value, sizeof(value))) return SYMBIAN_CURL_OK;
  if(out->count == SYMBIAN_CURL_MAX_ADDRESSES) return SYMBIAN_CURL_LIMIT;
  out->addresses[out->count++] = value;
  return SYMBIAN_CURL_OK;
}
}
extern "C" int symbian_curl_resolve_records(const char* absl_nullable host, int family,
    int numeric_only, SymbianCurlAddresses* absl_nullable out) {
  if(!out) return SYMBIAN_CURL_INVALID;
  memset(out, 0, sizeof(*out));
  if(!host || (family && family != KAfInet && family != KAfInet6)) return SYMBIAN_CURL_INVALID;
  TBuf<256> name;
  unsigned length = 0;
  for(; length <= SYMBIAN_CURL_MAX_HOSTNAME && host[length]; ++length) {
    unsigned char c = static_cast<unsigned char>(host[length]);
    /* WebKit/curl passes ASCII IDNA hostnames; reject raw UTF-8, don't truncate. */
    if(c > 127 || !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_' || c == ':' || c == '%'))
      return SYMBIAN_CURL_INVALID;
    name.Append(c);
  }
  if(!length || length > SYMBIAN_CURL_MAX_HOSTNAME) return SYMBIAN_CURL_INVALID;
  TInetAddr numeric;
  if(numeric.Input(name) == KErrNone) {
    int status = append(numeric, family, out);
    copy_name(name, out->canonical);
    return status ? status : (out->count ? SYMBIAN_CURL_OK : SYMBIAN_CURL_NONAME);
  }
  if(numeric_only) return SYMBIAN_CURL_NONAME;
  RSocketServ sockets;
  TInt error = sockets.Connect();
  if(error) return classify(error);
  RHostResolver resolver;
  error = resolver.Open(sockets, KAfInet, KProtocolInetUdp);
  if(error) { sockets.Close(); return classify(error); }
  RTimer timer;
  error = timer.CreateLocal();
  if(error) { resolver.Close(); sockets.Close(); return classify(error); }
  TRequestStatus request, timeout;
  TNameEntry entry;
  timer.After(timeout, TTimeIntervalMicroSeconds32(10000000));
  resolver.GetByName(name, entry, request);
  int status = SYMBIAN_CURL_OK;
  unsigned seen = 0;
  for(;;) {
    User::WaitForRequest(request, timeout);
    if(request.Int() == KRequestPending) {
      resolver.Cancel(); User::WaitForRequest(request); status = SYMBIAN_CURL_TIMEOUT; break;
    }
    if(timeout.Int() != KRequestPending) { status = SYMBIAN_CURL_TIMEOUT; break; }
    error = request.Int();
    if(error == KErrEof) { status = out->count ? SYMBIAN_CURL_OK : SYMBIAN_CURL_NONAME; break; }
    if(error) { status = classify(error); break; }
    const TSockAddr& raw = entry().iAddr;
    if(raw.Length() < 12 || raw.Length() > static_cast<TInt>(KMaxSockAddrSize) ||
        (raw.Family() == KAfInet6 && raw.Length() != 32)) { status = SYMBIAN_CURL_INVALID; break; }
    const TInetAddr address(raw);
    status = append(address, family, out);
    if(status) break;
    if(!out->canonical[0]) copy_name(entry().iName, out->canonical);
    if(++seen == 64) { status = SYMBIAN_CURL_LIMIT; break; }
    resolver.Next(entry, request);
  }
  timer.Cancel(); User::WaitForRequest(timeout);
  timer.Close(); resolver.Close(); sockets.Close();
  if(status) memset(out, 0, sizeof(*out));
  return status;
}
