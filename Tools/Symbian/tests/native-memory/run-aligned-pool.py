#!/usr/bin/env python3
"""Exercise the actual AlignedPool class and RChunk bridge with kernel shims."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[3]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--legacy-source", type=Path,
                    help="reproduce the old fixed-pool boundary from this saved FastMalloc.cpp")
args = parser.parse_args()
source = (args.legacy_source or root / "Source/WTF/wtf/FastMalloc.cpp").read_text()
begin = source.index("class AlignedPool {")
end = source.index("// Never destroyed:", begin)
with tempfile.TemporaryDirectory(prefix="symbian-aligned-pool-") as scratch:
    build = Path(scratch)
    # Extract from the production TU, not a second implementation in the test.
    (build / "AlignedPool.inc").write_text(source[begin:end])
    executable = build / "aligned-pool"
    subprocess.run(shlex.split(os.environ.get("CXX", "c++")) + [
        "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pthread",
        f"-I{here / 'include'}", f"-I{root / 'Source/WTF/wtf/symbian'}", f"-I{build}",
        str(here / ("aligned-pool-limit.cpp" if args.legacy_source else "aligned-pool.cpp")),
        str(root / "Source/WTF/wtf/symbian/SymbianSystem.cpp"),
        "-o", str(executable),
    ], check=True)
    subprocess.run([executable], check=True)
    if args.legacy_source:
        subprocess.run([executable, "reduced"], check=True)
