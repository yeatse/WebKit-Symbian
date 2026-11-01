#!/usr/bin/env python3
"""Synthetic validator-only literal6 guards; no socket/TLS/guest/display."""
import hashlib,importlib.util,json,tempfile
from pathlib import Path
p=Path(__file__).parent/'validator.py';s=importlib.util.spec_from_file_location('validator',p);v=importlib.util.module_from_spec(s);s.loader.exec_module(v);token='synthetic-c22-literal6-only'
def baseline():
 return {'suite':'network-guest-c22-literal6','run':token,'mode':'literal6','errors':[],'h2_connections':[{'alpn':'h2','page_requests':1,'items':[],'complete_items':[],'bytes_sent':0}],'h1_requests':[],'literal6':{'https':[{'run':token,'peer':'::1','alpn':'http/1.1','path':'/literal','bytes':4096,'sha256':hashlib.sha256(bytes((j*7+3)%251 for j in range(4096))).hexdigest(),'response_complete':True}],'wss':[{'run':token,'peer':'::1','alpn':'http/1.1','path':'/socket','upgrade':True,'dynamic_accept':True,'echoes':1,'bytes':4,'sha256':hashlib.sha256(b'ping').hexdigest(),'client_masked':True,'normal_close':True,'close_code':1000}]}}
def check(name,change,expected):
 state=baseline();result={'run':token,'kind':'result','status':'PASS','suite':'network-guest-c22-literal6','https_bytes':4096,'verified_all_bytes':True,'wss_echo':True,'wss_normal_close':True};params={'token':token};change(state,result,params)
 with tempfile.TemporaryDirectory(prefix='c22-literal6-validator-only-') as d:
  p=Path(d);(p/'ready.json').write_text(json.dumps({'suite':'network-guest-c22-literal6','run':token,'mode':'literal6'}));(p/'server-state.json').write_text(json.dumps(state));records=[{'run':token,'kind':'ready','status':'READY','suite':'network-guest-c22-literal6'},result];(p/'guest.log').write_text('\n'.join('console: synthetic-only: NETWORK-GUEST-RESULT '+json.dumps(x) for x in records));actual=v.validate(p,p/'guest.log',params['token'],'literal6')['status'];assert actual==expected,(name,actual);print(name,actual,'expected',expected)
check('synthetic-only-baseline',lambda s,r,p:None,'PASS')
check('stale-token',lambda s,r,p:p.update(token='other-c22-literal'),'FAIL')
check('wrong-IP-peer',lambda s,r,p:s['literal6']['https'][0].update(peer='127.0.0.1'),'FAIL')
check('wrong-HTTPS-bytes',lambda s,r,p:s['literal6']['https'][0].update(bytes=4095),'FAIL')
check('wrong-WSS-ALPN',lambda s,r,p:s['literal6']['wss'][0].update(alpn='h2'),'FAIL')
check('missing-dynamic-accept',lambda s,r,p:s['literal6']['wss'][0].update(dynamic_accept=False),'FAIL')
check('wrong-WSS-message',lambda s,r,p:s['literal6']['wss'][0].update(sha256='0'*64),'FAIL')
check('missing-normal-close',lambda s,r,p:s['literal6']['wss'][0].update(normal_close=False),'FAIL')
check('page-fail',lambda s,r,p:r.update(status='FAIL'),'FAIL')
check('server-error',lambda s,r,p:s['errors'].append('synthetic error'),'FAIL')
print('10 synthetic literal6 guards PASS; no runtime/network')
