#!/usr/bin/env python3
"""Check the real SDK ABI with curl's configured ARM compiler; no Ninja build."""

import argparse
import hashlib
import json
import os
import shlex
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--curl-build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--ninja", default=os.environ.get("NINJA", "ninja"))
    args = parser.parse_args()
    build = args.curl_build.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    command = (
        subprocess.check_output(
            [
                args.ninja,
                "-C",
                str(build),
                "-t",
                "commands",
                "lib/CMakeFiles/libcurl.dir/curl_addrinfo.c.o",
            ],
            text=True,
        )
        .strip()
        .splitlines()[-1]
    )
    original = shlex.split(command)
    compiler = []
    source = None
    skip = False
    for argument in original:
        if skip:
            skip = False
            continue
        if argument in ("-o", "-MF", "-MT", "-MQ"):
            skip = True
            continue
        if argument in ("-c", "-MD", "-MMD") or argument.startswith(
            "-DCURL_SA_FAMILY_T="
        ):
            continue
        if argument.endswith("/curl_addrinfo.c"):
            source = Path(argument)
            continue
        compiler.append(argument)
    if source is None:
        raise RuntimeError("configured command did not name curl_addrinfo.c")
    abi = Path(__file__).resolve().with_name("native-abi.c")
    results = {}
    for variant in ("old", "corrected"):
        flags = [] if variant == "old" else ["-DCURL_SA_FAMILY_T=unsigned short"]
        checked = subprocess.run(
            [*compiler, *flags, "-fsyntax-only", str(abi)],
            cwd=build,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
        )
        (output / f"{variant}-abi.log").write_text(checked.stdout)
        if variant == "old":
            if (
                checked.returncode == 0
                or "curl must preserve the complete Symbian IPv4 address family"
                not in checked.stdout
            ):
                raise RuntimeError(
                    "old real-SDK cast did not reproduce the expected ABI failure"
                )
        elif checked.returncode:
            raise RuntimeError(
                f"corrected real-SDK ABI failed; see {output}/corrected-abi.log"
            )
        assembly = output / f"{variant}-curl_addrinfo.s"
        subprocess.run(
            [*compiler, *flags, "-S", str(source), "-o", str(assembly)],
            cwd=build,
            check=True,
        )
        results[variant] = {
            "abi_exit": checked.returncode,
            "assembly_sha256": hashlib.sha256(assembly.read_bytes()).hexdigest(),
        }
    results["source"] = {
        "path": str(source),
        "sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
    }
    results["configured_command"] = command
    (output / "result.json").write_text(json.dumps(results, indent=2) + "\n")
    print(
        "PASS: actual SDK rejects old family width and accepts corrected cast; both ARM assemblies saved"
    )


if __name__ == "__main__":
    main()
