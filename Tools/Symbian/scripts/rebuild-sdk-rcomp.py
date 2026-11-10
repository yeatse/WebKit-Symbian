#!/usr/bin/env python3
"""Rebuild the pinned SDK 0.3.0 Linux resource compiler and verify identifiers."""

import argparse
import hashlib
import io
import json
import os
import shutil
import subprocess
import tarfile
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATCHES = ROOT / "patches/sdk-0.3.0/rcomp"


def digest(path):
    """Return the SHA-256 of one regular build input or output."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, **kwargs):
    """Run a bounded command with inherited outer Git redirection disabled."""
    environment = {
        key: value for key, value in os.environ.items() if not key.startswith("GIT_")
    }
    environment.update(GIT_CONFIG_NOSYSTEM="1", GIT_CONFIG_GLOBAL=os.devnull)
    return subprocess.run(
        list(map(str, command)),
        check=True,
        env=environment,
        timeout=180,
        **kwargs,
    )


def git(repository, *arguments):
    """Read a pinned Git object without relying on the worktree contents."""
    return run(["git", "-C", repository, *arguments], capture_output=True).stdout


def prepare_source(repository, record, managed=True):
    """Fetch pinned sources, repairing an interrupted managed initialization."""
    revision = record["rcomp_revision"]
    if not (repository / ".git").exists():
        if not managed:
            raise SystemExit("The supplied source repository is not a Git checkout")
        repository.mkdir(parents=True, exist_ok=True)
        run(["git", "init", "-q", repository])
    if managed:
        try:
            origin = git(repository, "remote", "get-url", "origin").decode().strip()
        except subprocess.CalledProcessError:
            run(
                [
                    "git",
                    "-C",
                    repository,
                    "remote",
                    "add",
                    "origin",
                    record["rcomp_repository"],
                ]
            )
        else:
            if origin != record["rcomp_repository"]:
                raise SystemExit("Unexpected cached rcomp source remote")
        run(["git", "-C", repository, "config", "remote.origin.promisor", "true"])
        run(
            [
                "git",
                "-C",
                repository,
                "config",
                "remote.origin.partialclonefilter",
                "blob:none",
            ]
        )
    try:
        git(repository, "cat-file", "-e", revision + "^{commit}")
    except subprocess.CalledProcessError:
        run(
            [
                "git",
                "-C",
                repository,
                "fetch",
                "--depth=1",
                "--filter=blob:none",
                "origin",
                revision,
            ]
        )
    return git(repository, "archive", revision, "bintools/rcomp")


def main():
    """Build and test first; replace only rcomp when --install is explicit."""
    default_work = Path(
        os.environ.get("SYMBIAN_WEBKIT_WORK", ROOT.parents[1] / "WebKitBuild/Symbian")
    ).resolve()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work-dir", type=Path, default=default_work / "sdk-rcomp")
    parser.add_argument(
        "--platform-source", type=Path, default=default_work / "symbian-platform"
    )
    parser.add_argument("--source-repository", type=Path)
    parser.add_argument(
        "--sdk",
        type=Path,
        default=Path(
            os.environ.get("SYMBIAN_SDK_PREFIX", default_work / "symbian-sdk")
        ),
    )
    parser.add_argument("--cxx", default=os.environ.get("HOST_CXX", "c++"))
    parser.add_argument("--install", action="store_true")
    args = parser.parse_args()
    work = args.work_dir.resolve()
    work.mkdir(parents=True, exist_ok=True)
    sdk = args.sdk.resolve()
    installed = sdk / "bin/rcomp"
    uidcrc = sdk / "bin/uidcrc"
    record = json.loads((PATCHES / "manifest.json").read_text())
    if digest(uidcrc) != record["uidcrc_sha256"]:
        raise SystemExit("Unsupported SDK uidcrc; expected the 0.3.0 Linux x86_64 tool")
    original_hash = digest(installed)
    previous_path = work / "installation.json"
    previous = json.loads(previous_path.read_text()) if previous_path.exists() else {}
    known_installed = (
        previous.get("sdk_tool") == str(installed)
        and previous.get("source_pin") == record["rcomp_revision"]
        and previous.get("new_sha256") == original_hash
    )
    if original_hash != record["stock_rcomp_sha256"] and not known_installed:
        raise SystemExit(
            "Unsupported rcomp bytes; refusing to replace an unknown SDK tool"
        )
    base = work / "base"
    base.mkdir(exist_ok=True)
    stock = base / "rcomp"
    if not stock.exists():
        old = (
            installed
            if original_hash == record["stock_rcomp_sha256"]
            else Path(previous["backup"])
        )
        if digest(old) != record["stock_rcomp_sha256"]:
            raise SystemExit(
                "Original SDK rcomp backup does not match the pinned release"
            )
        shutil.copy2(old, stock)
    if digest(stock) != record["stock_rcomp_sha256"]:
        raise SystemExit("Original SDK rcomp backup has changed")
    shutil.copy2(uidcrc, base / "uidcrc")
    compiler = shutil.which(args.cxx)
    if not compiler:
        raise SystemExit("A host C++ compiler is required")
    repository = (args.source_repository or work / "upstream").resolve()
    archive = prepare_source(repository, record, managed=not args.source_repository)
    source_hashes = {}
    with tempfile.TemporaryDirectory(prefix="build-", dir=work) as temporary:
        stage = Path(temporary)
        source = stage / "source"
        source.mkdir()
        with tarfile.open(fileobj=io.BytesIO(archive)) as package:
            for member in package.getmembers():
                if not (source / member.name).resolve().is_relative_to(source) or not (
                    member.isfile() or member.isdir()
                ):
                    raise SystemExit("Unexpected pinned source archive member")
            package.extractall(source, filter="data")
        support = stage / "support"
        for name, expected in record["support_files"].items():
            content = git(
                args.platform_source.resolve(),
                "show",
                record["platform_revision"] + ":" + name,
            )
            if hashlib.sha256(content).hexdigest() != expected:
                raise SystemExit("SDK resource-tool support source mismatch: " + name)
            target = support / Path(name).relative_to("research/rcomp")
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(content)
        patch_files = [support / "modern-host.patch", support / "uidcrc-spawn.patch"]
        for name, expected in record["patches"].items():
            patch = PATCHES / name
            if digest(patch) != expected:
                raise SystemExit("Resource compiler patch hash mismatch")
            patch_files.append(patch)
        for patch in patch_files:
            # patch(1) cannot discover an enclosing WebKit Git worktree.
            run(
                ["patch", "-p1", "--batch", "--forward", "--fuzz=0", "-i", patch],
                cwd=source,
                stdout=subprocess.DEVNULL,
            )
        rcomp = source / "bintools/rcomp"
        inputs = sorted((rcomp / "src").glob("*.cpp")) + sorted(
            (rcomp / "src").glob("*.CPP")
        )
        for path in sorted(rcomp.rglob("*")):
            if path.is_file():
                source_hashes[str(path.relative_to(source))] = digest(path)
        build = stage / "bin"
        build.mkdir()
        output = build / "rcomp"
        command = [
            compiler,
            "-std=c++14",
            "-O2",
            "-D__LINUX__",
            "-Wno-deprecated-declarations",
            "-ffile-prefix-map=" + str(source) + "=.",
            "-I" + str(support / "compat"),
            "-I" + str(rcomp / "inc"),
            "-I" + str(rcomp / "src"),
            *map(str, inputs),
            "-o",
            str(output),
        ]
        with (work / "build.log").open("w") as log:
            run(command, stdout=log, stderr=subprocess.STDOUT)
        shutil.copy2(uidcrc, build / "uidcrc")
        run(
            [
                "python3",
                ROOT / "tests/rcomp/test-rcomp.py",
                "--rcomp",
                output,
                "--reference",
                stock,
                "--output",
                work / "regression.json",
            ]
        )
        new_hash = digest(output)
        final_bin = work / "bin"
        final_bin.mkdir(exist_ok=True)
        shutil.copy2(output, final_bin / "rcomp")
        shutil.copy2(uidcrc, final_bin / "uidcrc")
        result = {
            "sdk_tool": str(installed),
            "source_pin": record["rcomp_revision"],
            "platform_revision": record["platform_revision"],
            "stock_sha256": record["stock_rcomp_sha256"],
            "new_sha256": new_hash,
            "backup": str(stock),
            "compiler": run(
                [compiler, "--version"], capture_output=True, text=True
            ).stdout.splitlines()[0],
            "manifest_sha256": digest(PATCHES / "manifest.json"),
            "source_sha256": source_hashes,
            "installed": args.install,
        }
        (work / "verification.json").write_text(json.dumps(result, indent=2) + "\n")
        if args.install:
            if digest(installed) != original_hash:
                raise SystemExit(
                    "SDK rcomp changed during the build; refusing to overwrite it"
                )
            temporary_tool = installed.with_name("rcomp.new-" + str(os.getpid()))
            shutil.copy2(output, temporary_tool)
            temporary_tool.chmod(0o755)
            os.replace(temporary_tool, installed)
            previous_path.write_text(json.dumps(result, indent=2) + "\n")
        print("rcomp source and 14 regressions verified; SHA256 " + new_hash)
        if args.install:
            print("Installed only the verified SDK rcomp tool")


if __name__ == "__main__":
    main()
