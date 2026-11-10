#!/usr/bin/env bash
set -euo pipefail
CHECKOUT=$(cd "$(dirname "$0")/../../../.." && pwd)
TEST_WORK=${TEST_WORK:-$(mktemp -d)}
mkdir -p "$TEST_WORK"
# Use the port's exact SQLite amalgamation when available; callers may provide
# SQLITE_SOURCE explicitly. Do not substitute a mock database or mock COMMIT.
SQLITE_SOURCE=${SQLITE_SOURCE:-${SYMBIAN_WEBKIT_WORK:-$CHECKOUT/WebKitBuild/Symbian}/deps/src/sqlite-amalgamation-3350500}
if [[ ! -f "$SQLITE_SOURCE/sqlite3.c" ]]; then
    printf 'Set SQLITE_SOURCE to the port SQLite amalgamation directory.\n' >&2
    exit 2
fi
"${CC:-cc}" -O2 -DSQLITE_OMIT_WAL=1 -DSQLITE_THREADSAFE=1 -I"$SQLITE_SOURCE" -c "$SQLITE_SOURCE/sqlite3.c" -o "$TEST_WORK/sqlite3.o"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -O2 -I"$CHECKOUT/Source/WebCore/Modules/indexeddb/server" -I"$SQLITE_SOURCE" "$CHECKOUT/Tools/Symbian/tests/idb-quota/blob-commit.cpp" "$TEST_WORK/sqlite3.o" -ldl -lpthread -o "$TEST_WORK/blob-commit"
"$TEST_WORK/blob-commit" "$TEST_WORK/files"
