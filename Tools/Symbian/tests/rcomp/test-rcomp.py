#!/usr/bin/env python3
"""Check real rcomp identifier parsing, relocation and error handling."""

import argparse
import hashlib
import json
import re
import resource
import subprocess
import tempfile
from pathlib import Path


def verify(rcomp, reference=None):
    """Compile independent fixtures and compare symbolic/numeric resources."""
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    records = []
    with tempfile.TemporaryDirectory(prefix="rcomp symbols ") as temporary:
        root = Path(temporary)
        prefix = (
            "CHARACTER_SET UTF8\nNAME TSTQ\n"
            "STRUCT ITEM { LLINK target=0; LONG number=0; }\n"
        )

        def compile_case(name, source, compiler=rcomp, stdin=False):
            """Run a bounded compiler invocation and collect its output."""
            path = root / (name + ".rss")
            output = path.with_suffix(".rsc")
            header = path.with_suffix(".rsg")
            path.write_text(source)
            command = [str(compiler), "-u"]
            if not stdin:
                command.append("-s" + str(path))
            command += ["-o" + str(output), "-h" + str(header)]
            with path.open() as stream:
                run = subprocess.run(
                    command,
                    check=False,
                    stdin=stream if stdin else subprocess.DEVNULL,
                    capture_output=True,
                    text=True,
                    timeout=15,
                )
            data = output.read_bytes() if output.exists() else b""
            ids = header.read_text() if header.exists() else ""
            return run, data, ids

        def record(name, ok, **details):
            """Record one assertion; fail without promoting partial output."""
            records.append({"name": name, "passed": bool(ok), **details})
            if not ok:
                raise AssertionError(json.dumps(records[-1]))

        aliases = prefix + (
            "enum { KOne=1, KAlias=KOne };\nRESOURCE ITEM r_value { number=KAlias; }\n"
        )
        run, alias_data, alias_ids = compile_case("enum-alias", aliases)
        record("enum alias without line directives", run.returncode == 0)
        run, literal_data, literal_ids = compile_case(
            "enum-literal",
            aliases.replace("KAlias=KOne", "KAlias=1").replace(
                "number=KAlias", "number=1"
            ),
        )
        record(
            "enum alias equals literal resource",
            run.returncode == 0
            and bool(alias_data)
            and alias_data == literal_data
            and alias_ids == literal_ids,
        )
        for forward in (False, True):
            name = "forward" if forward else "backward"
            source = prefix + (
                "RESOURCE ITEM r_first { target=r_second; number=7; }\n"
                "RESOURCE ITEM r_second { number=8; }\n"
                if forward
                else "RESOURCE ITEM r_first { number=7; }\n"
                "RESOURCE ITEM r_second { target=r_first; number=8; }\n"
            )
            run, symbolic, ids = compile_case(name, source)
            record(name + " symbolic LLINK", run.returncode == 0)
            target = "R_SECOND" if forward else "R_FIRST"
            number = re.search(r"#define\s+" + target + r"\s+(\S+)", ids)
            record(name + " generated resource ID", number is not None)
            numeric = source.replace(
                "target=" + target.lower(), "target=" + number.group(1)
            )
            run, numeric_data, numeric_ids = compile_case(name + "-id", numeric)
            record(
                name + " relocation equals numeric resource",
                run.returncode == 0
                and bool(symbolic)
                and symbolic == numeric_data
                and ids == numeric_ids,
            )
        run, stdin_data, _ = compile_case("stdin", aliases, stdin=True)
        record("seekable stdin input", run.returncode == 0 and stdin_data == alias_data)
        run, line_data, _ = compile_case(
            "line-directive", '# 1 "original.rss"\n' + aliases
        )
        record(
            "line directives preserved", run.returncode == 0 and line_data == alias_data
        )
        run, _, _ = compile_case(
            "missing", prefix + "RESOURCE ITEM r_value { target=r_missing; }\n"
        )
        record(
            "missing link rejected with location",
            run.returncode == 1
            and "Link name not found" in run.stderr
            and "missing.rss(4)" in run.stderr,
        )
        run, _, _ = compile_case(
            "syntax", prefix + "RESOURCE ITEM r_value { number=; }\n"
        )
        record("malformed syntax rejected", run.returncode == 1)
        if reference:
            literal = aliases.replace("KAlias=KOne", "KAlias=1").replace(
                "number=KAlias", "number=1"
            )
            run, old_data, _ = compile_case("stock-literal", literal, reference)
            record(
                "stock literal compatibility",
                run.returncode == 0 and old_data == literal_data,
            )
            run, _, _ = compile_case("stock-alias", aliases, reference)
            record("stock alias crash negative control", run.returncode == -11)
    return {
        "status": "PASS",
        "checks": len(records),
        "results": records,
        "rcomp_sha256": hashlib.sha256(rcomp.read_bytes()).hexdigest(),
        "reference_sha256": hashlib.sha256(reference.read_bytes()).hexdigest()
        if reference
        else None,
    }


def main():
    """Run the regression suite using the selected actual compiler."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rcomp", type=Path, required=True)
    parser.add_argument("--reference", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = verify(
        args.rcomp.resolve(), args.reference.resolve() if args.reference else None
    )
    text = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.write_text(text)
    print(f"rcomp: {result['checks']} checks PASS")


if __name__ == "__main__":
    main()
