#!/usr/bin/env python3
"""Compile and run the production EC helpers against a native mbedTLS archive."""
import argparse
import hashlib
import json
import os
import shlex
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--work", type=Path, required=True)
parser.add_argument("--checkout", type=Path)
parser.add_argument("--mbedtls-source", type=Path, required=True)
parser.add_argument("--mbedcrypto", type=Path, required=True)
parser.add_argument("--abseil-include", type=Path, required=True)
parser.add_argument("--cxx", default=os.environ.get("CXX", "c++"))
parser.add_argument("--cxx-flag", action="append", default=[])
parser.add_argument("--runner", default="", help="Command prefix, parsed with shlex; no shell is invoked")
args = parser.parse_args()
args.checkout = args.checkout or Path(__file__).resolve().parents[4]
runner = shlex.split(args.runner)
here = Path(__file__).resolve().parent
args.work.mkdir(parents=True, exist_ok=True)
subprocess.run(["python3", str(here / "prepare-vectors.py"), "--work", str(args.work), "--checkout", str(args.checkout)], check=True)
helpers = args.checkout / "Source/WebCore/crypto/mbedtls"
command = [args.cxx, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", "-DMBEDTLS_STATIC",
           '-DMBEDTLS_USER_CONFIG_FILE="symbian_mbedtls/config.h"', "-I" + str(helpers),
           "-I" + str(args.work), "-I" + str(args.mbedtls_source / "include"),
           "-I" + str(args.abseil_include), *args.cxx_flag, str(here / "host.cpp"),
           str(args.mbedcrypto), "-pthread", "-o", str(args.work / "test")]
subprocess.run(command, check=True)
with (args.work / "test.log").open("w") as stream:
    result = subprocess.run([*runner, str(args.work / "test")], stdout=stream, stderr=subprocess.STDOUT, timeout=120)
manifest = {"exit": result.returncode, "scope": "EC key encoding and operations", "command": command,
            "runner": runner, "archive_sha256": hashlib.sha256(args.mbedcrypto.read_bytes()).hexdigest(),
            "sources": {name: hashlib.sha256((helpers / name).read_bytes()).hexdigest() for name in ("MbedTLSECKey.h", "MbedTLSECDER.h", "MbedTLSECOperations.h", "MbedTLSRSADER.h", "MbedTLSRSAKey.h")},
            "test_sha256": hashlib.sha256((here / "host.cpp").read_bytes()).hexdigest(),
            "vector_sources_sha256": hashlib.sha256((args.work / "vector-sources.json").read_bytes()).hexdigest(),
            "log_sha256": hashlib.sha256((args.work / "test.log").read_bytes()).hexdigest()}
(args.work / "result.json").write_text(json.dumps(manifest, indent=2) + "\n")
print((args.work / "test.log").read_text())
raise SystemExit(result.returncode)
