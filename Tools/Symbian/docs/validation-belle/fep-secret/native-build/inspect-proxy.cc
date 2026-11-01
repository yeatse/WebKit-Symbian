#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <set>
#include <string>
#include <absl/base/nullability.h>
#include "symbian/sdk/exports.h"
int main(int argc, char* absl_nullable* absl_nonnull argv) {
  if (argc < 3) return 2;
  std::ifstream input(argv[1], std::ios::binary);
  std::string data((std::istreambuf_iterator<char>(input)), {});
  if (!input || data.empty() || data.size() > 16 * 1024 * 1024) return 3;
  const auto proxy = symbian::sdk::InspectProxy(data);
  if (!proxy.ok()) { std::cerr << proxy.status(); return 4; }
  std::set<uint32_t> needed;
  for (int i = 2; i < argc; ++i) needed.insert(static_cast<uint32_t>(std::strtoul(argv[i], nullptr, 10)));
  std::cout << "{\"status\":\"PASS\",\"target_dll\":" << std::quoted(proxy->target_dll)
            << ",\"proxy_present_bindings\":" << proxy->exports.size() << ",\"used_bindings\":[";
  bool first = true;
  for (const auto& item : proxy->exports) {
    if (!needed.erase(item.ordinal)) continue;
    if (!first) std::cout << ',';
    first = false;
    std::cout << "{\"ordinal\":" << item.ordinal << ",\"symbol\":" << std::quoted(item.symbol)
              << ",\"data\":" << (item.data ? "true" : "false") << '}';
  }
  if (!needed.empty()) { std::cerr << "Missing imported ordinal\n"; return 5; }
  std::cout << "]}\n";
  return 0;
}
