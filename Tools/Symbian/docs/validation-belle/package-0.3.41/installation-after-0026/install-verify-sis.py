#!/usr/bin/env python3
"""Validate local SIS installation; default is preparation only, --execute runs EKA.

No payload is copied into a guest drive. Existing C-drive shadows are preserved.
"""

import argparse
import contextlib
import fcntl
import hashlib
import importlib.util
import json
import os
import re
import signal
import struct
import subprocess
import sys
import time
from pathlib import Path

WORK = Path(__file__).resolve().parent
REPO = Path("/workspace/WebKit-Symbian-graphics")
VERIFY = REPO / "Tools/Symbian/scripts/verify-sis-payloads.py"
CLOSE = REPO / "Tools/Symbian/eka2l1/close-emulator-window.py"
DATA = Path("/workspace/eka-validation/xdg/data/EKA2L1/data")
EMULATOR = Path("/workspace/EKA2L1-validation/build/bin/eka2l1_qt")
REPORT_SHA = "3e676ca4133dce545a57f8b41c48361126da9808c32b13be59c3e836b18038fd"
PANIC = re.compile(r"access violation|panicked|KERN-EXEC|USER-EXEC", re.IGNORECASE)


def require(value, message):
    if not value:
        raise RuntimeError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def identity(path):
    if path is None or not path.exists():
        return {"exists": False}
    raw = path.read_bytes()
    return {"exists": True, "path": str(path), "bytes": len(raw), "sha256": digest(raw)}


def load_verifier():
    spec = importlib.util.spec_from_file_location("verify_sis_payloads", VERIFY)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class RegistryReader:
    """EKA 5.4 registry layout, scoped to these ASCII package/path strings.

    Based on package/src/registry.cpp, utils/include/utils/des.h and
    utils/src/cardinality.cpp. Unsupported SCSU is rejected, not guessed.
    """

    def __init__(self, data):
        self.data, self.position = data, 0

    def take(self, count):
        require(0 <= count <= len(self.data) - self.position, "truncated registry")
        result = self.data[self.position : self.position + count]
        self.position += count
        return result

    def number(self, code="I"):
        return struct.unpack("<" + code, self.take(struct.calcsize("<" + code)))[0]

    def descriptor(self, unicode=True):
        first = self.number("B")
        if not first & 1:
            length = first >> 1
        elif not first & 2:
            length = (first | self.number("B") << 8) >> 2
        else:
            raise RuntimeError("registry cardinality outside this release scope")
        is_bytes = bool(length & 1)
        raw = self.take(length >> 1)
        if not unicode:
            require(is_bytes, "expected 8-bit descriptor")
            return raw
        require(not is_bytes, "expected Unicode descriptor")
        require(
            all(b in (9, 10, 13) or 0x20 <= b <= 0x7F for b in raw),
            "unsupported non-ASCII SCSU package string",
        )
        return raw.decode("ascii")

    def hash(self):
        require(self.number() == 1, "unexpected registry hash algorithm")
        return self.descriptor(False).hex()

    def count(self):
        count = self.number()
        require(count <= 1000, "unexpected registry collection size")
        return count

    def package(self):
        return {
            "uid": hex(self.number()),
            "name": self.descriptor(),
            "vendor": self.descriptor(),
            "index": self.number(),
        }

    def parse(self):
        result = self.package()
        result["drives"] = self.number()
        result["completely_present_raw"] = self.number()
        result["sids"] = [hex(self.number()) for _ in range(self.count())]
        result["controllers"] = []
        for _ in range(self.count()):
            # EKA writes the version union three times here; token version below
            # is the authoritative three independent version components.
            words = [self.number() for _ in range(3)]
            offset = self.number()
            result["controllers"].append(
                {"version_words": words, "offset": offset, "hash": self.hash()}
            )
        result["version"] = ".".join(str(self.number()) for _ in range(3))
        result["language"] = self.number()
        result["selected_drive_mask"] = self.number()
        self.take(8)
        result["format"] = [self.number("H"), self.number("H")]
        require(result["format"] == [5, 4], "expected EKA registry format 5.4")
        result["localized_vendor"] = self.descriptor()
        result["install_type"] = self.number()
        result["in_rom"] = self.number()
        result["deletable_preinstalled_raw"] = self.number()
        result["signed_raw"] = self.number()
        result["trust_raw"] = self.number()
        self.take(4 + 8)
        self.take(self.count() * 28)
        result["embedded"] = [self.package() for _ in range(self.count())]
        self.take(self.count() * 8)
        result["files"] = []
        for _ in range(self.count()):
            result["files"].append(
                {
                    "target": self.descriptor(),
                    "mime": self.descriptor(),
                    "operation": self.number(),
                    "options": self.number(),
                    "sha1": self.hash(),
                    "bytes": self.number("Q"),
                    "index": self.number(),
                    "sid": hex(self.number()),
                }
            )
        self.take(36)
        self.take(self.count() * 4)
        result["is_removable"] = self.number()
        result["signed_by_sucert_raw"] = self.number()
        self.take(self.count() * 4)
        result["localized_names"] = [self.descriptor() for _ in range(self.count())]
        result["localized_vendors"] = [self.descriptor() for _ in range(self.count())]
        require(self.position == len(self.data), "unparsed registry bytes")
        return result


