#!/usr/bin/env python3
"""Recover fixed upstream EC known-answer vectors without modifying the checkout."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--work", type=Path, required=True)
parser.add_argument("--checkout", type=Path)
args = parser.parse_args()
args.checkout = args.checkout or Path(__file__).resolve().parents[4]
args.work.mkdir(parents=True, exist_ok=True)
base = "4fb33923"
root = "LayoutTests/imported/w3c/web-platform-tests/WebCryptoAPI/"
manifest = []

def load(suffix):
    path = root + suffix
    source = subprocess.check_output(["git", "show", base + ":" + path], cwd=args.checkout)
    manifest.append({"git_ref": base, "path": path,
                     "git_blob": subprocess.check_output(["git", "rev-parse", base + ":" + path], cwd=args.checkout, text=True).strip(),
                     "sha256": hashlib.sha256(source).hexdigest()})
    return source.decode()

replacer = "(key, value) => ArrayBuffer.isView(value) ? Array.from(value) : value"
source = load("sign_verify/ecdsa_vectors.js")
rows = json.loads(subprocess.check_output(["node", "-e", source + "\nconsole.log(JSON.stringify(getTestVectors(), " + replacer + "));"], text=True))
ecdsa = [{"name": row["name"], "curve": row["namedCurve"], "hash": row["hashName"],
          "spki": bytes(row["publicKeyBuffer"]).hex(), "pkcs8": bytes(row["privateKeyBuffer"]).hex(),
          "data": bytes(row["plaintext"]).hex(), "signature": bytes(row["signature"]).hex()}
         for row in rows]

source = load("derive_bits_keys/ecdh_bits.js")
marker = "    return importKeys(pkcs8, spki, sizes)"
if source.count(marker) != 1:
    raise SystemExit("fixed ECDH fixture boundary changed")
source = source.split(marker)[0] + "    return {pkcs8, spki, sizes, derivations};\n}\n"
source = "const self = {crypto: {subtle: {}}};\n" + source
source += "console.log(JSON.stringify(define_tests(), " + replacer + "));"
rows = json.loads(subprocess.check_output(["node", "-e", source], text=True))
ecdh = [{"name": curve + " ECDH WPT known secret", "curve": curve,
         "spki": bytes(rows["spki"][curve]).hex(), "pkcs8": bytes(rows["pkcs8"][curve]).hex(),
         "secret": bytes(rows["derivations"][curve]).hex()}
        for curve in ("P-256", "P-384", "P-521")]

(args.work / "vectors.json").write_text(json.dumps({"ecdsa": ecdsa, "ecdh": ecdh}, indent=2) + "\n")
(args.work / "vector-sources.json").write_text(json.dumps(manifest, indent=2) + "\n")
header = ["// Generated from fixed WPT files by prepare-vectors.py.",
          "struct ECDSAVector { const char* name; const char* curve; const char* hash; const char* spki; const char* pkcs8; const char* data; const char* signature; };",
          "static const ECDSAVector ecdsaVectors[] = {"]
for row in ecdsa:
    header.append("    { " + ", ".join(json.dumps(row[key]) for key in ("name", "curve", "hash", "spki", "pkcs8", "data", "signature")) + " },")
header += ["};", "struct ECDHVector { const char* name; const char* curve; const char* spki; const char* pkcs8; const char* secret; };", "static const ECDHVector ecdhVectors[] = {"]
for row in ecdh:
    header.append("    { " + ", ".join(json.dumps(row[key]) for key in ("name", "curve", "spki", "pkcs8", "secret")) + " },")
header += ["};"]
(args.work / "vectors.h").write_text("\n".join(header) + "\n")
print("Prepared", len(ecdsa), "ECDSA and", len(ecdh), "ECDH independent known-answer vectors")
