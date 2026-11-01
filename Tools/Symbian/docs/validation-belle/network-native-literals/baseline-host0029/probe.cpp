/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

// Original Open C networking headers require these prerequisites in this order.
// clang-format off
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
// clang-format on

#include <absl/base/nullability.h>
#include <e32base.h>
#include <errno.h>
#include <es_sock.h>
#include <in_sock.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static_assert(AF_INET == KAfInet && AF_INET6 == KAfInet6);
static_assert(sizeof(sockaddr_in) == 28);
static_assert(sizeof(sockaddr_in6) == 32);
static_assert(offsetof(sockaddr_in, sin_family) == 0);
static_assert(offsetof(sockaddr_in, sin_port) == 2);
static_assert(offsetof(sockaddr_in, sin_addr) == 4);
static_assert(offsetof(sockaddr_in6, sin6_family) == 0);
static_assert(offsetof(sockaddr_in6, sin6_port) == 2);
static_assert(offsetof(sockaddr_in6, sin6_addr) == 4);
static_assert(offsetof(sockaddr_in6, sin6_flowinfo) == 20);
static_assert(offsetof(sockaddr_in6, sin6_scope_id) == 24);
static_assert(offsetof(sockaddr_in6, sin6_len) == 28);
static_assert(sizeof(TInetAddr) == 40 && sizeof(TNameRecord) == 564);
static_assert(sizeof(TIp6Addr) == 16 && sizeof(SInet6Addr) == 24);
static_assert(KMaxSockAddrSize == 32);

