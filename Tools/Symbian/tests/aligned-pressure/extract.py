#!/usr/bin/env python3
"""Use the production allocator body; never maintain a test implementation."""
import hashlib
import json
from pathlib import Path
import argparse

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("checkout", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--pool-source", type=Path, help="saved historical FastMalloc.cpp for a negative control")
args = parser.parse_args()
checkout, output = args.checkout, args.output
output.mkdir(parents=True, exist_ok=True)
source = args.pool_source or checkout / "Source/WTF/wtf/FastMalloc.cpp"
text = source.read_text()
begin = text.index("class AlignedPool {")
end = text.index("// Never destroyed:", begin)
(output / "AlignedPool.inc").write_text(text[begin:end])
paths = [source, checkout / "Source/WTF/wtf/symbian/SymbianSystem.cpp",
         checkout / "Source/WTF/wtf/symbian/MemoryDiagnostics.h"]
records = [{"path": name, "sha256": hashlib.sha256(p.read_bytes()).hexdigest()}
           for p, name in zip(paths, ["Source/WTF/wtf/FastMalloc.cpp",
               "Source/WTF/wtf/symbian/SymbianSystem.cpp", "Source/WTF/wtf/symbian/MemoryDiagnostics.h"])]
(output / "source-hashes.json").write_text(json.dumps(records, indent=2) + "\n")
(output / "source-hashes.h").write_text(
    '#define ALIGNED_POOL_SOURCE_SHA256 "' + records[0]["sha256"] + '"\n'
    '#define CHUNK_BRIDGE_SOURCE_SHA256 "' + records[1]["sha256"] + '"\n')
