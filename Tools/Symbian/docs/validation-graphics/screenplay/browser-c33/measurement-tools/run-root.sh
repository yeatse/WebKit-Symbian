#!/usr/bin/env bash
set -euo pipefail
cd /workspace/symbian-screenplay-browser-runtime-harness-v9-work
/usr/bin/sha256sum --check --quiet SHA256SUMS
exec env DISPLAY=:95 XDG_DATA_HOME=/workspace/eka-validation/xdg/data XDG_CONFIG_HOME=/workspace/eka-validation/xdg/config PULSE_SERVER=unix:/workspace/symbian-media-work/runtime/native python3 -B adapter.py --output /workspace/symbian-screenplay-root-browser-host39-v9-run-01-public --private-root /workspace/symbian-screenplay-root-browser-host39-v9-run-01-private --execute
