#!/usr/bin/env python3
"""Prepare by default; execute root's single dedicated DNS/EKA namespace."""
import argparse,json,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--execute',action='store_true');p.add_argument('--output',required=True,type=Path);a=p.parse_args()
 if not a.execute:return subprocess.call([sys.executable,'-B',str(ROOT/'run.py'),'--mode','h2v6','--output',str(a.output)])
 if not a.output.is_absolute()or a.output.exists():raise RuntimeError('fresh absolute output required')
 if subprocess.run(['pgrep','-x','eka2l1_qt'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode!=1:raise RuntimeError('EKA already running')
 os.environ['SYMBIAN_DNS_OUTER_NS']=json.dumps({n:os.readlink('/proc/self/ns/'+n)for n in ['user','net','mnt','pid']});os.execvp('unshare',['unshare','--user','--map-root-user','--mount','--net','--propagation','private',sys.executable,'-B',str(ROOT/'namespace-inner.py'),'--output',str(a.output)])
if __name__=='__main__':raise SystemExit(main())
