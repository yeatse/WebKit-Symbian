#!/bin/sh
set -eu
: "${FFMPEG_PREFIX:?Set this to the EKA FFmpeg install directory}"
: "${AMR_LIBRARY:?Set this to the opencore-amrnb archive used by that FFmpeg build}"
fixture=${1:?Pass the generated video.mp4 fixture}
output=${2:?Pass a scratch output directory}
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$output"
python3 - "$fixture" <<'PY'
import hashlib,sys
from pathlib import Path
p=Path(sys.argv[1])
assert hashlib.sha256(p.read_bytes()).hexdigest() == '8e1a6daf8220ce796b29e6cf5db328e1362186c3d5df42974bb1828c2cb8cb85'
PY
"${CC:-cc}" -O2 -I"$FFMPEG_PREFIX/include" "$source_dir/partial-packet.c" \
    -L"$FFMPEG_PREFIX/lib" -lavformat -lavcodec -lavutil "$AMR_LIBRARY" \
    -lm -latomic -lz -pthread -lstdc++ -o "$output/partial-packet"
"${CXX:-c++}" -O2 -std=c++17 -I"$FFMPEG_PREFIX/include" "$source_dir/read-ahead.cpp" \
    -L"$FFMPEG_PREFIX/lib" -lavformat -lavcodec -lavutil "$AMR_LIBRARY" \
    -lm -latomic -lz -pthread -o "$output/read-ahead"
"$output/partial-packet" "$fixture" "$output" > "$output/partial-packet.jsonl" 2> "$output/partial-packet-ffmpeg.log"
"$output/read-ahead" "$fixture" "$output" > "$output/read-ahead.jsonl" 2> "$output/read-ahead-ffmpeg.log"
cat "$output/partial-packet.jsonl" "$output/read-ahead.jsonl"
