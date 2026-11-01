"""Observer verdicts for the explicit WebKit target, without broad panic scanning."""
import re
from parent_probe import validate_closure
TARGET_PANIC=re.compile(r'\bThread\s+(?P<thread>WebKit)\s+panicked\s+with\s+category:\s*(?P<category>[^\r\n]+?)\s+and\s+exit\s+code:\s*(?P<code>-?[0-9]+)\b',re.I)
def target_thread_failures(lines):
 result=[]
 for number,line in enumerate(lines,1):
  match=TARGET_PANIC.search(line)
  if match:result.append({'line':number,'thread':'WebKit','category':match.group('category').strip(),'exit_code':int(match.group('code'))})
 return result

def driver_contract(observation,interaction,public_probe=None):
 if observation.get('calibration_only'):
  return {'status':'CALIBRATION_NOT_RUN','failures':[],'scope':'Public layout observation only; no authentication PASS'}
 failures=[];case=observation['case'];expected='EXIT_REQUEST_SENT_PENDING_VALIDATION' if case.startswith('exit') else 'INTERACTION_COMPLETE_PENDING_VALIDATION'
 if observation.get('input_exit')!=0:failures.append('native driver exit was not zero')
 if not isinstance(interaction,dict):failures.append('fresh native interaction record is absent');return {'status':'FAIL','failures':failures}
 if any(interaction.get(k)!=observation.get(k) for k in ('run','scheme','case')):failures.append('native interaction identity differs')
 if interaction.get('status')!=expected:failures.append('native interaction did not complete expected case')
 if interaction.get('credential_stage_screenshots') is not False or interaction.get('public_probe_only_screenshots') is not True:failures.append('credential-stage screenshot prohibition not recorded')
 expected_probes=3 if case=='retry' else 1 if case=='success' or case.endswith('password') else 0
 if (not isinstance(public_probe,dict) or public_probe.get('status')!='PASS'
     or public_probe.get('expected')!=expected_probes or public_probe.get('approved')!=expected_probes
     or interaction.get('screenshots_taken') is not True
     or not isinstance(interaction.get('password_mask_reviews'),list)
     or len(interaction['password_mask_reviews'])!=expected_probes):failures.append('current public password probe reviews not complete')
 failures+=validate_closure(interaction)
 if not isinstance(interaction.get('heading_reviews'),list) or len(interaction['heading_reviews'])!=1:failures.append('current native heading review absent')
 records=interaction.get('records',[]);events=[r.get('event') for r in records if isinstance(r,dict)]
 if not case.startswith('exit'):
  quiet=[r for r in records if isinstance(r,dict) and r.get('event')=='quiet-observation']
  if len(quiet)!=1 or type(quiet[0].get('seconds')) not in (int,float) or quiet[0]['seconds']<3:failures.append('required quiet observation absent')
 if case=='success' and events.count('credential-submitted')!=1:failures.append('expected one actual credential submission')
 if case=='retry' and [r.get('attempt') for r in records if isinstance(r,dict) and r.get('event')=='credential-submitted']!=[1,2,3]:failures.append('retry inputs do not match exactly attempts 1/2/3')
 if case.startswith('cancel') and events.count('native-cancel')!=1:failures.append('native Cancel absent')
 if case.startswith('navigate') and events.count('navigation-requested')!=1:failures.append('normal navigation request absent')
 if case.startswith('exit') and events.count('normal-exit-helper-launch')!=1:failures.append('official helper launch absent')
 return {'status':'FAIL' if failures else 'PENDING_STRICT_HTTP_AND_UI_VALIDATOR','failures':failures,'scope':'Input completion only; original HTTP/UI validator still required'}

def close_contract(host_exit_code,forced_cleanup,failures):
 host_normal=host_exit_code==0 and not forced_cleanup
 return {'normal_host_close':host_normal,'target_guest_failed':bool(failures),'normal_close':host_normal and not failures}
