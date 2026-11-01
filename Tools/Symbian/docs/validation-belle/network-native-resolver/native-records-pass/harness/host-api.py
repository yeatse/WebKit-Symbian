#!/usr/bin/env python3
"""One controlled DNS host API call, only inside the executing private NS."""
import argparse,json,os,re,socket
p=argparse.ArgumentParser();p.add_argument('--execute',action='store_true');p.add_argument('--hostname',required=True);a=p.parse_args()
if not a.execute:print(json.dumps({'status':'PREPARED_NOT_EXECUTED'}));raise SystemExit(0)
if os.environ.get('SYMBIAN_DNS_PRIVATE_NS')!='1'or not re.fullmatch(r'dual-[a-z0-9-]{8,32}\.webkit-belle\.test',a.hostname):raise RuntimeError('private controlled DNS host required')
rows=socket.getaddrinfo(a.hostname,8830,socket.AF_UNSPEC,socket.SOCK_STREAM,socket.IPPROTO_TCP);result=[{'family':r[0],'socktype':r[1],'protocol':r[2],'address':r[4][0]}for r in rows];ok=any(r['family']==socket.AF_INET6 and r['address']=='::1'for r in result)and any(r['family']==socket.AF_INET and r['address']=='127.0.0.1'for r in result);print(json.dumps({'status':'PASS'if ok else'FAIL','hostname':a.hostname,'rows':result}));raise SystemExit(0 if ok else 1)