namespace {

constexpr unsigned kMaximumResults = 16;
constexpr unsigned char kV4[4] = {127, 0, 0, 1};
constexpr unsigned char kV6[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                   0, 0, 0, 0, 0, 0, 0, 1};
unsigned passed = 0;
unsigned failed = 0;

struct Results {
  int code = 0;
  unsigned count = 0;
  unsigned v4 = 0;
  unsigned v6 = 0;
  bool valid = true;
  bool complete = true;
};

const char* absl_nonnull JsonBool(bool value) {
  return value ? "true" : "false";
}

void Case(const char* absl_nonnull name, bool success, const Results& result) {
  printf(
      "CASE {\"name\":\"%s\",\"pass\":%s,\"rc\":%d,"
      "\"count\":%u,\"v4\":%u,\"v6\":%u,\"valid\":%s,"
      "\"complete\":%s}\n",
      name, JsonBool(success), result.code, result.count, result.v4, result.v6,
      JsonBool(result.valid), JsonBool(result.complete));
  fflush(stdout);
  success ? ++passed : ++failed;
}

bool NumericName(const sockaddr* absl_nonnull address, socklen_t length,
                 int family, const unsigned char* absl_nonnull expected) {
  char host[NI_MAXHOST] = {};
  char service[NI_MAXSERV] = {};
  const int result =
      getnameinfo(address, length, host, sizeof(host), service, sizeof(service),
                  NI_NUMERICHOST | NI_NUMERICSERV);
  unsigned char binary[16] = {};
  const bool match = !result && inet_pton(family, host, binary) == 1 &&
                     !memcmp(binary, expected, family == AF_INET ? 4 : 16) &&
                     !strcmp(service, "443");
  // Never print a returned name/address: only fixed inputs and result flags.
  printf("INFO numeric-name family=%d rc=%d hostMatch=%s serviceMatch=%s\n",
         family, result, JsonBool(match), JsonBool(!strcmp(service, "443")));
  return match;
}

void ManualNameInfo() {
  sockaddr_in v4 = {};
  v4.sin_family = AF_INET;
  v4.sin_port = htons(443);
  Results first;
  first.code = inet_pton(AF_INET, "127.0.0.1", &v4.sin_addr);
  first.count = 1;
  first.v4 = 1;
  first.valid =
      first.code == 1 && NumericName(reinterpret_cast<const sockaddr*>(&v4),
                                     sizeof(v4), AF_INET, kV4);
  Case("nameinfo-manual-v4", first.valid, first);
  sockaddr_in6 v6 = {};
  v6.sin6_family = AF_INET6;
  v6.sin6_port = htons(443);
  v6.sin6_len = sizeof(v6);
  Results second;
  second.code = inet_pton(AF_INET6, "::1", &v6.sin6_addr);
  second.count = 1;
  second.v6 = 1;
  second.valid = second.code == 1 && !memcmp(&v6.sin6_addr, kV6, sizeof(kV6)) &&
                 NumericName(reinterpret_cast<const sockaddr*>(&v6), sizeof(v6),
                             AF_INET6, kV6);
  Case("nameinfo-manual-v6", second.valid, second);
}

Results AddressInfo(const char* absl_nonnull name, int family, bool numeric) {
  addrinfo hints = {};
  hints.ai_family = family;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_NUMERICSERV | (numeric ? AI_NUMERICHOST : 0);
  addrinfo* absl_nullable first = nullptr;
  Results result;
  errno = 0;
  result.code = getaddrinfo(name, "443", &hints, &first);
  const int system_error = errno;
  printf("INFO getaddrinfo fixedInput=%s family=%d numeric=%s rc=%d errno=%d\n",
         name, family, JsonBool(numeric), result.code, system_error);
  if (result.code) {
    result.valid = false;
    // Ownership of a result is established only by a successful call.
    return result;
  }
  result.valid = first != nullptr;
  const addrinfo* absl_nullable current = first;
  for (; current && result.count < kMaximumResults;
       current = current->ai_next) {
    ++result.count;
    if (!current->ai_addr ||
        current->ai_addrlen < sizeof(current->ai_addr->sa_family) ||
        current->ai_socktype != SOCK_STREAM ||
        current->ai_protocol != IPPROTO_TCP ||
        (family != AF_UNSPEC && current->ai_family != family)) {
      result.valid = false;
      continue;
    }
    if (current->ai_family == AF_INET) {
      ++result.v4;
      if (current->ai_addrlen != sizeof(sockaddr_in)) {
        result.valid = false;
        continue;
      }
      const auto* absl_nonnull address =
          reinterpret_cast<const sockaddr_in*>(current->ai_addr);
      const bool correct = address->sin_family == AF_INET &&
                           address->sin_port == htons(443) &&
                           !memcmp(&address->sin_addr, kV4, sizeof(kV4));
      result.valid &= correct;
      if (correct) {
        result.valid &=
            NumericName(current->ai_addr, current->ai_addrlen, AF_INET, kV4);
      }
    } else if (current->ai_family == AF_INET6) {
      ++result.v6;
      if (current->ai_addrlen != sizeof(sockaddr_in6)) {
        result.valid = false;
        continue;
      }
      const auto* absl_nonnull address =
          reinterpret_cast<const sockaddr_in6*>(current->ai_addr);
      const bool correct = address->sin6_family == AF_INET6 &&
                           address->sin6_port == htons(443) &&
                           !memcmp(&address->sin6_addr, kV6, sizeof(kV6));
      result.valid &= correct;
      if (correct) {
        result.valid &=
            NumericName(current->ai_addr, current->ai_addrlen, AF_INET6, kV6);
      }
    } else {
      result.valid = false;
    }
  }
  result.complete = current == nullptr;
  // A normal fixed-host result is tiny. Do not traverse an unbounded/cyclic
  // list again through freeaddrinfo after the enumeration contract failed.
  if (result.complete && first) {
    freeaddrinfo(first);
  }
  return result;
}

void AddressInfoCase(const char* absl_nonnull label,
                     const char* absl_nonnull name, int family, bool numeric,
                     bool need_v4, bool need_v6) {
  const auto result = AddressInfo(name, family, numeric);
  const bool success = !result.code && result.valid && result.complete &&
                       result.count && (!need_v4 || result.v4) &&
                       (!need_v6 || result.v6) &&
                       (!numeric || (need_v4 ? !result.v6 : !result.v4));
  Case(label, success, result);
}

void NativeLiteral(const char* absl_nonnull label, const TDesC& name,
                   bool ipv6) {
  TInetAddr address;
  Results result;
  result.code = address.Input(name);
  result.count = !result.code;
  result.valid = !result.code && address.Length() >= 12 &&
                 address.Length() <= static_cast<TInt>(KMaxSockAddrSize);
  if (result.valid && ipv6) {
    result.v6 = address.Family() == KAfInet6;
    result.valid = result.v6 && address.Length() == 32 &&
                   !memcmp(address.Ip6Address().u.iAddr8, kV6, sizeof(kV6));
  } else if (result.valid) {
    // Native IPv4 may also be represented as an IPv4-mapped IPv6 address.
    result.v4 = address.Family() == KAfInet ||
                (address.Family() == KAfInet6 && address.Length() == 32 &&
                 address.IsV4Mapped());
    result.valid = result.v4 && address.Address() == KInetAddrLoop;
  }
  if (result.valid) {
    address.SetPort(443);
    result.valid = address.Port() == 443;
  }
  Case(label, result.valid, result);
}

TInt WaitForResolver(RHostResolver* absl_nonnull resolver,
                     RTimer* absl_nonnull timer,
                     TRequestStatus* absl_nonnull request) {
  TRequestStatus timeout;
  timer->After(timeout, TTimeIntervalMicroSeconds32(3000000));
  User::WaitForRequest(*request, timeout);
  const bool timed_out = request->Int() == KRequestPending;
  if (timed_out) {
    resolver->Cancel();
    User::WaitForRequest(*request);
  }
  timer->Cancel();
  User::WaitForRequest(timeout);
  return timed_out ? KErrTimedOut : request->Int();
}

Results NativeResolve(RSocketServ* absl_nonnull sockets, const TDesC& name) {
  Results result;
  RHostResolver resolver;
  // Match the native resolver provider used by the public Open C backend.
  // This does not open/send an application UDP socket or claim UDP IPv6.
  result.code = resolver.Open(*sockets, KAfInet, KProtocolInetUdp);
  if (result.code) {
    result.valid = false;
    return result;
  }
  RTimer timer;
  result.code = timer.CreateLocal();
  if (result.code) {
    resolver.Close();
    result.valid = false;
    return result;
  }
  TNameEntry record;
  TRequestStatus request;
  resolver.GetByName(name, record, request);
  for (;;) {
    const TInt error = WaitForResolver(&resolver, &timer, &request);
    if (error == KErrEof && result.count) {
      break;
    }
    if (error) {
      result.code = error;
      result.valid = false;
      break;
    }
    ++result.count;
    const TSockAddr& raw = record().iAddr;
    if (raw.Length() < 12 ||
        raw.Length() > static_cast<TInt>(KMaxSockAddrSize)) {
      result.valid = false;
    } else {
      const TInetAddr address(raw);
      if (address.Family() == KAfInet) {
        ++result.v4;
        result.valid &= address.Address() == KInetAddrLoop;
      } else if (address.Family() == KAfInet6 && raw.Length() == 32) {
        const bool v6 =
            !memcmp(address.Ip6Address().u.iAddr8, kV6, sizeof(kV6));
        const bool mapped_v4 =
            address.IsV4Mapped() && address.Address() == KInetAddrLoop;
        if (v6) {
          ++result.v6;
        }
        if (mapped_v4) {
          ++result.v4;
        }
        result.valid &= v6 || mapped_v4;
      } else {
        result.valid = false;
      }
    }
    if (result.count == kMaximumResults) {
      result.complete = false;
      break;
    }
    resolver.Next(record, request);
  }
  resolver.Close();
  timer.Close();
  return result;
}

void NativeResolverCase(RSocketServ* absl_nonnull sockets,
                        const char* absl_nonnull label, const TDesC& name,
                        bool need_v4, bool need_v6) {
  const auto result = NativeResolve(sockets, name);
  const bool success = !result.code && result.valid && result.complete &&
                       result.count && (!need_v4 || result.v4) &&
                       (!need_v6 || result.v6) && (need_v4 || !result.v4) &&
                       (need_v6 || !result.v6);
  Case(label, success, result);
}

bool ReadArguments(char* absl_nonnull token, bool* absl_nonnull literals_only) {
  TBuf<192> arguments;
  if (User::CommandLineLength() > arguments.MaxLength()) {
    return false;
  }
  User::CommandLine(arguments);
  arguments.Trim();
  const TPtrC prefix = _L("--run ");
  if (arguments.Length() <= prefix.Length() ||
      arguments.Left(prefix.Length()) != prefix) {
    return false;
  }
  const TPtrC rest = arguments.Mid(prefix.Length());
  const TInt space = rest.Locate(' ');
  const TInt length = space == KErrNotFound ? rest.Length() : space;
  if (length < 8 || length > 64) {
    return false;
  }
  for (TInt index = 0; index < length; ++index) {
    const TText c = rest[index];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '_')) {
      return false;
    }
    token[index] = static_cast<char>(c);
  }
  token[length] = '\0';
  *literals_only = space != KErrNotFound;
  return !*literals_only || rest.Mid(space) == _L(" --literals-only");
}

}  // namespace

