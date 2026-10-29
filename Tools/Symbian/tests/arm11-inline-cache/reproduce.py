#!/usr/bin/env python3
"""Run the production IC emitter with a prebuilt ARMv6 Linux JSC's dependencies."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--checkout", type=Path, required=True)
p.add_argument("--qemu-build", type=Path, required=True)
p.add_argument("--sysroot", type=Path, required=True)
p.add_argument("--qemu", type=Path, required=True)
p.add_argument("--work", type=Path, required=True)
p.add_argument("--baseline", default="17cf20e6748e87a1749f11b1bf018cf2304eb392")
a = p.parse_args()
a.work.mkdir(parents=True, exist_ok=True)
relative = "Source/JavaScriptCore/jit/JITInlineCacheGenerator.cpp"
old = subprocess.check_output(["git", "show", a.baseline + ":" + relative], cwd=a.checkout).decode()
new = (a.checkout / relative).read_text()
ninja = (a.qemu_build / "build.ninja").read_text()
block = re.search(r"^build .*jsc\.cpp\.o:.*\n((?:  .*\n)+)", ninja, re.M).group(1)
fields = dict(re.findall(r"^  (\w+) = (.*)$", block, re.M))
flags = shlex.split(fields["DEFINES"] + " " + fields["FLAGS"] + " " + fields["INCLUDES"])
# Use current source headers with the existing build's generated configuration.
overlay = a.work / "headers/JavaScriptCore"
overlay.mkdir(parents=True, exist_ok=True)
for header in (a.qemu_build / "JavaScriptCore/PrivateHeaders/JavaScriptCore").iterdir():
    resolved = header.resolve()
    if "/Source/" not in str(resolved):
        continue
    current = a.checkout / ("Source/" + str(resolved).split("/Source/", 1)[1])
    if current.is_file():
        target = overlay / header.name
        if target.is_symlink():
            target.unlink()
        target.symlink_to(current)
flags = ["-I" + str(overlay.parent), "-I" + str(a.checkout / "Source/JavaScriptCore/jit")] + flags
link = re.search(r"^build bin/jsc:.*\n((?:  .*\n)+)", ninja, re.M).group(1)
libraries = shlex.split(dict(re.findall(r"^  (\w+) = (.*)$", link, re.M))["LINK_LIBRARIES"])
compiler = a.sysroot / "usr/bin/arm-linux-gnueabi-g++"
records = []
for name, source, expected in [("before", old, 1), ("after", new, 0)]:
    work = a.work / name
    work.mkdir(exist_ok=True)
    start = source.index("void JITByIdGenerator::generateFastCommon(")
    end = source.index("\n}\n", start) + 2
    method = source[start:end].replace("JITByIdGenerator::", "ProbeGenerator::", 1)
    (work / "method.inc").write_text(method + "\n")
    obj, binary = work / "probe.o", work / "probe"
    command = [str(compiler), "--sysroot=" + str(a.sysroot)] + flags + ["-I" + str(work),
        "-c", str(Path(__file__).with_name("probe.cpp")), "-o", str(obj)]
    with (work / "build.log").open("w") as log:
        subprocess.run(command, cwd=a.qemu_build, stdout=log, stderr=subprocess.STDOUT, check=True)
        subprocess.run([str(compiler), "--sysroot=" + str(a.sysroot), "-march=armv6", "-marm", "-mfpu=vfp", "-mfloat-abi=softfp",
            "-Wl,--gc-sections", str(obj), "-o", str(binary)] + libraries,
            cwd=a.qemu_build, stdout=log, stderr=subprocess.STDOUT, check=True)
    with (work / "guest.log").open("w") as log:
        result = subprocess.run([str(a.qemu), "-cpu", "arm1136", "-L", str(a.sysroot / "usr/arm-linux-gnueabi"), str(binary)],
            stdout=log, stderr=subprocess.STDOUT, timeout=30)
    print(name, "exit", result.returncode, (work / "guest.log").read_text(), end="", flush=True)
    summary = re.search(r"RESULT cases=(\d+) corruptions=(\d+) wrongPadding=(\d+) postRegionPools=(\d+)", (work / "guest.log").read_text())
    if result.returncode != expected or not summary or int(summary[1]) != 3300 or (name == "before" and not int(summary[2])):
        raise SystemExit("unexpected regression result: " + name)
    records.append({"case": name, "source": relative, "sourceSHA256": hashlib.sha256(source.encode()).hexdigest(),
        "methodSHA256": hashlib.sha256(method.encode()).hexdigest(), "guestExit": result.returncode,
        "guestLogSHA256": hashlib.sha256((work / "guest.log").read_bytes()).hexdigest()})
(a.work / "result.json").write_text(json.dumps(records, indent=2) + "\n")
