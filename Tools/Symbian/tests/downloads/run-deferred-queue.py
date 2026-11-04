#!/usr/bin/env python3
"""Run actual queue method bodies with host substitutes, never a guest/build tree."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


def definition(text, marker):
    start = text.index(marker)
    opening = text.index("{", start)
    depth = 1
    position = opening + 1
    while depth:
        depth += (text[position] == "{") - (text[position] == "}")
        position += 1
    if text[position:position + 1] == ";":
        position += 1
    return text[start:position]


def main():
    tests = Path(__file__).resolve().parent
    checkout = tests.parents[3]
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--core-source-root", type=Path, default=checkout)
    args = parser.parse_args()
    args.build.mkdir(parents=True, exist_ok=True)
    source = args.core_source_root / "Source/WebCore/platform/network/curl/ResourceHandleCurl.cpp"
    text = source.read_text()
    methods = ["void ResourceHandle::cancel()", "bool ResourceHandle::acceptsCurlCallback(",
               "void ResourceHandle::dispatchCurlCallback(", "void ResourceHandle::drainCurlCallbacks()",
               "void ResourceHandle::curlDidReceiveDownloadData("]
    bodies = "\n\n".join(definition(text, name) for name in methods) + "\n"
    scope = checkout / "Source/WTF/wtf/CompletionHandler.h"
    scope_body = definition(scope.read_text(), "class CompletionHandlerCallingScope final") + "\n"
    (args.build / "resource-handle-queue-under-test.inc").write_text(bodies)
    (args.build / "completion-scope-under-test.inc").write_text(scope_body)
    identities = {"resource_handle_source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                  "method_bodies_sha256": hashlib.sha256(bodies.encode()).hexdigest(),
                  "completion_scope_source_sha256": hashlib.sha256(scope.read_bytes()).hexdigest(),
                  "scope": "ResourceHandle deferred completion queue"}
    (args.build / "queue-source-identities.json").write_text(json.dumps(identities, indent=2) + "\n")
    command = [os.environ.get("CXX", "c++"), "-std=c++23", "-Wall", "-Wextra", "-Werror",
               "-I", str(args.build), "-I", str(checkout / "Source/WebCore/platform/network/curl"),
               str(tests / "deferred-queue.cpp"), "-o", str(args.build / "queue-tests")]
    subprocess.run(command, check=True)
    subprocess.run([str(args.build / "queue-tests")], check=True)


if __name__ == "__main__":
    main()
