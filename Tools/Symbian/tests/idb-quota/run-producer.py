#!/usr/bin/env python3
"""Compile the actual BlobRegistry writer/producer bodies with faultable I/O."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--work", type=Path)
parser.add_argument("--source", type=Path, help="Optional historical BlobRegistryImpl.cpp for a negative control")
args = parser.parse_args()
checkout = Path(__file__).resolve().parents[4]
work = args.work or Path(tempfile.mkdtemp(prefix="idb-blob-producer-"))
work.mkdir(parents=True, exist_ok=True)
source = args.source or checkout / "Source/WebCore/platform/network/BlobRegistryImpl.cpp"
text = source.read_text()
start = text.index("static bool writeFilePathsOrDataBuffersToFile(")
end = text.index("Vector<Ref<BlobDataFileReference>> BlobRegistryImpl::filesInBlob", start)
(work / "producer.inc").write_text(text[start:end])
print("source_sha256=" + hashlib.sha256(source.read_bytes()).hexdigest(), flush=True)
subprocess.run([os.environ.get("CXX", "c++"), "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", "-include", "utility", "-I" + str(work), str(Path(__file__).with_name("blob-producer.cpp")), "-o", str(work / "producer")], check=True)
raise SystemExit(subprocess.run([str(work / "producer"), str(work / "files")]).returncode)