def guest_path(data, target):
    require(
        re.fullmatch(r"[a-zA-Z]:\\[^:]+", target) is not None,
        "invalid drive-qualified target",
    )
    parts = [target[0].lower(), *target[3:].split("\\")]
    require(
        all(part and part not in (".", "..") for part in parts), "invalid guest path"
    )
    current = data / "drives"
    for part in parts:
        if not current.is_dir():
            return None
        # Match EKA common::resolve_case_insensitive_path: prefer the exact
        # component spelling before case folding. Preserve pre-existing
        # WebKit/webkit directories instead of merging or rewriting them.
        exact = current / part
        matches = (
            [exact]
            if exact.exists()
            else [
                entry
                for entry in current.iterdir()
                if entry.name.casefold() == part.casefold()
            ]
        )
        require(len(matches) <= 1, "case-colliding path: " + str(current / part))
        if not matches:
            return None
        current = matches[0]
        require(not current.is_symlink(), "unexpected symlink in guest path")
    return current


def expected_target(item):
    target = item["target"]
    return "e" + target[1:] if target.startswith("!:") else target


def snapshots(data, package):
    values = []
    for item in package["payloads"]:
        target = expected_target(item)
        values.append({"target": target, **identity(guest_path(data, target))})
        if item["target"].startswith("!:"):
            shadow = "c" + target[1:]
            values.append(
                {"target": shadow, "shadow": True, **identity(guest_path(data, shadow))}
            )
    return values


def controller_bytes(sis_path, verifier):
    _, contents, _ = verifier.sis.field(sis_path.read_bytes(), 16)
    compressed, _ = verifier.unique_field(verifier.sis.fields(contents), 3)
    expanded = verifier.sis.decompress(compressed)
    kind, raw, end = verifier.sis.field(expanded)
    require(kind == 13 and end == len(expanded), "invalid SIS controller")
    return raw


def controller_lengths(raw, verifier):
    install, _ = verifier.unique_field(verifier.sis.fields(raw), 28)
    descriptions = verifier.sis.array(verifier.sis.fields(install)[0][2], 24)
    lengths = []
    for description in descriptions:
        _, _, position = verifier.sis.field(description)
        _, _, position = verifier.sis.field(description, position)
        kind, _, position = verifier.sis.field(description, position)
        if kind == 41:
            kind, _, position = verifier.sis.field(description, position)
        require(kind == 25, "missing SIS file hash before length metadata")
        _, _, stored, expanded, _ = struct.unpack("<IIQQI", description[position:])
        lengths.append({"stored": stored, "expanded": expanded})
    return lengths


