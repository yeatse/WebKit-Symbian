#!/usr/bin/env python3
"""Pass the source-declared ECom plugin identity to the native converter."""
import json
import os
from pathlib import Path
import sys

config = json.loads((Path(__file__).resolve().parent / 'build-config.json').read_text())
args = sys.argv[1:]
if args and args[0] == 'convert-dll':
    if '--uid3' not in args or int(args[args.index('--uid3') + 1], 0) != 0x100056DE:
        raise SystemExit('This launcher is for the declared general FEP target only')
    args[1:1] = ['--uid2', '0x10009d8d']
os.execv(config['converter'], [config['converter'], *args])
