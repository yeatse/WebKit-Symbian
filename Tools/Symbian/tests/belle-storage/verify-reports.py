#!/usr/bin/env python3
"""Check all three page reports. Host exit/panic checks remain runner duties."""
import argparse
import json
from pathlib import Path
from contract import SUITES, validate

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("reports", type=Path)
parser.add_argument("run")
parser.add_argument("--suite", choices=tuple(SUITES), default="storage")
parser.add_argument("--origin")
args = parser.parse_args()
args.origin = args.origin or ("http://localhost:8780" if args.suite == "storage" else "http://localhost:8784")
for phase in SUITES[args.suite]:
    path = args.reports / (args.run + "-" + phase + ".json")
    report = json.loads(path.read_text())
    validate(report, args.run, phase, args.origin, args.suite)
    print(f"PASS {phase}: {len(SUITES[args.suite][phase])}/{len(SUITES[args.suite][phase])}")
