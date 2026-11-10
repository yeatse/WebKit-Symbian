#!/usr/bin/env bash
# ICU 70.1 for JavaScriptCore Intl/collation and browser URL processing: a host build (for the data
# tools and the data archive), then static Symbian libraries. The data archive
# retains UTS #46 domain-name data and selected locales (see icu-filter.py and ICU_LOCALES)
# and either installed as icudt70l.dat for ICU to read at run time from
# ICU_DATA_DIR (ICU_DATA=file) or linked in as libicudata.a (ICU_DATA=static).
. "$(dirname "$0")/common.sh"
require_sdk
src=$ICU_DIR/src
if [ ! -d "$src/.git" ]; then
  git init -q "$src"
  git -C "$src" remote add origin https://github.com/unicode-org/icu
  git -C "$src" sparse-checkout init --cone
  git -C "$src" sparse-checkout set icu4c/source
  git -C "$src" fetch -q --depth 1 --filter=blob:none origin "refs/tags/$ICU_TAG"
  git -C "$src" checkout -q FETCH_HEAD
fi
for patch in "$REPO"/patches/icu-70.1/*.patch; do
  if ! git -C "$src" apply --reverse --check "$patch" 2>/dev/null; then
    git -C "$src" apply "$patch"
    # ICU's makefiles do not track header changes: rebuild the target.
    rm -rf "${ICU_DIR:?}/symbian"
  fi
done
host=$ICU_DIR/host
if [ ! -f "$host/data/out/tmp/icudt70l.dat" ]; then
  mkdir -p "$host"
  (cd "$host" && "$src/icu4c/source/configure" --disable-tests --disable-samples \
     --disable-extras --enable-static --disable-shared && make -j"$JOBS") \
    > "$ICU_DIR/host.log" 2>&1
fi
target=$ICU_DIR/symbian
mkdir -p "$target"
# Read the data file with stdio (Open C's mmap does not map files reliably) from
# ICU_DATA_DIR unless the ICU_DATA environment variable or u_setDataDirectory
# says otherwise.
cat > "$ICU_DIR/symbian-config.h" <<EOF
#define U_HAVE_MMAP 0
#define ICU_DATA_DIR "$ICU_DATA_DIR"
EOF
# No exceptions, as everywhere in the image. ICU keeps RTTI (it uses
# dynamic_cast); e32/CMakeLists.txt links its typeinfo as read-only imports.
(cd "$target" && CC="$REPO/toolchain/sym-cc" CXX="$REPO/toolchain/sym-c++" \
   AR="$SDK/bin/llvm-ar" RANLIB="$SDK/bin/llvm-ranlib" \
   CPPFLAGS="-DU_USING_ICU_NAMESPACE=0 -DU_HAVE_NL_LANGINFO_CODESET=0 -include $ICU_DIR/symbian-config.h" \
   CFLAGS="$OPTIMIZE" CXXFLAGS="$OPTIMIZE -std=c++11 -fno-exceptions" \
   "$src/icu4c/source/configure" --host=arm-linux-gnueabi \
     --with-cross-build="$host" --disable-shared --enable-static \
     --disable-tests --disable-samples --disable-extras --disable-tools \
     --disable-layoutex --with-data-packaging=static --prefix="$ICU_DIR/install" \
   && mkdir -p lib bin && make -j"$JOBS" -C common && make -j"$JOBS" -C i18n && make -j"$JOBS" -C io \
   && make -C common install && make -C i18n install && make -C io install) \
  > "$ICU_DIR/symbian.log" 2>&1

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

mkdir -p "$ICU_DIR/install/lib" "$ICU_DIR/install/share/icu"
rm -f "$ICU_DIR/install/lib/libicudata.a" "$ICU_DIR/install/share/icu/icudt70l.dat"
if [ "$ICU_DATA" = static ]; then
  "$host/bin/genccode" -a gcc -e icudt70 -d "$data" "$data/icudt70l.dat" > /dev/null
  "$REPO/toolchain/sym-cc" -c "$data/icudt70l_dat.S" -o "$data/icudata.o"
else
  # ICU's stub: an empty common data, so the file is looked up at run time.
  "$REPO/toolchain/sym-c++" $OPTIMIZE -I"$src/icu4c/source/common" -c \
    "$src/icu4c/source/stubdata/stubdata.cpp" -o "$data/icudata.o"
  cp "$data/icudt70l.dat" "$ICU_DIR/install/share/icu/"
fi
"$SDK/bin/llvm-ar" rcs "$ICU_DIR/install/lib/libicudata.a" "$data/icudata.o"
ls -la "$ICU_DIR/install/lib" "$ICU_DIR/install/share/icu"
