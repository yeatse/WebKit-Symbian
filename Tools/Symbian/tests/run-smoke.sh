#!/usr/bin/env bash
set -euo pipefail
test_dir=$(cd -- "$(dirname -- "$0")" && pwd)
test_work=$(mktemp -d)
trap 'rm -rf "$test_work"' EXIT

bash "$test_dir/native-memory/run.sh"
bash "$test_dir/native-filesystem/run.sh"
TEST_WORK="$test_work/idb-quota" bash "$test_dir/idb-quota/run-ledger.sh"
python3 "$test_dir/cookie-samesite/test-host.py"
python3 "$test_dir/screenplay/run.py"
python3 "$test_dir/screenplay-update/run.py"
python3 "$test_dir/benchmarks/test-summarize.py"

printf 'PASS: Symbian host smoke tests\n'
