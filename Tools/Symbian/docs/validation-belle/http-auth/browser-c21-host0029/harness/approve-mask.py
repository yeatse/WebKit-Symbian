#!/usr/bin/env python3
"""Root approves after viewing this public maskcheck FEP image."""
import argparse
from pathlib import Path
from mask_review import approve,require
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--gate',required=True,type=Path);p.add_argument('--pid',required=True,type=int)
p.add_argument('--image-sha256',required=True);p.add_argument('--masked',action='store_true')
a=p.parse_args();require(a.masked,'explicit --masked requires actual root image review')
instance=approve(a.gate,a.pid,a.image_sha256)
print('Approved public password probe instance '+str(instance)+'; private input follows only after clearing')
