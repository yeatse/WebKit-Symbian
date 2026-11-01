"""Four fixed ordinary API results; functional failures remain failures."""
import json
NAMES=['aaaa-native-records','dual-native-records','fallback-native-records','numeric-native-records']
def validate(data,token,mode=None):
 lines=data.decode(errors='replace').splitlines();begin=[json.loads(l[6:])for l in lines if l.startswith('BEGIN ')];cases=[json.loads(l[5:])for l in lines if l.startswith('CASE ')];end=[json.loads(l[7:])for l in lines if l.startswith('RESULT ')];ok=len(begin)==1 and begin[0].get('run')==token and len(end)==1 and end[0].get('run')==token and [r.get('name')for r in cases]==NAMES
 if not ok:return {'status':'FAIL','cases':len(cases),'error':'fresh fixed four-case log contract absent'}
 total=end[0];ok=total.get('total')==4 and total.get('passed')==sum(c.get('pass')is True for c in cases)and total.get('failed')==sum(c.get('pass')is False for c in cases)
 passed=ok and total['failed']==0 and lines[-1]=='dns-native-contract exiting 0'
 return {'status':'LOG_CONTRACT_PASS_PENDING_TARGET_EXIT'if passed else'FUNCTIONAL_FAIL','cases':4,'case_results':cases,'summary':total}
