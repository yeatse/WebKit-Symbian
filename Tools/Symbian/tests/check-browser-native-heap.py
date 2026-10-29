#!/usr/bin/env python3
"""Compile browser ARM objects and verify native-class allocation/deletion relocations."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess

CASES = {
    "BrowserApp.cpp": {
        "CUrlBar": "CBrowserAppUi10ConstructLEv",
        "CUrlSuggestions": "CBrowserAppUi10ConstructLEv",
        "CBrowserAppUi": "CBrowserDocument12CreateAppUiLEv",
        "CBrowserDocument": "CBrowserApplication15CreateDocumentLEv",
        "CBrowserApplication": "NewApplicationEv",
    },
    "WebContainer.cpp": {"CWebContainer": "CWebContainer4NewL"},
    "RunLoopDriver.cpp": {"CRunLoopDriver": "CRunLoopDriver4NewL", "CWorkTimer": "CRunLoopDriver10ConstructLEv"},
    "PageList.cpp": {"CPageList": "CPageList4NewL"},
    "NetworkConnection.cpp": {"CNetworkConnection": "CNetworkConnection4NewL"},
}
FREE = "_ZN4User4FreeEPv"
ALLOCATIONS = {"_ZN4User6AllocZEi", "_ZN4User7AllocZLEi"}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def relocations(text):
    result = {}
    current = None
    for line in text.splitlines():
        section = re.match(r"Relocation section '([^']+)'", line)
        if section:
            current = section[1]
            result[current] = []
        elif current:
            target = re.match(r"\s*[0-9a-f]+\s+[0-9a-f]+\s+R_ARM_\S+\s+[0-9a-f]+\s+(\S+)", line)
            if target:
                result[current].append(target[1])
    return {name: targets for name, targets in result.items() if name.startswith(".rel.text.")}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compile-commands", required=True, type=Path, help="Configured browser compile_commands.json")
    parser.add_argument("--output", required=True, type=Path, help="New directory for independent objects, logs and summary")
    parser.add_argument("--readelf", default="readelf")
    args = parser.parse_args()
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    entries = json.loads(args.compile_commands.read_text())
    browser = Path(next(item["file"] for item in entries if item["file"].endswith("/browser/BrowserApp.cpp"))).parent
    result = {"status": "FAIL", "kind": "ARM object relocation checks; no guest execution",
              "compile_commands_sha256": digest(args.compile_commands), "objects": [], "classes": [],
              "headers": [{"name": name, "sha256": digest(browser / name)} for name in
                          ("NativeHeapDelete.h", "WebContainer.h", "RunLoopDriver.h", "PageList.h", "NetworkConnection.h")]}
    try:
        for source, cases in CASES.items():
            entry = next(item for item in entries if item["file"].endswith("/browser/" + source))
            command = entry.get("arguments") or shlex.split(entry["command"])
            command = list(command)
            obj = args.output / (source + ".o")
            command[command.index("-o") + 1] = str(obj)
            completed = subprocess.run(command, cwd=entry["directory"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            (args.output / (source + ".compile.log")).write_bytes(completed.stdout)
            if completed.returncode:
                raise RuntimeError(source + " ARM compilation failed")
            header = subprocess.check_output([args.readelf, "-h", str(obj)], text=True)
            if not re.search(r"Machine:\s+ARM\s*$", header, re.M):
                raise RuntimeError(source + " is not an ARM object")
            text = subprocess.check_output([args.readelf, "-rW", str(obj)], text=True)
            (args.output / (source + ".relocations.txt")).write_text(text)
            sections = relocations(text)
            result["objects"].append({"source": source, "source_sha256": digest(Path(entry["file"])), "object_sha256": digest(obj)})
            for name, allocator in cases.items():
                destructors = [(section, targets) for section, targets in sections.items() if name + "D0Ev" in section]
                if len(destructors) != 1:
                    raise RuntimeError(name + " does not have one deleting-destructor section")
                section, targets = destructors[0]
                if FREE not in targets or any(target.startswith("_Zdl") for target in targets):
                    raise RuntimeError(name + " deleting destructor does not exclusively release through User::Free")
                allocation = [(section, sorted(set(targets) & ALLOCATIONS)) for section, targets in sections.items()
                              if allocator in section and set(targets) & ALLOCATIONS]
                if len(allocation) != 1:
                    raise RuntimeError(name + " native allocation site was not found")
                result["classes"].append({"class": name, "status": "PASS", "deleting_destructor": section,
                                          "deallocation": FREE, "allocation_site": allocation[0][0], "allocations": allocation[0][1]})
        result["status"] = "PASS"
    except Exception as error:
        result["error"] = str(error)
    finally:
        (args.output / "summary.json").write_text(json.dumps(result, indent=2) + "\n")
    print(f"{result['status']}: {len(result['classes'])}/10 native classes; {args.output}")
    if result["status"] != "PASS":
        print(result["error"])
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
