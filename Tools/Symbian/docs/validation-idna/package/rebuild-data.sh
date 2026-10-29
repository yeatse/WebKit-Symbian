#!/usr/bin/env bash
set -euo pipefail
export SYMBIAN_WEBKIT_WORK=/workspace/symbian-idna-work
export ICU_DIR=/workspace/symbian-idna-work/icu
. /workspace/WebKit-Symbian-graphics/Tools/Symbian/scripts/common.sh
[ "$ICU_DIR" = /workspace/symbian-idna-work/icu ]
host=/workspace/symbian-work/icu/host
# Trim the data archive.
data=$ICU_DIR/data
rm -rf "$data" && mkdir -p "$data/index"
export LD_LIBRARY_PATH=$host/lib
full=$host/data/out/tmp/icudt70l.dat
"$host/bin/icupkg" -l "$full" > "$data/items.txt"
locales=$([ "$ICU_LOCALES" = all ] || echo "$ICU_LOCALES")
python3 "$REPO/scripts/icu-filter.py" "$data/items.txt" $locales > "$data/remove.txt"
"$host/bin/icupkg" -r "$data/remove.txt" "$full" "$data/trimmed.dat"
if [ -n "$locales" ]; then
  # Each tree's res_index lists its installed locales; keep it true.
  mkdir -p "$data/original"
  grep 'res_index.res$' "$data/items.txt" | grep -v '^rbnf/' > "$data/index.txt"
  "$host/bin/icupkg" -x "$data/index.txt" -d "$data/original" "$full"
  pattern="^($(echo $locales | tr ' ' '|'))(_|\$)"
  for tree in "" brkitr coll curr lang region unit zone; do
    out=$data/index/icudt70l/$tree
    mkdir -p "$out"
    { echo "res_index:table(nofallback){"
      (cd "$data/original/$tree" && "$host/bin/derb" -c --to-stdout ./res_index.res) |
        sed -n '/InstalledLocales/,$p' |
        awk -v re="$pattern" '/^ +[A-Za-z0-9_]+ *\{ *"" *\}/ { if ($1 !~ re) next } { print }'
    } > "$out/res_index.txt"
    "$host/bin/genrb" -q -s "$out" -d "$out" res_index.txt
  done
  "$host/bin/icupkg" -a "$data/index.txt" -s "$data/index/icudt70l" "$data/trimmed.dat" "$data/icudt70l.dat"
else
  mv "$data/trimmed.dat" "$data/icudt70l.dat"
fi

mkdir -p "$ICU_DIR/install/share/icu"
cp "$data/icudt70l.dat" "$ICU_DIR/install/share/icu/"
