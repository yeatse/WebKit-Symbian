#!/usr/bin/env python3
"""Negative contracts for separating actual pressure evidence from page PASS."""

import importlib.util
import json
import pathlib
import unittest

SPEC = importlib.util.spec_from_file_location(
    "pressure_validator", pathlib.Path(__file__).with_name("validate-browser.py")
)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)
TOKEN = "1" * 32
URL = "http://localhost:8794/browser-live.js"


def console(kind, **data):
    return (
        f"console: {URL}:20: MEMORY_PRESSURE_{kind}: "
        + json.dumps({"token": TOKEN, **data})
        + "\n"
    )


READY = console("READY", count=1572864, bufferBytes=8388608)
RELIEF = "Symbian memory pressure: relief level=2 systemFreeRAM=4194304 valid=1 nativeLow=1\n"
HEAP = "Symbian memory pressure: JS heap phase=before cellBytes=100663296 extraBytes=8388608\nSymbian memory pressure: JS heap phase=after-scheduling cellBytes=100663296 extraBytes=8388608\n"
VERIFIED = console("VERIFIED", verifications=1, elapsedMS=20000) + console(
    "VERIFIED", verifications=2, elapsedMS=40000
)
PASSED = console("PASS", count=1572864, bufferBytes=8388608, verifications=2)
BROWSER = READY + RELIEF + HEAP + VERIFIED + PASSED
WORKER = f"START token={TOKEN} free=67108864 hardCap=260046848\nLOW token={TOKEN} crossed=1 free=4194304 total=62914560\nWORKER_RESULT token={TOKEN} accounting=PASS low=PASS pattern=PASS release=PASS\nEXIT error=0\n"


class ValidatorContracts(unittest.TestCase):
    def test_complete_independent_evidence(self):
        result = MODULE.validate(BROWSER, WORKER, TOKEN, URL)
        self.assertEqual(result["status"], "PASS")
        self.assertEqual(result["measured_cell_bytes"], [100663296])

    def test_page_pass_cannot_replace_native_or_live_evidence(self):
        cases = {
            "page-only": READY + VERIFIED + PASSED,
            "synthetic-no-native-low": BROWSER.replace("nativeLow=1", "nativeLow=0"),
            "buffer-bytes-not-cells": BROWSER.replace(
                "cellBytes=100663296", "cellBytes=8388608"
            ),
            "no-later-verification": READY + VERIFIED + RELIEF + HEAP + PASSED,
            "duplicate-pass": BROWSER + PASSED,
            "wrong-console-source": BROWSER.replace(URL, URL + "-wrong"),
            "release-storm": READY + (RELIEF + HEAP) * 5 + VERIFIED + PASSED,
        }
        for name, browser in cases.items():
            with self.subTest(name=name), self.assertRaises(ValueError):
                MODULE.validate(browser, WORKER, TOKEN, URL)
        with self.assertRaises(ValueError):
            MODULE.validate(
                BROWSER, WORKER.replace("release=PASS", "release=FAIL"), TOKEN, URL
            )
        with self.assertRaises(ValueError):
            MODULE.validate(BROWSER, WORKER + "EXIT error=0\n", TOKEN, URL)


if __name__ == "__main__":
    unittest.main()
