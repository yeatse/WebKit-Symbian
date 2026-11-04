#!/usr/bin/env python3
"""Build the WebKit installer and check its metadata and installed files."""

import argparse
import importlib.util
import os
from pathlib import Path
import struct
import subprocess
import tempfile

SYMBIAN = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("sis_fields", SYMBIAN / "scripts/sis_fields.py")
sis = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(sis)


def payload(entries, kind):
    return next(value for tag, value, _ in entries if tag == kind)


def check(work, icu_data):
    generated = {
        r"!:\sys\bin\jsc.exe": ("build/e32/e32/jsc.exe", b"JSC executable fixture\n"),
        r"!:\sys\bin\browser.exe": ("build/browser/e32/browser.exe", b"Browser executable fixture\n"),
        r"!:\private\10003a3f\import\apps\browser_reg.rsc": ("build/browser/e32/browser_reg.rsc", b"Browser registration\n"),
        r"!:\resource\apps\browser_loc.rsc": ("build/browser/e32/browser_loc.rsc", b"Browser caption\n"),
        r"!:\resource\apps\browser_ui.rsc": ("build/browser/e32/browser_ui.rsc", b"Browser commands\n"),
        r"c:\resource\icu\icudt70l.dat": ("icu/install/share/icu/icudt70l.dat", b"ICU data fixture\n"),
        r"c:\resource\webkit\cacert.pem": ("downloads/cacert-2026-09-25.pem", b"CA fixture\n"),
    }
    expected = {}
    for target, (relative, data) in generated.items():
        source = work / relative
        source.parent.mkdir(parents=True, exist_ok=True)
        source.write_bytes(data)
        expected[target] = data
    for name in ("basic.js", "es2020.js", "intl.js", "resizable-arraybuffer.js"):
        expected["c:\\data\\" + name] = (SYMBIAN / "tests" / name).read_bytes()
    expected[r"c:\data\webkit\test.html"] = (SYMBIAN / "browser/test.html").read_bytes()
    expected[r"c:\resource\fontconfig\fonts.conf"] = (SYMBIAN / "resources/fonts.conf").read_bytes()
    if icu_data == "static":
        del expected[r"c:\resource\icu\icudt70l.dat"]

    environment = os.environ.copy()
    for name in ("E32_BUILD", "BROWSER_BUILD", "ICU_DIR", "SIS_CERTIFICATE", "SIS_KEY"):
        environment.pop(name, None)
    environment.update(SYMBIAN_WEBKIT_WORK=str(work), ICU_DATA=icu_data, SIS_VERSION="0.3.0")
    subprocess.run(["bash", str(SYMBIAN / "scripts/package-sis.sh")], env=environment, check=True)
    packages = list((work / "sis").glob("*.sis"))
    assert [path.name for path in packages] == ["symbian-webkit.sis"], packages
    image = packages[0].read_bytes()
    assert struct.unpack_from("<III", image) == (0x10201A7A, 0, 0xE1A5C012)
    kind, contents, _ = sis.field(image, 16)
    assert kind == 12
    sections = sis.fields(contents)
    kind, controller, _ = sis.field(sis.decompress(payload(sections, 3)))
    assert kind == 13
    children = sis.fields(controller)
    info = sis.fields(payload(children, 14)[:-2])
    assert struct.unpack("<I", payload(info, 9))[0] == 0xE1A5C012
    names = next(raw for tag, value, raw in info if tag == 2)
    assert [name.decode("utf-16le") for name in sis.array(names, 1)] == ["WebKit for Symbian"]
    install = sis.fields(payload(children, 28))
    descriptions = sis.array(install[0][2], 24)
    units = sis.array(payload(sections, 30), 31)
    files = sis.array(units[0], 32)
    assert len(descriptions) == len(files) == len(expected)
    observed = {}
    for description, stored in zip(descriptions, files):
        kind, destination, _ = sis.field(description)
        assert kind == 1
        kind, compressed, _ = sis.field(stored)
        assert kind == 3
        observed[destination.decode("utf-16le")] = sis.decompress(compressed)
    assert observed == expected, (observed.keys(), expected.keys())
    print(f"PASS: symbian-webkit.sis; ICU_DATA={icu_data}; {len(expected)} payloads")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--icu-data", choices=("file", "static"), default="file")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="symbian-sis-package-") as temporary:
        check(Path(temporary), args.icu_data)


if __name__ == "__main__":
    main()
