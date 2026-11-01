#!/usr/bin/env python3
"""Pair real loopback DNS questions/answers with exact TLS peers and browser bytes."""
import argparse,hashlib,ipaddress,json,re
from pathlib import Path
def validate(server_dir,guest_log,token,mode='dns'):
 failures=[]
 def require(v,m):
  if not v:failures.append(m)
 try:
  d=Path(server_dir);ready=json.loads((d/'ready.json').read_text());s=json.loads((d/'server-state.json').read_text());label=token.replace('_','-').lower()[-16:];names={r:r+'-'+label+'.webkit-belle.test'for r in ['aaaa','dual','fallback']}
  require(mode=='dns'and bool(re.fullmatch(r'[A-Za-z0-9_-]{8,80}',token)),'mode/token invalid');require(ready['run']==s['run']==token and ready['mode']==s['mode']=='dns'and ready['suite']==s['suite']=='network-guest-c22-dns','fresh suite identity differs');require(ready['names']==s['names']==names,'controlled names differ');require(not s['errors'],'server reported functional/protocol errors')
  records=[]
  for line in Path(guest_log).read_text(errors='replace').splitlines():
   if 'console:'in line and 'NETWORK-GUEST-RESULT 'in line:
    try:v=json.loads(line.split('NETWORK-GUEST-RESULT ',1)[1])
    except ValueError:continue
    if isinstance(v,dict)and v.get('run')==token:records.append(v)
  begin=[r for r in records if r.get('kind')=='ready'];end=[r for r in records if r.get('kind')=='result'];require(len(begin)==1 and begin[0].get('status')=='READY','fresh browser ready absent/duplicate');require(len(end)==1,'one browser terminal result required')
  if len(end)==1:
   p=end[0];require(p.get('status')=='PASS'and p.get('suite')=='network-guest-c22-dns'and all(p.get(k)==4096 for k in ['aaaa_bytes','dual_bytes','fallback_bytes'])and all(p.get(k)is True for k in ['verified_all_bytes','aaaa_wss_echo','aaaa_wss_normal_close']),'browser did not verify fixed DNS-origin responses')
  for role in names:
   q=[r for r in s['dns']if r.get('role')==role];a=['127.0.0.1']if role!='aaaa'else[];aaaa=['::2']if role=='fallback'else['::1']
   for typ,answers in [('A',a),('AAAA',aaaa)]:require(any(r.get('name')==names[role]and r.get('qtype')==typ and r.get('answers')==answers and r.get('rcode')==0 and r.get('transport')in ['udp','tcp']for r in q),'actual DNS '+role+' '+typ+' query/answer absent')
  require(all(r.get('name')==names.get(r.get('role'))for r in s['dns']),'unexpected controlled DNS row');require(any(r.get('transport')=='udp'for r in s['dns']),'actual ordinary UDP DNS absent')
  c=s['h2_connections'];require(len(c)==1 and c[0]['alpn']=='h2'and c[0]['page_requests']==1 and c[0]['items']==[],'bootstrap identity/protocol differs')
  h=s['https'];require(len(h)==3 and sorted(r['role']for r in h)==['aaaa','dual','fallback'],'fixed HTTPS role set differs')
  for r in h:
   role=r['role'];expect='127.0.0.1'if role=='fallback'else'::1';body=bytes((j*13+len(role))%251 for j in range(4096));require(r['host']==names[role]and str(ipaddress.ip_address(r['peer']))==expect and r['alpn']=='http/1.1'and r['bytes']==4096 and r['sha256']==hashlib.sha256(body).hexdigest()and r['response_complete']is True,'actual '+role+' TLS peer/body differs')
  w=s['wss'];require(len(w)==1,'one AAAA WSS required')
  if len(w)==1:
   r=w[0];require(r['role']=='aaaa'and r['host']==names['aaaa']and str(ipaddress.ip_address(r['peer']))=='::1'and r['alpn']=='http/1.1'and all(r[k]is True for k in ['upgrade','dynamic_accept','client_masked','normal_close'])and r['echoes']==1 and r['bytes']==8 and r['sha256']==hashlib.sha256(b'dns-ping').hexdigest()and r['close_code']==1000,'real AAAA WSS Upgrade/echo/close differs')
  require(ready['unreachable_fallback_aaaa']=='::2'and ready['fallback_ipv4_only_listener']=='127.0.0.1:8831','refused6 fallback topology differs')
 except (OSError,ValueError,TypeError,KeyError)as e:failures.append(type(e).__name__+': missing/malformed evidence')
 return {'status':'FAIL'if failures else'PASS','suite':'network-guest-c22-dns','run':token,'failures':failures,'scope':'real controlled DNS AAAA-only HTTPS/WSS, dual IPv6 and unreachable-IPv6/A fallback; outer wrapper must verify ordinary closure/restoration; not public Internet/realdevice/scoped DNS/TCP-DNS runtime claim'}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('server_dir',type=Path);p.add_argument('guest_log',type=Path);p.add_argument('token');a=p.parse_args();r=validate(a.server_dir,a.guest_log,a.token);print(json.dumps(r,indent=2));raise SystemExit(0 if r['status']=='PASS'else 1)
