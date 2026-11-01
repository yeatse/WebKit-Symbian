#!/usr/bin/env python3
"""Owned process entry: private lo and resolver mount; never a host mutation."""
import argparse,hashlib,json,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent;CONF=Path('/workspace/symbian-network-h2v6-c25-work/resolv.fixture.conf')
def main():
 p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=Path);a=p.parse_args();outer=json.loads(os.environ['SYMBIAN_DNS_OUTER_NS']);proof={n:os.readlink('/proc/self/ns/'+n)for n in ['user','net','mnt','pid']};proof.update({'outer_'+n:v for n,v in outer.items()})
 if not all(proof[n]!=proof['outer_'+n]for n in ['user','net','mnt'])or proof['pid']!=proof['outer_pid']:raise RuntimeError('dedicated user/net/mount and shared PID namespaces required')
 subprocess.run(['/usr/sbin/ip','link','set','lo','up'],check=True);subprocess.run(['/usr/bin/mount','--bind',str(CONF),'/etc/resolv.conf'],check=True);subprocess.run(['/usr/bin/mount','-o','remount,bind,ro','/etc/resolv.conf'],check=True)
 expected=hashlib.sha256(CONF.read_bytes()).hexdigest()
 if hashlib.sha256(Path('/etc/resolv.conf').read_bytes()).hexdigest()!=expected:raise RuntimeError('private resolver mount fingerprint differs')
 proof.update(resolver_fixture_sha256=expected,host_resolver_read=False,host_resolver_changed=False,loopback_up=True);os.environ['SYMBIAN_DNS_PRIVATE_NS']='1';os.environ['SYMBIAN_DNS_NS_PROOF']=json.dumps(proof);os.execv(sys.executable,[sys.executable,'-B',str(ROOT/'run.py'),'--execute','--mode','h2v6','--output',str(a.output)])
if __name__=='__main__':main()
