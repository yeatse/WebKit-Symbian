#!/usr/bin/env python3
"""Compile the changed IDB TUs against an existing Symbian WebCore build."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compdb", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--checkout", type=Path, default=Path(__file__).resolve().parents[4])
    parser.add_argument("--sdk", type=Path, default=Path(os.environ["SYMBIAN_SDK_PREFIX"]) if "SYMBIAN_SDK_PREFIX" in os.environ else None)
    args = parser.parse_args()
    if args.sdk is None: parser.error("--sdk or SYMBIAN_SDK_PREFIX is required")
    checkout = args.checkout.resolve(); work = args.work.resolve(); work.mkdir(parents=True, exist_ok=True)
    entries = json.loads(args.compdb.read_text())
    entry = next(item for item in entries if "/Source/WebCore/" in item["file"])
    previous = entry["file"].split("/Source/WebCore/")[0]
    build = Path(entry["directory"])
    includes = work / "include"
    (includes / "WebCore").mkdir(parents=True, exist_ok=True)
    for path in (build / "WebCore/PrivateHeaders/WebCore").glob("*.h"):
        target = includes / "WebCore" / path.name
        if target.is_symlink(): target.unlink()
        target.symlink_to(str(path.resolve()).replace(previous, str(checkout)))
    (includes / "wtf").mkdir(exist_ok=True)
    target = includes / "wtf/FileSystem.h"
    if target.is_symlink(): target.unlink()
    target.symlink_to(checkout / "Source/WTF/wtf/FileSystem.h")
    command = entry.get("arguments") or shlex.split(entry["command"])
    base = []; skip = False
    for value in command:
        if skip: skip = False; continue
        if value in ("-o", "-MF", "-MT", "-MQ"): skip = True; continue
        if value in ("-c", "-MD", "-MMD") or value == entry["file"]: continue
        base.append(value.replace(previous, str(checkout)))
    base.insert(1, "-I" + str(includes))
    environment = os.environ.copy(); environment["SYMBIAN_SDK_PREFIX"] = str(args.sdk.resolve())
    sources = ["Modules/indexeddb/server/" + name + ".cpp" for name in ("IDBServer", "UniqueIDBDatabase", "SQLiteIDBBackingStore", "SQLiteIDBTransaction")]
    sources.append("platform/network/BlobRegistryImpl.cpp")
    results = []
    for name in sources:
        path = checkout / "Source/WebCore" / name
        command = base + ["-fsyntax-only", str(path)]
        label = path.stem
        (work / (label + ".command.json")).write_text(json.dumps(command, indent=2) + "\n")
        with (work / (label + ".log")).open("w") as stream:
            result = subprocess.run(command, cwd=build, env=environment, stdout=stream, stderr=subprocess.STDOUT)
        results.append({"source": "Source/WebCore/" + name, "sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "exit": result.returncode})
        print(label, result.returncode, flush=True)
    (work / "result.json").write_text(json.dumps(results, indent=2) + "\n")
    return any(item["exit"] for item in results)


if __name__ == "__main__": raise SystemExit(main())
