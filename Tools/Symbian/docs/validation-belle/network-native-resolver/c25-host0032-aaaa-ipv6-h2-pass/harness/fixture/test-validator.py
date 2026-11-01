"""Labeled synthetic only; never changes an actual result or starts networking."""
import copy,json,tempfile,unittest
from pathlib import Path
from validator import validate
class Guards(unittest.TestCase):
 def setUp(self):
  old=json.loads(Path('/workspace/network-c25-ipv4-host0032-run-01/server/server-state.json').read_text());self.s=copy.deepcopy(old);self.s.update(run='synthetic-h2v6-0001',mode='h2v6',suite='network-guest-c25-h2v6',names={'aaaa':'aaaa-0123456789abcdef.webkit-belle.test'},dns=[{'role':'aaaa','name':'aaaa-0123456789abcdef.webkit-belle.test','qtype':t,'answers':a,'rcode':0,'transport':'udp'}for t,a in [('A',[]),('AAAA',['::1'])]]);self.s['h2_connections'][0]['peer']='::1';self.page={'kind':'result','run':self.s['run'],'status':'PASS','items':12,'total_bytes':704512,'verified_all_bytes':True}
 def check(self,status):
  with tempfile.TemporaryDirectory()as d:
   p=Path(d);(p/'server-state.json').write_text(json.dumps(self.s));ready={'run':self.s['run'],'mode':'h2v6','suite':'network-guest-c25-h2v6','names':self.s['names'],'url':'https://'+self.s['names']['aaaa']+':8824/?run='+self.s['run']};(p/'ready.json').write_text(json.dumps(ready));(p/'guest.log').write_text('console: NETWORK-GUEST-RESULT '+json.dumps({'run':self.s['run'],'kind':'ready','status':'READY'})+'\nconsole: NETWORK-GUEST-RESULT '+json.dumps(self.page)+'\n');self.assertEqual(validate(p,p/'guest.log',self.s['run'],'h2v6')['status'],status)
 def test_synthetic_complete(self):self.check('PASS')
 def test_peer4_fails(self):self.s['h2_connections'][0]['peer']='127.0.0.1';self.check('FAIL')
 def test_no_dns_fails(self):self.s['dns']=[];self.check('FAIL')
 def test_stream_limit_fails(self):self.s['h2_connections'][0]['peak_active']=9;self.check('FAIL')
 def test_no_multiplex_fails(self):self.s['h2_connections'][0]['peak_active']=1;self.check('FAIL')
 def test_no_flow_updates_fails(self):self.s['h2_connections'][0]['window_updates']=0;self.check('FAIL')
 def test_no_fixed_bytes_fails(self):self.page['verified_all_bytes']=False;self.check('FAIL')
 def test_window_changed_fails(self):self.s['h2_connections'][0]['client_settings']['4']=1;self.check('FAIL')
if __name__=='__main__':unittest.main()
