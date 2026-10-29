#!/usr/bin/env python3
"""Exercise production RSA helpers against a native build of SDK mbedTLS 3.4.1."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--work", type=Path, required=True)
parser.add_argument("--mbedtls-source", type=Path, required=True)
parser.add_argument("--mbedcrypto", type=Path, required=True)
parser.add_argument("--abseil-include", type=Path, required=True)
args = parser.parse_args()
here = Path(__file__).resolve().parent
checkout = here.parents[3]
args.work.mkdir(parents=True, exist_ok=True)
subprocess.run(["python3", str(here / "prepare-vectors.py"), "--work", str(args.work)], check=True)
command = [os.environ.get("CXX", "c++"), "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", "-DMBEDTLS_STATIC", '-DMBEDTLS_USER_CONFIG_FILE="symbian_mbedtls/config.h"', "-I" + str(checkout / "Source/WebCore/crypto/mbedtls"), "-I" + str(args.work), "-I" + str(args.mbedtls_source / "include"), "-I" + str(args.abseil_include), str(here / "host.cpp"), str(args.mbedcrypto), "-pthread", "-o", str(args.work / "test")]
subprocess.run(command, check=True)
with (args.work / "test.log").open("w") as stream:
    result = subprocess.run([str(args.work / "test")], stdout=stream, stderr=subprocess.STDOUT)
manifest = {"exit": result.returncode, "scope": "native host production helpers; not E32/browser validation", "archive_sha256": hashlib.sha256(args.mbedcrypto.read_bytes()).hexdigest(), "sources": {name: hashlib.sha256((checkout / "Source/WebCore/crypto/mbedtls" / name).read_bytes()).hexdigest() for name in ("MbedTLSRSAKey.h", "MbedTLSRSADER.h", "MbedTLSRSAOperations.h")}, "log_sha256": hashlib.sha256((args.work / "test.log").read_bytes()).hexdigest()}
(args.work / "result.json").write_text(json.dumps(manifest, indent=2) + "\n")
print((args.work / "test.log").read_text())
raise SystemExit(result.returncode)
