#!/usr/bin/env python3
"""Build real ICU70 IDNA checks against stubdata and filtered data packages."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile


def run(command):
    result = subprocess.run([str(arg) for arg in command], text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError(f"{shlex.join(map(str, command))}\n{result.stdout}{result.stderr}")
    return result.stdout


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--icu-root", type=Path, required=True,
                        help="build-icu.sh root containing src/icu4c/source and host")
    parser.add_argument("--existing-data", type=Path, help="also check an exact deployed/release data package")
    parser.add_argument("--expect-existing-missing", action="store_true",
                        help="reproduce the old package's exact U_FILE_ACCESS_ERROR")
    parser.add_argument("--output", type=Path, help="write portable JSON evidence")
    args = parser.parse_args()
    if args.expect_existing_missing and not args.existing_data:
        parser.error("--expect-existing-missing requires --existing-data")
    root = args.icu_root.resolve()
    source = root / "src/icu4c/source"
    host = root / "host"
    full = host / "data/out/tmp/icudt70l.dat"
    icupkg = host / "bin/icupkg"
    tests = Path(__file__).resolve().parent
    filter_script = tests.parents[1] / "scripts/icu-filter.py"
    report = {
        "icu_version": "70.1",
        "full_data_sha256": sha256(full),
        "filter_sha256": sha256(filter_script),
        "probe_sha256": sha256(tests / "probe.cpp"),
        "isolation": "ICU host libicuuc.a plus freshly compiled empty stubdata.cpp; UDATA_NO_FILES; explicit udata_setCommonData",
        "checks": [],
    }
    with tempfile.TemporaryDirectory(prefix="icu-idna-") as scratch:
        work = Path(scratch)
        probe = work / "probe"
        run(shlex.split(os.environ.get("CXX", "c++")) + [
            "-std=c++17", "-DU_STATIC_IMPLEMENTATION", f"-I{source / 'common'}",
            tests / "probe.cpp", source / "stubdata/stubdata.cpp",
            host / "lib/libicuuc.a", "-lpthread", "-ldl", "-o", probe,
        ])

        def check(label, data, missing=False):
            output = run([probe, data] + (["--expect-missing"] if missing else []))
            report["checks"].append({"name": label, "data_sha256": sha256(data),
                                     "expect_missing": missing, "result": "PASS", "output": output})
            print(f"PASS {label}")

        check("full package", full)
        if args.existing_data:
            check("existing package", args.existing_data, args.expect_existing_missing)
        items = work / "items.txt"
        items.write_text(run([icupkg, "-l", full]))
        for label, locales in (("all-locales", []), ("en-only", ["en"]), ("zh-only", ["zh"])):
            directory = work / label
            directory.mkdir()
            removed = directory / "remove.txt"
            removed.write_text(run([sys.executable, filter_script, items, *locales]))
            if "uts46.nrm" in removed.read_text().splitlines():
                raise RuntimeError(f"{label}: filter still removes required UTS46 normalization data")
            package = directory / "icudt70l.dat"
            run([icupkg, "-r", removed, full, package])
            if "uts46.nrm" not in run([icupkg, "-l", package]).splitlines():
                raise RuntimeError(f"{label}: packed data lost UTS46")
            check(label, package)
            # Each negative control must fail even if ICU_DATA points to a full
            # package; this detects accidental embedded/system-data fallback.
            broken = directory / "without-uts46.dat"
            run([icupkg, "-r", "uts46.nrm", package, broken])
            check(label + " missing UTS46 negative control", broken, True)
    report["result"] = "PASS"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print("IDNA packaging regression PASS")


if __name__ == "__main__":
    main()