def verify_installed(data, package, verifier):
    base = "c:\\sys\\install\\sisregistry\\" + package["uid3"][2:] + "\\"
    reg_path = guest_path(data, base + "00000000.reg")
    ctl_path = guest_path(data, base + "00000000_0000.ctl")
    require(reg_path and ctl_path, "missing installed registry/controller")
    registry = RegistryReader(reg_path.read_bytes()).parse()
    require(
        registry["uid"] == package["uid3"] and registry["version"] == "0.3.41",
        "registry UID/version mismatch",
    )
    require(
        registry["index"] == 0
        and registry["install_type"] == 0
        and not registry["in_rom"],
        "unexpected package kind",
    )
    require(registry["selected_drive_mask"] == 1 << 4, "CLI did not select E drive")
    require(not registry["embedded"], "unexpected embedded registry")
    require(
        len(registry["controllers"]) == 1 and registry["controllers"][0]["offset"] == 0,
        "unexpected controller list",
    )
    controller = ctl_path.read_bytes()
    require(
        len(controller) >= 4
        and struct.unpack_from("<I", controller)[0] == len(controller) - 4,
        "controller length mismatch",
    )
    require(
        controller[4:] == controller_bytes(Path(package["path"]), verifier),
        "installed controller differs from SIS",
    )
    require(
        len(registry["files"]) == len(package["payloads"]),
        "registry payload count mismatch",
    )
    files, mask = [], 1 << 4
    lengths = controller_lengths(controller[4:], verifier)
    for registered, item, length in zip(
        registry["files"], package["payloads"], lengths
    ):
        target = expected_target(item)
        require(
            registered["target"].casefold() == target.casefold(),
            "registry target/order mismatch",
        )
        require(
            registered["bytes"] in (length["stored"], length["expanded"])
            and length["expanded"] == item["bytes"]
            and registered["operation"] == 1
            and registered["options"] == 0
            and not registered["mime"],
            "registry metadata mismatch",
        )
        require(registered["index"] == len(files), "registry index mismatch")
        path = guest_path(data, registered["target"])
        require(path and path.is_file(), "installed payload missing: " + target)
        raw = path.read_bytes()
        require(
            len(raw) == item["bytes"]
            and digest(raw) == item["sha256"]
            and raw == Path(item["source"]).read_bytes(),
            "installed/source bytes differ: " + target,
        )
        require(
            hashlib.sha1(raw).hexdigest() == registered["sha1"],
            "registry SHA-1 mismatch",
        )
        require(
            registered["sid"] == item.get("e32", {}).get("secure_id", "0x0"),
            "registry SID mismatch",
        )
        mask |= 1 << (ord(target[0].lower()) - ord("a"))
        files.append(
            {
                "target": registered["target"],
                **identity(path),
                "source_bytes_equal": True,
            }
        )
    require(registry["drives"] == mask, "registry drive mask mismatch")
    shadows = []
    for item in package["payloads"]:
        if item["target"].startswith("!:"):
            target = "c" + item["target"][1:]
            found = identity(guest_path(data, target))
            shadows.append(
                {
                    "target": target,
                    **found,
                    "matches_package": found.get("sha256") == item["sha256"],
                    "is_executable": "e32" in item,
                }
            )
    length_checks = [
        {
            "target": item["target"],
            "registry_value": registered["bytes"],
            "expected_uncompressed": length["expanded"],
            "sis_stored": length["stored"],
            "correct": registered["bytes"] == length["expanded"],
        }
        for registered, item, length in zip(
            registry["files"], package["payloads"], lengths
        )
    ]
    lengths_correct = all(check["correct"] for check in length_checks)
    return {
        "status": "PASS" if lengths_correct else "FAIL",
        "payload_status": "PASS",
        "registry_identity_status": "PASS",
        "registry_file_length_status": "PASS" if lengths_correct else "FAIL",
        "registry_file_lengths": length_checks,
        "registry": registry,
        "registry_file": identity(reg_path),
        "controller_file": identity(ctl_path),
        "controller_equals_sis": True,
        "registry_length_note": "Current EKA may store compressed length in the uncompressed_length field. Such entries FAIL registry metadata verification even when all installed payload bytes pass; stored-length correspondence is diagnostic only.",
        "files": files,
        "c_drive_shadows": shadows,
    }


