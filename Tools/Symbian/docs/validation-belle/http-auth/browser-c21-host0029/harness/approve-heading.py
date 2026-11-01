#!/usr/bin/env python3
"""Publish explicit Root review of the exact empty native query image."""
import argparse
from pathlib import Path
from heading_review import approve,require
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--gate',type=Path,required=True);p.add_argument('--pid',type=int,required=True);p.add_argument('--image-sha256',required=True);p.add_argument('--origin',required=True);p.add_argument('--http-sign-in-heading',action='store_true',required=True);p.add_argument('--empty-username',action='store_true',required=True);a=p.parse_args();approve(a.gate,a.pid,a.image_sha256,a.origin);print('APPROVED exact empty native heading image')
