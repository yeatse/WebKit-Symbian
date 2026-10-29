#!/usr/bin/env python3
"""Recover fixed upstream RSA vectors without changing the sparse checkout."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--work", type=Path, required=True)
parser.add_argument("--checkout", type=Path, default=Path(__file__).resolve().parents[4])
args = parser.parse_args()
args.work.mkdir(parents=True, exist_ok=True)
base = "4fb33923"
paths = ["encrypt_decrypt/rsa_vectors.js", "sign_verify/rsa_pkcs_vectors.js", "sign_verify/rsa_pss_vectors.js"]
manifest = []
vectors = []
for suffix in paths:
    path = "LayoutTests/imported/w3c/web-platform-tests/WebCryptoAPI/" + suffix
    source = subprocess.check_output(["git", "show", base + ":" + path], cwd=args.checkout)
    manifest.append({"git_ref": base, "path": path, "git_blob": subprocess.check_output(["git", "rev-parse", base + ":" + path], cwd=args.checkout, text=True).strip(), "sha256": hashlib.sha256(source).hexdigest()})
    javascript = source.decode() + "\nconsole.log(JSON.stringify(getTestVectors(), (key, value) => ArrayBuffer.isView(value) ? Array.from(value) : value));"
    rows = json.loads(subprocess.check_output(["node", "-e", javascript], text=True))
    if isinstance(rows, dict):
        rows = rows["passing"]
    for row in rows:
        vectors.append({"name": row["name"], "algorithm": row["algorithm"], "hash": row["hash"], "spki": bytes(row["publicKeyBuffer"]).hex(), "pkcs8": bytes(row["privateKeyBuffer"]).hex(), "data": bytes(row["plaintext"]).hex(), "result": bytes(row.get("ciphertext", row.get("signature"))).hex()})
(args.work / "vectors.json").write_text(json.dumps(vectors, indent=2) + "\n")
(args.work / "vector-sources.json").write_text(json.dumps(manifest, indent=2) + "\n")
header = ["// Generated from fixed WPT files by prepare-vectors.py.", "struct TestVector { const char* name; const char* algorithm; const char* hash; int salt; const char* spki; const char* pkcs8; const char* data; const char* result; const char* label; };", "static const TestVector vectors[] = {"]
for row in vectors:
    values = [row["name"], row["algorithm"]["name"], row["hash"], row["algorithm"].get("saltLength", -1), row["spki"], row["pkcs8"], row["data"], row["result"], bytes(row["algorithm"].get("label", [])).hex()]
    header.append("    { " + ", ".join(json.dumps(value) for value in values) + " },")
header.append("};")
(args.work / "vectors.h").write_text("\n".join(header) + "\n")
print("Prepared", len(vectors), "known-answer vectors from", len(manifest), "fixed upstream files")
