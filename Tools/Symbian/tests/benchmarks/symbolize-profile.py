#!/usr/bin/env python3
"""Names the samples of the browser's "profile" setting.

    symbolize-profile.py log.txt browser.elf [--top N]

log.txt holds "profile:" lines; browser.elf is the CI "elf" artifact of the
same build. Prints the share of samples per group (interpreter, JIT code,
system libraries, ...) and per function.
"""

import argparse
import bisect
import collections
import re
import shutil
import subprocess
import sys


def tool(*names):
    for name in names:
        if shutil.which(name):
            return name
    sys.exit("need one of: " + ", ".join(names))


def load_symbols(elf):
    nm = tool("llvm-nm", "arm-none-eabi-nm", "nm")
    output = subprocess.run([nm, "-n", "-C", "--defined-only", elf], check=True,
                            capture_output=True, text=True).stdout
    symbols = []
    for line in output.splitlines():
        parts = line.split(" ", 2)
        if len(parts) == 3 and parts[1] in "tTwW":
            name = parts[2]
            if name.startswith("$"):
                continue
            symbols.append((int(parts[0], 16) & ~1, name))
    symbols.sort()
    return symbols


def load_base(elf):
    readelf = tool("llvm-readelf", "arm-none-eabi-readelf", "readelf")
    output = subprocess.run([readelf, "-lW", elf], check=True, capture_output=True, text=True).stdout
    for line in output.splitlines():
        fields = line.split()
        if fields and fields[0] == "LOAD":
            return int(fields[2], 16)
    sys.exit("no LOAD segment in " + elf)


def group_of(name):
    if name.startswith(("llint_", "op_", "wasm_")) or "LLInt" in name or name.startswith("vmEntry"):
        return "interpreter (LLInt and its slow paths)"
    if name.startswith("JSC::DFG::") or name.startswith("JSC::B3"):
        return "DFG compiler/runtime"
    if "operation" in name and name.startswith("JSC::"):
        return "JIT operations (C++ called from JIT code)"
    if name.startswith(("JSC::Heap", "JSC::Slot", "JSC::Marked", "JSC::BlockDirectory", "JSC::LocalAllocator",
                        "JSC::CompleteSubspace", "JSC::IsoSubspace", "JSC::FreeList")):
        return "GC and allocation"
    if name.startswith(("JSC::Parser", "JSC::Lexer", "JSC::ASTBuilder", "JSC::SyntaxChecker", "JSC::BytecodeGenerator",
                        "JSC::Scope")) or "Node::emit" in name:
        return "JS parser and bytecode generator"
    if name.startswith(("JSC::Yarr", "JSC::RegExp")):
        return "regular expressions"
    if name.startswith("JSC::"):
        return "JavaScriptCore runtime"
    if name.startswith("WTF::") or name.startswith(("mi_", "_mi_", "malloc", "free", "memcpy", "memset", "__aeabi")):
        return "WTF, malloc and libc"
    if name.startswith("WebCore::JS") or name.startswith("WebCore::js"):
        return "DOM bindings"
    if name.startswith("WebCore::"):
        return "WebCore"
    return "other in image"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("log")
    parser.add_argument("elf")
    parser.add_argument("--top", type=int, default=60)
    arguments = parser.parse_args()

    samples = collections.Counter()
    header = None
    for line in open(arguments.log, errors="replace"):
        position = line.find("profile:")
        if position < 0:
            continue
        text = line[position + len("profile:"):]
        if " samples busy=" in text:
            header = text.strip()
        for kind, offset, count in re.findall(r" ([ijfc]):([0-9a-f]+):(\d+)", text):
            samples[(kind, int(offset, 16))] += int(count)
    if not samples:
        sys.exit("no profile: samples in " + arguments.log)

    symbols = load_symbols(arguments.elf)
    addresses = [address for address, _ in symbols]
    base = load_base(arguments.elf)

    def name_at(offset):
        # A bucket is named by the function at its middle.
        index = bisect.bisect_right(addresses, base + offset + 16) - 1
        return symbols[index][1] if index >= 0 else "?"

    functions = collections.Counter()
    groups = collections.Counter()
    callers = collections.Counter()
    foreign = collections.Counter()
    total = 0
    for (kind, offset), count in samples.items():
        if kind == "c":
            callers[name_at(offset)] += count
            continue
        total += count
        if kind == "i":
            name = name_at(offset)
            functions[name] += count
            groups[group_of(name)] += count
        elif kind == "j":
            functions["<JIT code>"] += count
            groups["JIT code"] += count
        else:
            foreign[offset] += count
            groups["system libraries and kernel calls"] += count

    if header:
        print(header)
    print("%d samples named (single-sample buckets are not logged)\n" % total)
    print("By group:")
    for name, count in groups.most_common():
        print("  %5.1f%%  %6d  %s" % (100.0 * count / total, count, name))
    print("\nBy function:")
    for name, count in functions.most_common(arguments.top):
        print("  %5.1f%%  %6d  %s" % (100.0 * count / total, count, name[:140]))
    if callers:
        print("\nCallers into system libraries:")
        for name, count in callers.most_common(20):
            print("  %5.1f%%  %6d  %s" % (100.0 * count / total, count, name[:140]))
    if foreign:
        print("\nSystem addresses:")
        for offset, count in foreign.most_common(10):
            print("  %5.1f%%  %6d  0x%08x" % (100.0 * count / total, count, offset))


if __name__ == "__main__":
    main()
