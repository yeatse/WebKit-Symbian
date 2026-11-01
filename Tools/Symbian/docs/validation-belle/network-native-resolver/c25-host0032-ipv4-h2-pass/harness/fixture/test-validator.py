#!/usr/bin/env python3
"""Synthetic validator-only guards; no sockets/TLS/guest/display."""
import hashlib,importlib.util,json,tempfile
from pathlib import Path
p=Path(__file__).parent/'validator.py';s=importlib.util.spec_from_file_location('validator',p);v=importlib.util.module_from_spec(s);s.loader.exec_module(v)
token='synthetic-c22-validator-only'
def baseline():
 items=[{'index':i,'stream':1+2*i,'connection':1,'bytes':v.LENGTHS[i],'sha256':hashlib.sha256(bytes((i*17+j)%251 for j in range(v.LENGTHS[i]))).hexdigest()} for i in range(12)]
 return {'suite':'network-guest-c22-ipv4','run':token,'mode':'ipv4','errors':[],'h2_connections':[{'id':1,'alpn':'h2','page_requests':1,'client_settings':{'3':8,'4':262144,'2':0},'peak_active':8,'window_updates':5,'bytes_sent':sum(v.LENGTHS),'complete_items':list(range(12)),'items':items}],'h1_requests':[{'run':token,'alpn':'http/1.1','path':'/fallback','bytes':len(v.FALLBACK),'sha256':hashlib.sha256(v.FALLBACK).hexdigest()}]}
def check(name,change,expected):
 state=baseline();result={'run':token,'kind':'result','status':'PASS','items':12,'total_bytes':sum(v.LENGTHS),'h1_fallback':True,'verified_all_bytes':True};params={'token':token};change(state,result,params)
 with tempfile.TemporaryDirectory(prefix='c22-validator-only-') as d:
  p=Path(d);(p/'ready.json').write_text(json.dumps({'suite':'network-guest-c22-ipv4','run':token,'mode':'ipv4'}));(p/'server-state.json').write_text(json.dumps(state));records=[{'run':token,'kind':'ready','status':'READY'},result];(p/'guest.log').write_text('\n'.join('console: validator-only: NETWORK-GUEST-RESULT '+json.dumps(x) for x in records));actual=v.validate(p,p/'guest.log',params['token'])['status'];assert actual==expected,(name,actual);print(name,actual,'expected',expected)
check('synthetic-only-baseline',lambda s,r,p:None,'PASS')
check('stale-token',lambda s,r,p:p.update(token='other-c22-token'),'FAIL')
check('H1-on-h2',lambda s,r,p:s['h2_connections'][0].update(alpn='http/1.1'),'FAIL')
check('duplicate-stream',lambda s,r,p:s['h2_connections'][0]['items'][1].update(stream=1),'FAIL')
check('wrong-bytes',lambda s,r,p:s['h2_connections'][0].update(bytes_sent=1),'FAIL')
check('no-window-update',lambda s,r,p:s['h2_connections'][0].update(window_updates=0),'FAIL')
check('no-multiplexing',lambda s,r,p:s['h2_connections'][0].update(peak_active=1),'FAIL')
check('wrong-H1-ALPN',lambda s,r,p:s['h1_requests'][0].update(alpn=None),'FAIL')
check('page-fail',lambda s,r,p:r.update(status='FAIL'),'FAIL')
check('server-error',lambda s,r,p:s['errors'].append('synthetic error'),'FAIL')
print('10 synthetic validator-only guards PASS; no runtime/network')