int main() {
  char token[65] = {};
  bool literals_only = false;
  if (!ReadArguments(token, &literals_only)) {
    return 2;
  }
  if (!freopen("C:/Data/WebKit/network-native-contract.log", "w", stdout)) {
    return 2;
  }
  printf(
      "BEGIN {\"suite\":\"network-native-contract\",\"version\":1,"
      "\"run\":\"%s\",\"mode\":\"%s\"}\n",
      token, literals_only ? "literals" : "all");
  printf(
      "INFO ABI AF_INET=%d AF_INET6=%d sockaddr_in=%u sockaddr_in6=%u "
      "nativeInet=%u nativePayload=%u IPv6AddressBytes=16\n",
      AF_INET, AF_INET6, static_cast<unsigned>(sizeof(sockaddr_in)),
      static_cast<unsigned>(sizeof(sockaddr_in6)),
      static_cast<unsigned>(sizeof(TInetAddr)), KMaxSockAddrSize);
  Case("abi", true, {});
  NativeLiteral("native-literal-v4", _L("127.0.0.1"), false);
  NativeLiteral("native-literal-v6", _L("::1"), true);
  ManualNameInfo();
  AddressInfoCase("addrinfo-numeric-v4-af4", "127.0.0.1", AF_INET, true, true,
                  false);
  AddressInfoCase("addrinfo-numeric-v4-unspec", "127.0.0.1", AF_UNSPEC, true,
                  true, false);
  AddressInfoCase("addrinfo-numeric-v6-af6", "::1", AF_INET6, true, false,
                  true);
  AddressInfoCase("addrinfo-numeric-v6-unspec", "::1", AF_UNSPEC, true, false,
                  true);
  RSocketServ sockets;
  const TInt connected = sockets.Connect();
  printf("INFO socket-server rc=%d\n", connected);
  if (!connected) {
    NativeResolverCase(&sockets, "native-resolver-v4-literal", _L("127.0.0.1"),
                       true, false);
    NativeResolverCase(&sockets, "native-resolver-v6-literal", _L("::1"), false,
                       true);
    if (!literals_only) {
      NativeResolverCase(&sockets, "native-resolver-localhost", _L("localhost"),
                         true, true);
    }
    sockets.Close();
  } else {
    Results error;
    error.code = connected;
    error.valid = false;
    Case("native-resolver-v4-literal", false, error);
    Case("native-resolver-v6-literal", false, error);
    if (!literals_only) {
      Case("native-resolver-localhost", false, error);
    }
  }
  if (!literals_only) {
    AddressInfoCase("addrinfo-localhost-af4", "localhost", AF_INET, false, true,
                    false);
    AddressInfoCase("addrinfo-localhost-af6", "localhost", AF_INET6, false,
                    false, true);
    AddressInfoCase("addrinfo-localhost-unspec", "localhost", AF_UNSPEC, false,
                    true, true);
  }
  printf(
      "RESULT {\"suite\":\"network-native-contract\",\"version\":1,"
      "\"run\":\"%s\",\"mode\":\"%s\",\"passed\":%u,\"failed\":%u,"
      "\"total\":%u}\n",
      token, literals_only ? "literals" : "all", passed, failed,
      passed + failed);
  printf("network-native-contract exiting %d\n", failed ? 1 : 0);
  fflush(stdout);
  fclose(stdout);
  return failed ? 1 : 0;
}
