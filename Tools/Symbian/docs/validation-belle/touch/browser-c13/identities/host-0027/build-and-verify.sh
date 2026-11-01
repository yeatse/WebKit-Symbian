#!/usr/bin/env bash
set -euo pipefail
if pgrep -x eka2l1_qt >/dev/null; then echo 'Refusing host build while emulator is running' >&2; exit 1; fi
. /workspace/host-build-env.sh
export LIBRARY_PATH=/workspace/host-sysroot/usr/lib/x86_64-linux-gnu${LIBRARY_PATH:+:$LIBRARY_PATH}
export CPLUS_INCLUDE_PATH=/workspace/host-sysroot/usr/include/x86_64-linux-gnu${CPLUS_INCLUDE_PATH:+:$CPLUS_INCLUDE_PATH}
export C_INCLUDE_PATH=/workspace/host-sysroot/usr/include/x86_64-linux-gnu${C_INCLUDE_PATH:+:$C_INCLUDE_PATH}
ninja -j2 -C /workspace/EKA2L1-validation/build eka2l1_qt > /workspace/symbian-belle-work/host-0027-20261101/build.log 2>&1
python3 - <<'PY'
from pathlib import Path
import shutil,hashlib,json
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
base=Path('/workspace/EKA2L1-validation/build/bin');source=Path('/workspace/symbian-media-work/native-patches-20261029T151914Z');assets={}
for name,expected in [('mediaclientaudio_general.dll','a07674f2957410a00f0bcdbc997590feca50eb028147a1497934713d8aa65d49'),('mediaclientvideo_v100.dll','03fd46f1ce54a2b5863fe841e87bdc17b99fbd05ef99459f5a56b7dbd75209c3')]:
 src=source/name;assert sha(src)==expected;shutil.copy2(src,base/'patch'/name);assert sha(base/'patch'/name)==expected;assets[name]={'sha256':expected,'bytes':src.stat().st_size}
for name,expected in [('mediaclientaudio.dll.map','5fadc737e5182ddd24230885af23717cd7537daf7427501ff09e43a9e40df5d3'),('mediaclientvideo.dll.map','1d0f3e76bcbc25d65207b0cb5674bcbbc9c19e58bdf0959b2d3ac0f7f9824282')]:
 p=base/'patch'/name;assert sha(p)==expected;assets[name]={'sha256':sha(p),'bytes':p.stat().st_size}
exe=base/'eka2l1_qt';result={'emulator_sha256':sha(exe),'bytes':exe.stat().st_size,'native_patch_assets':assets,'diagnostic_instrumentation_added':False,'change':'Patches through 0027: normal Ctrl/Alt keyboard state and ASCII editing profile; no diagnostic instrumentation'}
Path('/workspace/symbian-belle-work/host-0027-20261101/identities.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
PY
