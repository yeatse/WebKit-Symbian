#!/usr/bin/env python3
"""Check binary MathIC patch-prefix preservation using existing ARMv6 JSC libraries."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess

p = argparse.ArgumentParser(description=__doc__)
for option in ["checkout", "qemu-build", "sysroot", "qemu", "work"]:
    p.add_argument("--" + option, type=Path, required=True)
a = p.parse_args()
a.work.mkdir(parents=True, exist_ok=True)
cache = (a.qemu_build / "CMakeCache.txt").read_text()
original = Path(re.search(r"^CMAKE_HOME_DIRECTORY:INTERNAL=(.*)$", cache, re.M)[1])
paths = ["jit/JITMathIC.h", "jit/JITAddGenerator.cpp", "jit/JITSubGenerator.cpp", "jit/JITMulGenerator.cpp",
    "assembler/MacroAssemblerARM.h", "assembler/ARMAssembler.h", "assembler/AssemblerBufferWithConstantPool.h"]
identities = {}
for relative in paths:
    path = "Source/JavaScriptCore/" + relative
    current = (a.checkout / path).read_bytes()
    if current != (original / path).read_bytes():
        raise SystemExit("prebuilt MathIC/assembler source differs: " + path)
    identities[path] = hashlib.sha256(current).hexdigest()
ninja = (a.qemu_build / "build.ninja").read_text()
block = re.search(r"^build .*jsc\.cpp\.o:.*\n((?:  .*\n)+)", ninja, re.M)[1]
fields = dict(re.findall(r"^  (\w+) = (.*)$", block, re.M))
flags = shlex.split(fields["DEFINES"] + " " + fields["FLAGS"] + " " + fields["INCLUDES"])
block = re.search(r"^build bin/jsc:.*\n((?:  .*\n)+)", ninja, re.M)[1]
libraries = shlex.split(dict(re.findall(r"^  (\w+) = (.*)$", block, re.M))["LINK_LIBRARIES"])
compiler = [str(a.sysroot / "usr/bin/arm-linux-gnueabi-g++"), "--sysroot=" + str(a.sysroot)]
source = Path(__file__).with_name("math-review.cpp")
obj, binary = a.work / "probe.o", a.work / "probe"
with (a.work / "build.log").open("w") as log:
    subprocess.run(compiler + flags + ["-c", str(source), "-o", str(obj)], cwd=a.qemu_build,
        stdout=log, stderr=subprocess.STDOUT, check=True)
    subprocess.run(compiler + ["-march=armv6", "-marm", "-mfpu=vfp", "-mfloat-abi=softfp",
        "-Wl,--gc-sections", str(obj), "-o", str(binary)] + libraries, cwd=a.qemu_build,
        stdout=log, stderr=subprocess.STDOUT, check=True)
with (a.work / "guest.log").open("w") as log:
    result = subprocess.run([str(a.qemu), "-cpu", "arm1136", "-L", str(a.sysroot / "usr/arm-linux-gnueabi"), str(binary)],
        stdout=log, stderr=subprocess.STDOUT, timeout=30)
raw = (a.work / "guest.log").read_bytes()
print(raw.decode(), end="")
if result.returncode or b"RESULT cases=29700 repatched=26400 corruptions=0" not in raw:
    raise SystemExit("MathIC prefix review failed")
(a.work / "result.json").write_text(json.dumps({"guestExit": result.returncode, "sourceHashes": identities,
    "fixtureSHA256": hashlib.sha256(source.read_bytes()).hexdigest(),
    "guestLogSHA256": hashlib.sha256(raw).hexdigest(), "cases": 29700, "repatched": 26400,
    "corruptions": 0, "scope": "Add/Sub/Mul empty/int/double profiles, emission and 12-byte replacement-prefix preservation; no generated arithmetic execution"}, indent=2) + "\n")
