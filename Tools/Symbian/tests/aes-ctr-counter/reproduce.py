#!/usr/bin/env python3
"""Execute the unmodified AES-CTR counter helper under ARM1136 with size_t=32."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
for option in ("checkout", "qemu-build", "sysroot", "qemu", "work"):
    parser.add_argument("--" + option, type=Path, required=True)
parser.add_argument("--baseline", default="d46c6c5a84b")
args = parser.parse_args()
args.work.mkdir(parents=True, exist_ok=True)
relative = "Source/WebCore/crypto/algorithms/CryptoAlgorithmAESCTR.cpp"
header_relative = relative[:-4] + ".h"
header = (args.checkout / header_relative).read_text()
start = header.index("    class CounterBlockHelper {")
end = header.index("\n    };", start) + len("\n    };")
helper_class = header[start:end]
old = subprocess.check_output(["git", "show", args.baseline + ":" + relative], cwd=args.checkout).decode()
new = (args.checkout / relative).read_text()

# Expected values come from arbitrary-precision integer arithmetic in the
# runner. The C++ counter implementation is extracted verbatim from production.
cases = []
pattern = int("a57b31c8e4920fd6a57b31c8e4920fd6", 16)
for bits in (1, 7, 32, 33, 64, 128):
    mask = (1 << bits) - 1
    values = {0, 1, mask, max(0, mask - 1), mask // 2}
    if bits >= 32:
        boundary = (1 << bits) - (1 << 32)
        values.update({boundary, boundary + 1, max(0, boundary - 1)})
    for low in sorted(values):
        counter = (pattern & ~mask) | low
        capacity = min((1 << bits) - low, (1 << 32) - 1)
        wrapped = counter & ~mask
        cases.append((bits, f"{counter:032x}", capacity, f"{wrapped:032x}"))

test_source = r'''
struct Case { unsigned bits; const char* input; uint32_t capacity; const char* wrapped; };
static Vector<uint8_t> fromHex(const char* input)
{
    Vector<uint8_t> result(16);
    for (unsigned i = 0; i < 16; ++i) {
        unsigned byte = 0;
        sscanf(input + i * 2, "%2x", &byte);
        result[i] = byte;
    }
    return result;
}
int main()
{
    static_assert(sizeof(size_t) == 4);
    Case cases[] = { CASES };
    unsigned failures = 0;
    for (const auto& item : cases) {
        auto input = fromHex(item.input);
        WebCore::CryptoAlgorithmAESCTR::CounterBlockHelper helper(input, item.bits);
        size_t capacity = helper.countToOverflowSaturating();
        auto wrapped = helper.counterVectorAfterOverflow();
        bool success = capacity == item.capacity && wrapped == fromHex(item.wrapped);
        failures += !success;
        printf("%s bits=%u input=%s capacity=%zu expected=%u\n", success ? "PASS" : "FAIL", item.bits, item.input, capacity, item.capacity);
    }
    printf("RESULT size_t=%zu cases=%zu failures=%u\n", sizeof(size_t), sizeof(cases) / sizeof(cases[0]), failures);
    return !!failures;
}
'''.replace("CASES", ",\n".join(f'{{ {bits}, "{counter}", {capacity}U, "{wrapped}" }}' for bits, counter, capacity, wrapped in cases))

ninja = (args.qemu_build / "build.ninja").read_text()
block = re.search(r"^build .*jsc\.cpp\.o:.*\n((?:  .*\n)+)", ninja, re.M)[1]
fields = dict(re.findall(r"^  (\w+) = (.*)$", block, re.M))
flags = shlex.split(fields["DEFINES"] + " " + fields["FLAGS"] + " " + fields["INCLUDES"])
link = re.search(r"^build bin/jsc:.*\n((?:  .*\n)+)", ninja, re.M)[1]
libraries = shlex.split(dict(re.findall(r"^  (\w+) = (.*)$", link, re.M))["LINK_LIBRARIES"])
compiler = [str(args.sysroot / "usr/bin/arm-linux-gnueabi-g++"), "--sysroot=" + str(args.sysroot)]
results = []
for name, source in (("before", old), ("after", new)):
    directory = args.work / name
    directory.mkdir(exist_ok=True)
    begin = source.index("CryptoAlgorithmAESCTR::CounterBlockHelper::CounterBlockHelper(")
    end = source.rindex("} // namespace WebCore")
    methods = source[begin:end]
    constants = "\n".join(re.findall(r"^static constexpr (?:size_t counterSize|uint64_t allBitsSet) = .*;$", source, re.M))
    generated = '''#include "config.h"
#include <wtf/Vector.h>
#include <wtf/FlipBytes.h>
#include <wtf/StdLibExtras.h>
#include <cstdio>
#include <limits>
using namespace WTF;
namespace WebCore {
class CryptoAlgorithmAESCTR { public:
''' + helper_class + "\n};\nnamespace CryptoAlgorithmAESCTRInternal {\n" + constants + "\n}\n" + methods + "\n}\n" + test_source
    probe = directory / "probe.cpp"
    probe.write_text(generated)
    obj, binary = directory / "probe.o", directory / "probe"
    commands = [compiler + flags + ["-c", str(probe), "-o", str(obj)],
        compiler + ["-march=armv6", "-marm", "-mfpu=vfp", "-mfloat-abi=softfp", "-Wl,--gc-sections", str(obj), "-o", str(binary)] + libraries,
        [str(args.qemu), "-cpu", "arm1136", "-L", str(args.sysroot / "usr/arm-linux-gnueabi"), str(binary)]]
    (directory / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
    with (directory / "build.log").open("w") as log:
        for command in commands[:2]: subprocess.run(command, cwd=args.qemu_build, stdout=log, stderr=subprocess.STDOUT, check=True)
    run = subprocess.run(commands[2], capture_output=True, timeout=30)
    (directory / "guest.log").write_bytes(run.stdout + run.stderr)
    print(name + ":\n" + run.stdout.decode(), end="", flush=True)
    match = re.search(rb"RESULT size_t=4 cases=(\d+) failures=(\d+)", run.stdout)
    if not match or int(match[1]) != len(cases) or (name == "after" and (run.returncode or int(match[2]))) or (name == "before" and (run.returncode != 1 or int(match[2]) != 4)):
        raise SystemExit("unexpected counter regression result: " + name)
    results.append({"version": name, "source": relative, "source_sha256": hashlib.sha256(source.encode()).hexdigest(),
        "header_sha256": hashlib.sha256(header.encode()).hexdigest(), "methods_sha256": hashlib.sha256(methods.encode()).hexdigest(),
        "probe_sha256": hashlib.sha256(generated.encode()).hexdigest(), "guest_exit": run.returncode,
        "cases": len(cases), "failures": int(match[2]), "guest_log_sha256": hashlib.sha256(run.stdout + run.stderr).hexdigest()})
(args.work / "result.json").write_text(json.dumps(results, indent=2) + "\n")