def stop_failed_process(process, entry):
    if process is None or process.poll() is not None:
        return
    entry["forced_cleanup"] = True
    for sig in (signal.SIGTERM, signal.SIGKILL):
        if process.poll() is not None:
            break
        try:
            os.killpg(process.pid, sig)
        except ProcessLookupError:
            pass
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            continue


def install_one(args, package, output, descriptors, entry):
    log = output / (Path(package["package"]).stem + ".log")
    entry.update(
        command=[str(EMULATOR), "--device", args.device, "--install", package["path"]],
        log=str(log),
        forced_cleanup=False,
        close_requested=False,
    )
    process = None
    try:
        with log.open("x") as stream:
            process = subprocess.Popen(
                entry["command"],
                cwd=EMULATOR.parent,
                stdout=stream,
                stderr=subprocess.STDOUT,
                start_new_session=True,
                pass_fds=descriptors,
            )
            entry["pid"] = process.pid
            deadline = time.monotonic() + args.timeout
            marker = "registering with UID: " + package["uid3"]
            while time.monotonic() < deadline:
                text = log.read_text(errors="replace")
                require(
                    process.poll() is None,
                    f"installer exited before close: {process.returncode}",
                )
                require(
                    not PANIC.search(text), "panic or access violation during install"
                )
                position = text.find(marker)
                if (
                    position >= 0
                    and "Installation done!" in text[position:]
                    and "Supported features (for optimization)" in text
                ):
                    entry["installation_marker"] = True
                    break
                time.sleep(0.2)
            else:
                raise RuntimeError("installation/GUI readiness timeout")
            subprocess.run(
                [sys.executable, str(CLOSE), str(process.pid)], check=True, timeout=5
            )
            entry["close_requested"] = True
            process.wait(timeout=10)
            entry["host_exit"] = process.returncode
            require(process.returncode == 0, "installer failed to exit normally")
            require(
                not PANIC.search(log.read_text(errors="replace")),
                "panic or access violation at shutdown",
            )
            entry["guest_panic"] = False
    finally:
        previous = {
            sig: signal.signal(sig, signal.SIG_IGN)
            for sig in (signal.SIGINT, signal.SIGTERM)
        }
        try:
            stop_failed_process(process, entry)
            if process:
                entry["host_exit"] = process.poll()
            entry["log_identity"] = identity(log)
        finally:
            for sig, handler in previous.items():
                signal.signal(sig, handler)


