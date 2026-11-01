#!/usr/bin/env python3
"""Pure normal DNS wire/paired-validator guards; no sockets/listeners/keys."""
import copy,hashlib,json,struct,tempfile,unittest
from pathlib import Path
from dns_protocol import reply,names_for
from validator import validate
TOKEN='dns-synthetic-12345678';NAMES=names_for(TOKEN)
def query(name,t):return struct.pack('!6H',1234,0x0100,1,0,0,0)+b''.join(bytes([len(l)])+l.encode()for l in name.split('.'))+b'\0'+struct.pack('!HH',t,1)
class DNS(unittest.TestCase):
 def test_normal_answers(self):
  for r in NAMES:
   for t in [1,28]:
    out,row=reply(query(NAMES[r],t),NAMES,'udp');expected=['::2']if r=='fallback'and t==28 else['::1']if t==28 else[]if r=='aaaa'else['127.0.0.1'];self.assertEqual(row['answers'],expected);self.assertEqual(row['rcode'],0);self.assertEqual(struct.unpack('!6H',out[:12])[3],len(expected))
 def test_tcp_same_dns_message(self):self.assertEqual(reply(query(NAMES['aaaa'],28),NAMES,'tcp')[1]['answers'],['::1'])
 def test_unknown_name_redacted(self):self.assertIsNone(reply(query('outside.invalid',1),NAMES,'udp')[1]['name'])
 def setUp(self):
  self.s={'suite':'network-guest-c22-dns','run':TOKEN,'mode':'dns','names':NAMES,'dns':[reply(query(NAMES[r],t),NAMES,'udp')[1]for r in NAMES for t in [1,28]],'h2_connections':[{'alpn':'h2','page_requests':1,'items':[]}],'https':[{'role':r,'host':NAMES[r],'peer':'127.0.0.1'if r=='fallback'else'::1','alpn':'http/1.1','bytes':4096,'sha256':hashlib.sha256(bytes((j*13+len(r))%251 for j in range(4096))).hexdigest(),'response_complete':True}for r in NAMES],'wss':[{'role':'aaaa','host':NAMES['aaaa'],'peer':'::1','alpn':'http/1.1','upgrade':True,'dynamic_accept':True,'client_masked':True,'echoes':1,'bytes':8,'sha256':hashlib.sha256(b'dns-ping').hexdigest(),'normal_close':True,'close_code':1000}],'errors':[],'unreachable_fallback_aaaa':'::2','fallback_ipv4_only_listener':'127.0.0.1:8831'}
  self.p={'run':TOKEN,'kind':'result','status':'PASS','suite':'network-guest-c22-dns','aaaa_bytes':4096,'dual_bytes':4096,'fallback_bytes':4096,'verified_all_bytes':True,'aaaa_wss_echo':True,'aaaa_wss_normal_close':True}
 def check(self,status):
  with tempfile.TemporaryDirectory()as d:
   p=Path(d);(p/'ready.json').write_text(json.dumps(self.s));(p/'server-state.json').write_text(json.dumps(self.s));(p/'guest.log').write_text('console: NETWORK-GUEST-RESULT '+json.dumps({'run':TOKEN,'kind':'ready','status':'READY'})+'\nconsole: NETWORK-GUEST-RESULT '+json.dumps(self.p)+'\n');self.assertEqual(validate(p,p/'guest.log',TOKEN)['status'],status)
 def test_complete_synthetic(self):self.check('PASS')
 def test_missing_real_dns(self):self.s['dns']=[];self.check('FAIL')
 def test_wrong_aaaa_answer(self):self.s['dns'][1]['answers']=['127.0.0.1'];self.check('FAIL')
 def test_fallback_peer_wrong(self):self.s['https'][2]['peer']='::1';self.check('FAIL')
 def test_dual_v4_not_ipv6(self):self.s['https'][1]['peer']='127.0.0.1';self.check('FAIL')
 def test_wss_close_missing(self):self.s['wss'][0]['normal_close']=False;self.check('FAIL')
 def test_bytes_fail(self):self.p['verified_all_bytes']=False;self.check('FAIL')
if __name__=='__main__':unittest.main()
