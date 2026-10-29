#!/usr/bin/env python3
"""Exercise immutable HTTP reports and six distinct server origins."""
import json
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request
from contract import CASES

root = Path(__file__).resolve().parent
with tempfile.TemporaryDirectory() as directory:
    # Reserve an available contiguous test range before starting the server.
    for base in range(18790, 18990, 6):
        sockets = []
        try:
            for index in range(6):
                listener = socket.socket(); listener.bind(("127.0.0.1", base + index)); sockets.append(listener)
            break
        except OSError:
            for listener in sockets: listener.close()
    else: raise RuntimeError("No free test ports")
    for listener in sockets: listener.close()
    process = subprocess.Popen([sys.executable, str(root / "server.py"), "--bind", "127.0.0.1", "--base-port", str(base), "--reports", directory], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        origin = "http://127.0.0.1:" + str(base)
        for attempt in range(50):
            try:
                urllib.request.urlopen(origin, timeout=.2).read(); break
            except (OSError, urllib.error.URLError): time.sleep(.05)
        else: raise RuntimeError("Server did not start")
        for index in range(6):
            assert b"page.js" in urllib.request.urlopen("http://127.0.0.1:" + str(base + index), timeout=2).read()
        print("PASS six HTTP origins serve the fixture")
        token = "server-contract-token"
        report = {"schema": 1, "run": token, "phase": "erased", "origin": origin, "elapsed_ms": 1,
                  "total": 1, "passed": 1, "results": [{"name": CASES["erased"][0], "pass": True}]}
        def post(value):
            req = urllib.request.Request(origin + "/report?run=" + token + "&phase=erased", data=json.dumps(value).encode(), headers={"Content-Type": "application/json"})
            try: return urllib.request.urlopen(req, timeout=2).status
            except urllib.error.HTTPError as error: return error.code
        invalid = dict(report, run="stale-token")
        assert post(invalid) == 400
        print("PASS stale run identity rejected over HTTP")
        invalid = dict(report, results=[])
        assert post(invalid) == 400
        print("PASS incomplete case report rejected over HTTP")
        assert post(report) == 201
        saved = (Path(directory) / (token + "-erased.json")).read_bytes()
        assert post(report) == 400 and (Path(directory) / (token + "-erased.json")).read_bytes() == saved
        print("PASS report is immutable and duplicate cannot replace evidence")
        print("RESULT checks=4 failures=0")
    finally:
        process.terminate()
        try: process.wait(timeout=5)
        except subprocess.TimeoutExpired: process.kill(); process.wait()