def cancelled(signum, _frame):
    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, signal.SIG_IGN)
    raise RuntimeError(f"installation cancelled by signal {signum}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--execute", action="store_true")
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--emulator-sha256")
    parser.add_argument("--device", default="RM-707")
    parser.add_argument("--timeout", type=float, default=90)
    args = parser.parse_args()
    require(5 <= args.timeout <= 300, "invalid install timeout")
    verifier = load_verifier()
    raw_report = (WORK / "verification/package-verification.json").read_bytes()
    require(digest(raw_report) == REPORT_SHA, "package verification report changed")
    package_report = json.loads(raw_report)
    require(
        package_report["status"] == "PASS" and package_report["payload_count"] == 13,
        "missing prior package verification",
    )
    require(
        digest(VERIFY.read_bytes()) == package_report["validator_sha256"],
        "verifier changed",
    )
    require(
        digest(verifier.PARSER.read_bytes()) == package_report["field_parser_sha256"],
        "field parser changed",
    )
    for package in package_report["packages"]:
        require(
            identity(Path(package["path"]))["sha256"] == package["sha256"],
            "SIS changed",
        )
        for item in package["payloads"]:
            require(
                identity(Path(item["source"]))["sha256"] == item["sha256"],
                "payload source changed",
            )
    prepared = {
        "status": "PREPARED_NOT_EXECUTED",
        "sis_version": "0.3.41",
        "payload_count": 13,
        "script": identity(Path(__file__)),
        "emulator": identity(EMULATOR),
        "close_helper": identity(CLOSE),
        "packages": [p["package"] for p in package_report["packages"]],
        "default_install_drive": "E",
        "direct_payload_copy": False,
        "registry_trust_fields_are_not_signature_validation": True,
    }
    if not args.execute:
        print(json.dumps(prepared, indent=2))
        return
    require(args.output_dir is not None, "--execute requires a fresh --output-dir")
    require(
        args.emulator_sha256 == prepared["emulator"]["sha256"],
        "explicit emulator SHA required/mismatched",
    )
    require(
        Path(os.environ.get("XDG_DATA_HOME", "/invalid")).resolve() / "EKA2L1/data"
        == DATA,
        "source /workspace/eka-validation/env.sh before executing",
    )
    require(
        os.environ.get("PULSE_SERVER")
        == "unix:/workspace/symbian-media-work/runtime/native",
        "source /workspace/symbian-media-work/env.sh before executing",
    )
    output = args.output_dir.resolve()
    require(not output.exists(), "output exists; preserve previous attempt")
    output.mkdir(parents=True)
    result = {
        **prepared,
        "status": "FAIL",
        "installation_status": "NOT_COMPLETED",
        "runtime_validation_performed": False,
        "packages": [],
    }
    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, cancelled)
    try:
        with contextlib.ExitStack() as stack:
            locks = []
            for name in (".jsc-run.lock", ".browser-run.lock"):
                lock = stack.enter_context((DATA / name).open("a"))
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                locks.append(lock.fileno())
            for package in package_report["packages"]:
                entry = {
                    "package": package["package"],
                    "sis_sha256": package["sha256"],
                    "before": snapshots(DATA, package),
                    "status": "FAIL",
                }
                result["packages"].append(entry)
                install_one(args, package, output, tuple(locks), entry)
                entry["verification"] = verify_installed(DATA, package, verifier)
                entry["installation_status"] = "PASS"
                entry["status"] = entry["verification"]["status"]
            for package, entry in zip(package_report["packages"], result["packages"]):
                entry["verification"] = verify_installed(DATA, package, verifier)
                for before in entry["before"]:
                    if before.get("shadow"):
                        after = identity(guest_path(DATA, before["target"]))
                        require(
                            before["exists"] == after["exists"]
                            and before.get("sha256") == after.get("sha256"),
                            "SIS sequence unexpectedly changed a C-drive shadow",
                        )
            stale = [
                s
                for entry in result["packages"]
                for s in entry["verification"]["c_drive_shadows"]
                if s["exists"] and not s["matches_package"]
            ]
            metadata_ok = all(
                entry["verification"]["registry_file_length_status"] == "PASS"
                for entry in result["packages"]
            )
            result.update(
                status="PASS" if metadata_ok else "FAIL",
                payload_status="PASS",
                registry_metadata_status="PASS" if metadata_ok else "FAIL",
                metadata_followup="none"
                if metadata_ok
                else "Fix EKA registry uncompressed length and repeat formal SIS installation",
                installation_status="PASS",
                verified_payload_count=sum(
                    len(e["verification"]["files"]) for e in result["packages"]
                ),
                shadow_status="STALE_C_SHADOWS_PRESENT"
                if stale
                else "NO_STALE_C_SHADOWS",
                stale_c_shadows=stale,
                default_launch_payloads_match_package=not stale,
            )
    except (
        RuntimeError,
        OSError,
        ValueError,
        struct.error,
        subprocess.SubprocessError,
    ) as error:
        result["error"] = str(error)
    finally:
        (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "status": result["status"],
                "installation_status": result["installation_status"],
                "report": str(output / "result.json"),
            },
            indent=2,
        )
    )
    if result["status"] != "PASS":
        raise SystemExit(1)


if __name__ == "__main__":
    main()
