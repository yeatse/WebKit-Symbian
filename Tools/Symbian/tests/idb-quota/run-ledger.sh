#!/usr/bin/env bash
set -euo pipefail
test_dir=$(cd -- "$(dirname -- "$0")" && pwd)
checkout=$(cd -- "$test_dir/../../../.." && pwd)
test_work=${TEST_WORK:-$(mktemp -d)}
mkdir -p "$test_work"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -O2 \
  -I"$checkout/Source/WebCore/Modules/indexeddb/server" \
  "$test_dir/ledger.cpp" -o "$test_work/ledger"
"$test_work/ledger"
