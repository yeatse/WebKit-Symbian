#!/usr/bin/env python3
"""Validate retained ordinary browser-download evidence without running a guest."""
import argparse
import datetime as dt
import hashlib
import json
from pathlib import Path
import re
from urllib.parse import parse_qs, urlsplit

PAYLOAD_BYTES = 524288
PAYLOAD_SHA256 = hashlib.sha256(bytes(range(256)) * 2048).hexdigest()
POST_BODY = b"token=post-once"
CASE_PATHS = {"post": "/post-attachment", "get": "/attachment", "gzip": "/gzip",
              "chunked": "/chunked", "cancel-save": "/attachment",
              "cancel-transfer": "/slow", "existing": "/attachment"}
SUCCESS_CASES = {"post", "get", "gzip", "chunked"}
ROLES = {case: {"start", "save-accept"} for case in SUCCESS_CASES}
ROLES.update({"cancel-save": {"start", "save-cancel"},
              "cancel-transfer": {"start", "save-accept", "transfer-cancel"},
              "existing": {"start", "existing-attempt", "existing-dismiss"}})
CRASH = re.compile(r"access violation|panicked|KERN-EXEC|USER-EXEC|segmentation fault|AddressSanitizer|runtime error:", re.I)
TERMINAL = re.compile(r"download (\d+): terminal=(\d+) error=(-?\d+) cleanup=(-?\d+)")
PROGRESS = re.compile(r"download (\d+): (\d+)/(-?\d+) bytes")
HEX = re.compile(r"[0-9a-f]{64}")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def timestamp(value):
    if not isinstance(value, str):
        raise ValueError("missing UTC timestamp")
    result = dt.datetime.fromisoformat(value.replace("Z", "+00:00"))
    if result.utcoffset() != dt.timedelta(0):
        raise ValueError("timestamp must explicitly use UTC")
    return result


def relative_name(value):
    return (isinstance(value, str) and value and not value.startswith("/")
            and not any(c in value for c in "\\\x00")
            and all(part not in ("", ".", "..") for part in value.split("/")))


def basename(value):
    return relative_name(value) and "/" not in value


def guest_path(value):
    return value.replace("/", "\\").rstrip("\\").casefold() if isinstance(value, str) else None


class Evidence:
    def __init__(self):
        self.errors, self.missing, self.artifacts, self.checks = [], [], [], []

    def read(self, path, label, json_data=True):
        if path is None or not Path(path).is_file():
            self.missing.append(label + (": " + str(path) if path else ""))
            return None
        path = Path(path)
        try:
            self.artifacts.append({"kind": label, "path": str(path.resolve()), "sha256": digest(path)})
            text = path.read_text()
            return json.loads(text) if json_data else text
        except (OSError, UnicodeError, ValueError) as error:
            self.errors.append(label + ": " + str(error))
            return None

    def check(self, condition, message):
        if not condition:
            self.errors.append(message)
        return condition


def snapshot(value, label, ready, evidence):
    if not isinstance(value, dict):
        evidence.errors.append(label + " must be an object")
        return None
    for key in ("run", "case"):
        evidence.check(value.get(key) == ready[key], label + " " + key + " mismatch")
    for key in ("guest_directory", "host_directory"):
        evidence.check(isinstance(value.get(key), str) and bool(value[key]), label + " missing " + key)
    try:
        timestamp(value.get("captured_utc"))
    except (ValueError, TypeError) as error:
        evidence.errors.append(label + ": " + str(error))
    directories = value.get("directories")
    evidence.check(isinstance(directories, list) and all(relative_name(name) for name in directories)
                   and len(set(directories)) == len(directories), label + " invalid directory listing")
    files = value.get("files")
    if not isinstance(files, list):
        evidence.errors.append(label + " files must be an array")
        return None
    result = {}
    for entry in files:
        if not isinstance(entry, dict) or set(entry) != {"name", "bytes", "sha256", "mtime_ns", "inode"}:
            evidence.errors.append(label + " malformed file identity")
            continue
        if not relative_name(entry["name"]) or entry["name"] in result:
            evidence.errors.append(label + " invalid or duplicated filename")
            continue
        if (type(entry["bytes"]) is not int or entry["bytes"] < 0 or type(entry["mtime_ns"]) is not int
                or type(entry["inode"]) is not int or not isinstance(entry["sha256"], str)
                or not HEX.fullmatch(entry["sha256"])):
            evidence.errors.append(label + " invalid file metadata")
            continue
        result[entry["name"]] = entry
    if value.get("archive_directory"):
        for name, entry in result.items():
            archived = Path(value["archive_directory"]) / name
            if not archived.is_file():
                evidence.missing.append(label + " archived file: " + str(archived))
            else:
                evidence.check(archived.stat().st_size == entry["bytes"] and digest(archived) == entry["sha256"],
                               label + " archive bytes/hash mismatch")
    return result


