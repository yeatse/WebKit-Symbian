#!/usr/bin/env python3
"""Restore missing SDK headers from its own pinned Symbian source snapshots."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import sys


def apply(sdk, overlay):
    manifest = json.loads((overlay / "manifest.json").read_text())
    version = (sdk / "VERSION").read_text().strip()
    if version != manifest["sdk_version"]:
        raise ValueError(f"SDK {version} does not match overlay {manifest['sdk_version']}")
    sources = json.loads((sdk / "share/symbian/native/sources.json").read_text())["sources"]
    revisions = {source["repository"]: source["revision"] for source in sources.values()}

    # Validate every source and destination before adding any files. An SDK with
    # changed headers must be reconciled explicitly rather than overwritten.
    pending = []
    unchanged = 0
    for header in manifest["headers"]:
        relative = Path(header["path"])
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError(f"Invalid overlay path: {relative}")
        if revisions.get(header["repository"]) != header["revision"]:
            raise ValueError(f"SDK source revision mismatch: {header['repository']}")
        data = (overlay / relative).read_bytes()
        blob = hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
        if hashlib.sha256(data).hexdigest() != header["sha256"] or blob != header["git_blob_sha1"]:
            raise ValueError(f"Official source hash mismatch: {relative}")
        destination = sdk / relative
        if destination.is_symlink():
            raise ValueError(f"Refusing SDK header symlink: {relative}")
        if destination.exists():
            if not destination.is_file() or destination.read_bytes() != data:
                raise ValueError(f"SDK header differs; not overwriting: {relative}")
            unchanged += 1
        else:
            pending.append((destination, data))

    for destination, data in pending:
        destination.parent.mkdir(parents=True, exist_ok=True)
        with destination.open("xb") as stream:
            stream.write(data)
        destination.chmod(0o644)
    print(f"SDK {version}: restored {len(pending)} official headers; {unchanged} already match")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, default=os.environ.get("SYMBIAN_SDK_PREFIX"))
    args = parser.parse_args()
    if args.sdk is None:
        parser.error("set SYMBIAN_SDK_PREFIX or pass --sdk")
    overlay = Path(__file__).resolve().parents[1] / "patches/sdk-0.3.0"
    try:
        apply(args.sdk, overlay)
    except (OSError, ValueError, KeyError) as error:
        print(f"SDK header restoration failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
