#!/usr/bin/env python3
"""Fresh IPv6-literal HTTPS/WSS proof; no AAAA or hostname-DNS claim."""
import argparse,hashlib,importlib.util,json,re
from pathlib import Path

def validate(server_dir,guest_log,token,mode='ipv4'):
 if mode=='ipv4':
  spec=importlib.util.spec_from_file_location('original_ipv4_validator',Path(__file__).parent/'validator_ipv4.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m.validate(server_dir,guest_log,token,mode)
 failures=[]
 def require(ok,message):
  if not ok:failures.append(message)
 try:
  root=Path(server_dir);ready=json.loads((root/'ready.json').read_text());s=json.loads((root/'server-state.json').read_text())
  require(mode=='literal6' and bool(re.fullmatch(r'[A-Za-z0-9_-]{8,80}',token)),'invalid literal6 mode/token')
  require(ready['run']==s['run']==token and ready['mode']==s['mode']==mode,'fresh server identity differs')
  require(s['suite']==ready['suite']=='network-guest-c22-literal6','literal suite differs');require(not s['errors'],'server functional/protocol errors')
  records=[]
  for line in Path(guest_log).read_text(errors='replace').splitlines():
   if 'console:' in line and 'NETWORK-GUEST-RESULT ' in line:
    try:v=json.loads(line.split('NETWORK-GUEST-RESULT ',1)[1])
    except ValueError:continue
    if isinstance(v,dict) and v.get('run')==token:records.append(v)
  beginnings=[x for x in records if x.get('kind')=='ready'];results=[x for x in records if x.get('kind')=='result']
  require(len(beginnings)==1 and beginnings[0].get('status')=='READY' and beginnings[0].get('suite')=='network-guest-c22-literal6','fresh literal page READY absent/duplicate')
  require(len(results)==1,'exactly one terminal literal result required')
  if len(results)==1:
   p=results[0];require(p.get('status')=='PASS' and p.get('suite')=='network-guest-c22-literal6' and p.get('https_bytes')==4096 and p.get('verified_all_bytes')is True and p.get('wss_echo')is True and p.get('wss_normal_close')is True,'page did not verify literal bytes/WSS/normal close')
  connections=s['h2_connections'];require(len(connections)==1,'literal page requires exactly one h2 bootstrap connection')
  if len(connections)==1:
   c=connections[0];require(c['alpn']=='h2' and c['page_requests']==1 and not c['items'] and not c['complete_items'] and c['bytes_sent']==0,'h2 bootstrap identity differs or unexpected item transfers')
  require(not s['h1_requests'],'literal mode unexpectedly used IPv4 H1 fallback')
  literal=s['literal6'];https=literal['https'];wss=literal['wss'];require(len(https)==1,'exactly one IPv6 literal HTTPS response required');require(len(wss)==1,'exactly one IPv6 WSS connection required')
  if len(https)==1:
   expected={'run':token,'peer':'::1','alpn':'http/1.1','path':'/literal','bytes':4096,'sha256':hashlib.sha256(bytes((j*7+3)%251 for j in range(4096))).hexdigest(),'response_complete':True};require(https[0]==expected,'actual literal HTTPS peer/ALPN/body proof differs')
  if len(wss)==1:
   expected={'run':token,'peer':'::1','alpn':'http/1.1','path':'/socket','upgrade':True,'dynamic_accept':True,'echoes':1,'bytes':4,'sha256':hashlib.sha256(b'ping').hexdigest(),'client_masked':True,'normal_close':True,'close_code':1000};require(wss[0]==expected,'actual IPv6 WSS Upgrade/echo/normal-close proof differs')
 except (OSError,ValueError,TypeError,KeyError) as e:failures.append(type(e).__name__+': missing/malformed fresh evidence')
 return {'status':'FAIL' if failures else 'PASS','suite':'network-guest-c22-literal6','run':token,'mode':mode,'failures':failures,'scope':'IPv6 literal loopback HTTPS/WSS plus IPv4 h2 bootstrap only; outer wrapper owns shutdown/restoration; no AAAA DNS/fallback/cancel/budget-exhaustion/real-device claim'}
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('server_dir',type=Path);p.add_argument('guest_log',type=Path);p.add_argument('token');p.add_argument('--mode',default='ipv4');a=p.parse_args();r=validate(a.server_dir,a.guest_log,a.token,a.mode);print(json.dumps(r,indent=2));raise SystemExit(0 if r['status']=='PASS' else 1)
