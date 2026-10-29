#!/usr/bin/env python3
"""Ordinary metadata regression using the real registration function and types."""

import argparse
import difflib
import hashlib
import json
import re
import shlex
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--eka-root", type=Path, required=True)
parser.add_argument("--work", type=Path, required=True)
args = parser.parse_args()
WORK = args.work.resolve()
WORK.mkdir(parents=True, exist_ok=True)
EKA = args.eka_root.resolve()
REL = Path("src/emu/package/src/sis_script_interpreter.cpp")
SOURCE = EKA / REL
original = SOURCE.read_text()
old = "desc.uncompressed_length = file_des->len;"
new = "desc.uncompressed_length = file_des->uncompressed_len;"
if original.count(new) == 1:
    fixed = original
    original = original.replace(new, old)
elif original.count(old) == 1:
    fixed = original.replace(old, new)
else:
    raise RuntimeError("unexpected current registration source")
output = WORK / "overlay" / REL
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(fixed)
patch = (
    "Subject: [PATCH] Register the uncompressed installed SIS payload length\n\n"
    + "".join(
        difflib.unified_diff(
            original.splitlines(True),
            fixed.splitlines(True),
            fromfile="a/" + str(REL),
            tofile="b/" + str(REL),
        )
    )
)
(WORK / "0026-Register-uncompressed-SIS-payload-length.patch").write_text(patch)


def function(text, name):
    start = text.index("        static void " + name)
    brace = text.index("{", start)
    level = 1
    end = brace + 1
    while level:
        if text[end] == "{":
            level += 1
        elif text[end] == "}":
            level -= 1
        end += 1
    return text[start:end]


cases = json.loads((Path(__file__).resolve().parent / "cases.json").read_text())
(WORK / "cases.json").write_text(json.dumps(cases, indent=2) + "\n")
rows = ",\n".join(
    "{"
    + json.dumps(c["name"])
    + ","
    + str(c["stored"])
    + "ULL,"
    + str(c["expanded"])
    + "ULL}"
    for c in cases
)
head = """#include <loader/sis_fields.h>
#include <package/registry.h>
#include <cwctype>
#include <cstring>
#include <iostream>
#define LOG_WARN(...) ((void)0)
drive_number char16_to_drive(char16_t c) { return static_cast<drive_number>(std::towlower(c)-u'a'); }
namespace eka2l1 { class io_system; namespace loader {
// Exe SID I/O is outside this metadata-only test; its return is a named stub.
static epoc::uid read_executable_sid(io_system *, const std::u16string &) { return 0x12345678; }
"""
tail = """
}}
struct Case { const char *name; std::uint64_t stored, expanded; };
int main() {
    const Case cases[] = { CASES };
    unsigned failures = 0;
    for (const auto &test : cases) {
        eka2l1::loader::sis_file_des input{};
        input.len=test.stored; input.uncompressed_len=test.expanded;
        input.idx=17;input.op=static_cast<eka2l1::loader::ss_op>(1);input.op_op=0;
        input.mime_type.unicode_string=u"application/octet-stream";
        input.hash.hash_method=static_cast<decltype(input.hash.hash_method)>(1);
        input.hash.hash_data.raw_data={1,2,3,4,5};input.caps.raw_data={1};
        eka2l1::package::object parent{};parent.drives=0;
        eka2l1::loader::register_file_description(nullptr,&input,u"E:\\\\Data\\\\sample.bin",parent);
        const auto &desc=parent.file_descriptions.at(0);
        bool ok=desc.uncompressed_length==test.expanded && desc.index==17 && desc.operation==1
            && desc.operation_options==0 && desc.mime_type==input.mime_type.unicode_string
            && desc.hash.algorithm==1 && desc.hash.data==std::string("\\1\\2\\3\\4\\5",5)
            && desc.sid==0x12345678 && desc.target==u"E:\\\\Data\\\\sample.bin" && parent.drives==(1u<<4);
        if (!ok) { ++failures; std::cout<<"FAIL "<<test.name<<" length="<<desc.uncompressed_length<<" expected="<<test.expanded<<'\\n'; }
    }
    std::cout<<"CASES "<<std::size(cases)<<" FAILURES "<<failures<<'\\n';
    return failures?1:0;
}
""".replace("CASES", rows, 1)
includes = [
    EKA / "src/emu/loader/include",
    EKA / "src/emu/common/include",
    EKA / "src/emu/utils/include",
    EKA / "src/emu/package/include",
    EKA / "build/src/emu/common/include",
    EKA / "src/external/fmt/include",
]
results = {}
for name, text in [("before", original), ("after", fixed)]:
    cpp = WORK / (name + ".cpp")
    cpp.write_text(head + function(text, "register_file_description") + tail)
    command = [
        "/usr/bin/g++",
        "-std=c++20",
        "-O0",
        *[f"-I{p}" for p in includes],
        str(cpp),
        "-o",
        str(WORK / name),
    ]
    build = subprocess.run(
        command,
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    (WORK / (name + "-build.log")).write_text(build.stdout)
    if build.returncode:
        raise RuntimeError(name + " harness build failed: " + build.stdout[-2000:])
    run = subprocess.run(
        [str(WORK / name)],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    (WORK / (name + ".log")).write_text(run.stdout)
    results[name] = {
        "command": command,
        "exit_code": run.returncode,
        "output": run.stdout,
        "extracted_actual_function_sha256": hashlib.sha256(
            function(text, "register_file_description").encode()
        ).hexdigest(),
    }
if results["before"]["exit_code"] != 1 or results["after"]["exit_code"] != 0:
    raise RuntimeError("unexpected before/after result")
# Syntax-check the complete changed production TU with its actual configured flags.
ninja = (EKA / "build/build.ninja").read_text()
anchor = (
    "build src/emu/package/CMakeFiles/epocpkg.dir/src/sis_script_interpreter.cpp.o:"
)
block = ninja[ninja.index(anchor) :].split("\n\n", 1)[0]
flags = []
for key in ("DEFINES", "FLAGS", "INCLUDES"):
    flags += shlex.split(
        re.search(r"^  " + key + r" = (.*)$", block, re.MULTILINE).group(1)
    )
command = ["/usr/bin/g++", *flags, "-fsyntax-only", str(output)]
check = subprocess.run(
    command, check=False, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT
)
(WORK / "production-syntax.log").write_text(check.stdout)
if check.returncode:
    raise RuntimeError("production TU syntax failed")
results.update(
    status="PASS",
    case_count=len(cases),
    scope="Metadata registration only; no emulator launch or installation; current source/binary unchanged",
    production_syntax={"command": command, "exit_code": check.returncode},
    source={
        "path": str(SOURCE),
        "before_sha256": hashlib.sha256(original.encode()).hexdigest(),
        "after_sha256": hashlib.sha256(fixed.encode()).hexdigest(),
    },
    patch_sha256=hashlib.sha256(patch.encode()).hexdigest(),
)
(WORK / "result.json").write_text(json.dumps(results, indent=2) + "\n")
print(json.dumps({k: results[k] for k in ("status", "case_count", "patch_sha256")}))
