#!/usr/bin/env python3
"""Pair fresh page body verification with actual server TLS ALPN/stream proof."""
import argparse,hashlib,ipaddress,json,re
from pathlib import Path
LENGTHS=[524288]+[16384]*11;FALLBACK=b'HTTP1-ALPN-FALLBACK-C22'
def validate(server_dir,guest_log,token,mode='h2v6'):
 failures=[]
 def require(ok,message):
  if not ok:failures.append(message)
 try:
  root=Path(server_dir);ready=json.loads((root/'ready.json').read_text());s=json.loads((root/'server-state.json').read_text())
  require(mode=='h2v6' and bool(re.fullmatch(r'[A-Za-z0-9_-]{8,80}',token)),'invalid mode/token')
  require(ready['run']==s['run']==token and ready['mode']==s['mode']==mode,'fresh server identity differs')
  require(s['suite']==ready['suite']=='network-guest-c25-h2v6','server suite differs');require(not s['errors'],'server reported functional/protocol errors')
  records=[]
  for line in Path(guest_log).read_text(errors='replace').splitlines():
   if 'console:' in line and 'NETWORK-GUEST-RESULT ' in line:
    try:v=json.loads(line.split('NETWORK-GUEST-RESULT ',1)[1])
    except ValueError:continue
    if v.get('run')==token:records.append(v)
  beginnings=[x for x in records if x.get('kind')=='ready'];results=[x for x in records if x.get('kind')=='result']
  require(len(beginnings)==1 and beginnings[0].get('status')=='READY','fresh page READY absent/duplicate');require(len(results)==1,'exactly one terminal result required')
  if len(results)==1:
   p=results[0];require(p.get('status')=='PASS' and p.get('items')==12 and p.get('total_bytes')==sum(LENGTHS) and p.get('verified_all_bytes')is True,'page did not verify fixed bytes and H1 response')
  names=s['names'];require(ready['names']==names,'fresh DNS names differ')
  for typ,answers in [('A',[]),('AAAA',['::1'])]:require(any(r.get('role')=='aaaa'and r.get('name')==names['aaaa']and r.get('qtype')==typ and r.get('answers')==answers and r.get('rcode')==0 and r.get('transport')in ['udp','tcp']for r in s['dns']),'actual AAAA-only DNS '+typ+' absent')
  require(ready['url'].startswith('https://'+names['aaaa']+':8824/'),'initial URL must use actual AAAA hostname')
  connections=s['h2_connections'];require(len(connections)==1,'page/items must share exactly one TLS h2 connection')
  if len(connections)==1:
   c=connections[0];require(str(ipaddress.ip_address(c['peer']))=='::1','actual TLS peer must be IPv6 loopback');require(c['alpn']=='h2' and c['page_requests']==1,'page did not negotiate h2 exactly once');require(c['client_settings'].get('3')==8 and c['client_settings'].get('4')==262144 and c['client_settings'].get('2')==0,'actual peer settings differ from 8 streams/256KiB/no-push')
   require(2<=c['peak_active']<=8,'no actual multiplexing or stream ceiling exceeded');require(c['window_updates']>0,'large transfer lacks actual window updates');require(c['bytes_sent']==sum(LENGTHS) and sorted(c['complete_items'])==list(range(12)),'server did not complete exact item bytes')
   items=c['items'];require(len(items)==12 and sorted(x['index'] for x in items)==list(range(12)),'server item set differs/duplicates');require(len({x['stream'] for x in items})==12 and all(x['connection']==c['id'] for x in items),'item stream/connection identities differ')
   for x in items:
    i=x['index'];require(type(i)is int and 0<=i<12,'unknown item index')
    if type(i)is int and 0<=i<12:require(x['bytes']==LENGTHS[i] and x['sha256']==hashlib.sha256(bytes((i*17+j)%251 for j in range(LENGTHS[i]))).hexdigest(),'server payload identity differs')
 except (OSError,ValueError,TypeError,KeyError) as e:failures.append(type(e).__name__+': missing/malformed fresh evidence')
 return {'status':'FAIL' if failures else 'PASS','suite':'network-guest-c25-h2v6','run':token,'mode':mode,'failures':failures,'scope':'real controlled AAAA hostname IPv6 TLS h2 multiplex/flowcontrol; outer closure/restoration required, no ISP/scoped/race/budget exhaustion claim'}
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('server_dir',type=Path);p.add_argument('guest_log',type=Path);p.add_argument('token');p.add_argument('--mode',default='h2v6');a=p.parse_args();r=validate(a.server_dir,a.guest_log,a.token,a.mode);print(json.dumps(r,indent=2));raise SystemExit(0 if r['status']=='PASS' else 1)