def validate_evidence(fixture, before=None, after=None, destination_name=None, interactions=(),
                      guest_log=None, emulator_log=None, runner_report=None):
    evidence = Evidence()
    fixture = Path(fixture)
    ready = evidence.read(fixture / "ready.json", "fixture identity")
    result = {"suite": "symbian-download-runtime", "status": "INCOMPLETE",
              "errors": evidence.errors, "missing": evidence.missing,
              "checks": evidence.checks, "artifacts": evidence.artifacts}
    if ready is None:
        result["status"] = "FAIL" if evidence.errors else "INCOMPLETE"
        return result
    if not isinstance(ready, dict) or ready.get("case") not in CASE_PATHS or not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", str(ready.get("run", ""))):
        evidence.errors.append("invalid fixture run/case identity")
        result["status"] = "FAIL"
        return result
    case, run = ready["case"], ready["run"]
    result.update(run=run, case=case)
    url = urlsplit(ready.get("url", ""))
    evidence.check(url.scheme == "http" and url.hostname in {"localhost", "127.0.0.1"}
                   and parse_qs(url.query).get("run") == [run], "fixture URL is not this controlled HTTP run")
    evidence.check(ready.get("payload_bytes") == PAYLOAD_BYTES and ready.get("payload_sha256") == PAYLOAD_SHA256,
                   "fixture payload identity is not the known 524288-byte payload")
    evidence.check(basename(destination_name), "destination must be one exact basename")
    source = ready.get("fixture_source")
    source_text = evidence.read(source, "reused HTTP fixture source", False) if isinstance(source, str) else None
    if not isinstance(source, str):
        evidence.missing.append("reused HTTP fixture source identity")
    elif source_text is not None:
        evidence.check(HEX.fullmatch(str(ready.get("fixture_source_sha256", ""))) is not None
                       and digest(Path(source)) == ready["fixture_source_sha256"], "reused HTTP fixture source hash mismatch")

    raw_events = evidence.read(fixture / "events.jsonl", "HTTP events", False)
    if raw_events is not None:
        events = []
        try:
            events = [json.loads(line) for line in raw_events.splitlines() if line.strip()]
            if any(not isinstance(row, dict) for row in events):
                raise ValueError("event must be an object")
        except (ValueError, TypeError) as error:
            evidence.errors.append("malformed HTTP event log: " + str(error))
            events = []
        evidence.check(all(row.get("run") == run and row.get("case") == case for row in events), "HTTP event run/case mismatch")
        evidence.check([row.get("sequence") for row in events] == list(range(1, len(events) + 1)), "HTTP event sequence is missing or duplicated")
        evidence.check(not any(row.get("event") == "gate-timeout" for row in events), "transfer-cancellation gate timed out")
        requests = [row for row in events if row.get("event") == "request" and row.get("path") == CASE_PATHS[case]]
        if not requests:
            evidence.missing.append("expected browser download request")
        else:
            evidence.check(len(requests) == 1, "download endpoint must be requested exactly once")
            expected_body = POST_BODY if case == "post" else b""
            for row in requests:
                evidence.check(row.get("method") == ("POST" if case == "post" else "GET"), "download method mismatch")
                evidence.check(row.get("body_bytes") == len(expected_body)
                               and row.get("body_sha256") == hashlib.sha256(expected_body).hexdigest(), "download request body mismatch")
        evidence.check(not any(row.get("event") == "request" and row.get("path") in set(CASE_PATHS.values()) - {CASE_PATHS[case]} for row in events),
                       "another download endpoint was requested during this case")
        if case in SUCCESS_CASES:
            sends = [row for row in events if row.get("event") == "sent" and row.get("path") == CASE_PATHS[case]]
            if not sends:
                evidence.missing.append("complete server payload observation")
            else:
                evidence.check(len(sends) == 1 and sends[0].get("decoded_sha256") == PAYLOAD_SHA256
                               and type(sends[0].get("wire_body_bytes")) is int and sends[0]["wire_body_bytes"] > 0,
                               "server payload observation mismatch")
                if case != "gzip":
                    evidence.check(sends[0].get("wire_body_bytes") == PAYLOAD_BYTES, "server sent length mismatch")
        elif case == "cancel-transfer":
            gates = [row for row in events if row.get("event") == "gate-wait"]
            releases = [row for row in events if row.get("event") == "release-requested"]
            finished = [row for row in events if row.get("event") in {"sent", "disconnected"} and row.get("path") == "/slow"]
            if not gates:
                evidence.missing.append("held-open transfer gate observation")
            else:
                evidence.check(len(gates) == 1 and gates[0].get("wire_body_bytes") == 8192, "transfer gate observation mismatch")
            if not releases or not finished:
                evidence.missing.append("released and settled transfer fixture")
            else:
                evidence.check(len(releases) == 1 and len(finished) == 1
                               and (not gates or gates[0]["sequence"] < releases[0]["sequence"])
                               and releases[0]["sequence"] < finished[0]["sequence"], "transfer fixture release/settlement ordering mismatch")
        evidence.checks.append("HTTP request count, method and exact request-body identity")

    old_value = evidence.read(before, "before directory snapshot")
    new_value = evidence.read(after, "after directory snapshot")
    old = snapshot(old_value, "before", ready, evidence) if old_value is not None else None
    new = snapshot(new_value, "after", ready, evidence) if new_value is not None else None
    if old is not None and new is not None:
        evidence.check(all(old_value.get(key) == new_value.get(key) for key in ("guest_directory", "host_directory")), "before/after selected directories differ")
        evidence.check(old_value.get("directories") == new_value.get("directories"), "download changed the directory tree")
        try:
            evidence.check(timestamp(new_value["captured_utc"]) >= timestamp(old_value["captured_utc"]), "snapshot order is reversed")
        except (ValueError, KeyError, TypeError):
            pass
        if case in SUCCESS_CASES:
            evidence.check(destination_name not in old, "successful case destination existed before the browser request")
            evidence.check(set(new) == set(old) | {destination_name}, "success must add only the chosen destination, with no leftover temporary file")
            evidence.check(all(new.get(name) == value for name, value in old.items()), "success changed a pre-existing directory entry")
            destination = new.get(destination_name, {})
            evidence.check(destination.get("bytes") == PAYLOAD_BYTES and destination.get("sha256") == PAYLOAD_SHA256,
                           "actual destination bytes/hash do not match the decoded payload")
        else:
            evidence.check(old == new, "cancel/existing case changed the directory or left a temporary file")
            if case == "existing":
                sentinel = old.get(destination_name)
                evidence.check(sentinel is not None, "existing-target case has no pre-existing target")
                if sentinel is not None:
                    evidence.check(sentinel["sha256"] != PAYLOAD_SHA256,
                                   "existing target must contain distinct sentinel bytes; identical payload cannot prove non-overwrite")
            else:
                evidence.check(destination_name not in old, "cancel case destination was not initially absent")
        evidence.checks.append("actual directory/file size, SHA256, inode and mtime preservation")

    roles, image_count = [], 0
    if not interactions:
        evidence.missing.append("real native interaction record")
    for path in interactions:
        interaction = evidence.read(path, "native interaction")
        if interaction is None:
            continue
        if not isinstance(interaction, dict):
            evidence.errors.append("native interaction must be an object")
            continue
        evidence.check(interaction.get("run") == run and interaction.get("case") == case, "native interaction run/case mismatch")
        if interaction.get("status") != "INPUT_RECORDED":
            evidence.missing.append("completed native input sequence")
        records = interaction.get("records", [])
        if not isinstance(records, list):
            evidence.errors.append("interaction records must be an array")
        else:
            roles += [record["action"]["role"] for record in records if isinstance(record, dict)
                      and isinstance(record.get("action"), dict) and isinstance(record["action"].get("role"), str)]
        screenshots = interaction.get("screenshots", [])
        if not isinstance(screenshots, list):
            evidence.errors.append("interaction screenshots must be an array")
            continue
        for shot in screenshots:
            if not isinstance(shot, dict) or not isinstance(shot.get("path"), str) or not relative_name(shot["path"]):
                evidence.errors.append("screenshot path must stay relative to its interaction record")
                continue
            image = Path(path).parent / shot["path"]
            if not image.is_file():
                evidence.missing.append("native screenshot: " + str(image))
                continue
            data = image.read_bytes()
            if not evidence.check(HEX.fullmatch(str(shot.get("sha256", ""))) is not None and digest(image) == shot["sha256"], "native screenshot hash mismatch"):
                continue
            if not evidence.check(len(data) >= 24 and data[:8] == b"\x89PNG\r\n\x1a\n" and data[12:16] == b"IHDR"
                                  and int.from_bytes(data[16:20], "big") > 0 and int.from_bytes(data[20:24], "big") > 0,
                                  "native screenshot is not a PNG image"):
                continue
            image_count += 1
            evidence.artifacts.append({"kind": "native screenshot", "path": str(image.resolve()), "sha256": shot["sha256"]})
    missing_roles = ROLES[case] - set(roles)
    if missing_roles:
        evidence.missing.append("native action roles: " + ", ".join(sorted(missing_roles)))
    if roles.count("start") > 1:
        evidence.errors.append("case has more than one native start action")
    if not missing_roles:
        evidence.check(all(roles.index("start") < roles.index(role) for role in ROLES[case] - {"start"}),
                       "native actions occurred before the case start")
        if case == "cancel-transfer":
            evidence.check(roles.index("save-accept") < roles.index("transfer-cancel"), "transfer cancelled before save acceptance")
        if case == "existing":
            evidence.check(roles.index("existing-attempt") < roles.index("existing-dismiss"), "existing target dismissed before attempt")
    if not image_count:
        evidence.missing.append("hash-verified native UI screenshots")

    guest = evidence.read(guest_log, "fresh guest log", False)
    host = evidence.read(emulator_log, "fresh emulator log", False)
    for name, content in (("guest", guest), ("emulator", host)):
        if content is not None:
            evidence.check(not CRASH.search(content), name + " log contains a crash/panic diagnostic")
    if guest is not None:
        if ready["url"] not in guest:
            evidence.missing.append("guest load marker for this exact run URL")
        terminals = TERMINAL.findall(guest)
        if not terminals:
            evidence.missing.append("guest terminal download callback")
        else:
            evidence.check(len(terminals) == 1, "expected exactly one guest terminal download callback")
            expected = 0 if case in SUCCESS_CASES else 1
            for identifier, failure, error, cleanup in terminals:
                evidence.check((int(failure), int(error), int(cleanup)) == (expected, 0, 0), "guest terminal result/error/cleanup mismatch")
                if case == "cancel-transfer":
                    preceding = guest[:TERMINAL.search(guest).start()]
                    evidence.check(any(candidate == identifier and int(received) > 0 for candidate, received, _ in PROGRESS.findall(preceding)),
                                   "transfer cancellation has no positive progress for its terminal download ID")
        evidence.checks.append("fresh guest callback result, cleanup status and run URL")

    runner = evidence.read(runner_report, "runner report")
    if runner is not None:
        if not isinstance(runner, dict):
            evidence.errors.append("runner report must be an object")
        else:
            evidence.check(all(runner.get(key) == ready[key] for key in ("run", "case", "url")), "runner identity mismatch")
            evidence.check(type(runner.get("host_exit_code")) is int and runner["host_exit_code"] == 0 and runner.get("normal_close") is True
                           and runner.get("forced_cleanup") is False, "runner did not close normally without forced cleanup")
            for key in ("browser_sha256", "emulator_sha256"):
                value = str(runner.get(key, ""))
                if not HEX.fullmatch(value) or value == "0" * 64:
                    evidence.missing.append("runner actual binary identity: " + key)
            if runner.get("native_ui_reviewed") is not True:
                evidence.missing.append("operator review of actual native UI screenshots")
            selected = runner.get("selected_guest_path")
            if case != "cancel-save" and not isinstance(selected, str):
                evidence.missing.append("operator-confirmed selected guest path")
            elif selected is not None and old_value is not None and basename(destination_name):
                expected_path = guest_path(old_value.get("guest_directory"))
                evidence.check(expected_path is not None and guest_path(selected) == expected_path + "\\" + destination_name.casefold(),
                               "selected native guest path does not match the snapshotted destination")
            try:
                timestamp(runner.get("finished_utc"))
            except (ValueError, TypeError) as error:
                evidence.errors.append("runner finish timestamp: " + str(error))
    result["status"] = "FAIL" if evidence.errors else "INCOMPLETE" if evidence.missing else "PASS"
    result["scope"] = "Retained real-browser request, native-input, native-file and normal-close evidence; host fixture tests alone cannot pass."
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=Path, required=True)
    parser.add_argument("--before", type=Path)
    parser.add_argument("--after", type=Path)
    parser.add_argument("--destination-name", required=True)
    parser.add_argument("--interaction", type=Path, action="append", default=[])
    parser.add_argument("--guest-log", type=Path)
    parser.add_argument("--emulator-log", type=Path)
    parser.add_argument("--runner-report", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        result = validate_evidence(args.fixture, args.before, args.after, args.destination_name,
                                   args.interaction, args.guest_log, args.emulator_log, args.runner_report)
    except (OSError, ValueError, KeyError, TypeError) as error:
        result = {"suite": "symbian-download-runtime", "status": "FAIL", "errors": [str(error)]}
    text = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.write_text(text)
    print(text, end="")
    return {"PASS": 0, "FAIL": 1, "INCOMPLETE": 2}[result["status"]]


if __name__ == "__main__":
    raise SystemExit(main())
