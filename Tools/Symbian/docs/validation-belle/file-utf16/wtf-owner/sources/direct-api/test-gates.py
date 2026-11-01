# Synthetic records only; no guest/server/display/runtime.
import sys,json,copy
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent));sys.path.insert(0,str(Path(__file__).parent/'runtime'))
import validate,gates
checks=0
def check(v):
 global checks;checks+=1;assert v
run='validator-only-file'
def record(failure=False):
 lines=['BEGIN '+json.dumps({'suite':'file-io-contract','version':1,'run':run}),'ABI '+json.dumps({'wchar':2,'wchar_align':2,'TText':2,'stat':96,'off_t':8,'st_size':8,'unicode_units':[23383,55357,56832]})]
 for i in range(3):
  for api in ['native','narrow','wide']:
   r={'api':api,'file':i,'pass':True,'metadata_rc':0,'metadata_errno':0,'open_rc':0,'open_errno':0,'read_rc':0,'read_errno':0,'close_rc':0,'size':validate.SIZES[i],'bytes':validate.SIZES[i],'regular':True,'exact':True,'hash_ok':True,'sha256':validate.HASHES[i]}
   if failure and i==2 and api=='narrow':r.update({'pass':False},metadata_rc=-1,metadata_errno=2,open_rc=-1,open_errno=2,read_rc=-999,close_rc=-999,size=-1,bytes=0,regular=False,exact=False,hash_ok=False,sha256='')
   lines.append('CASE '+json.dumps(r))
 lines+=['RESULT '+json.dumps({'suite':'file-io-contract','version':1,'run':run,'passed':8 if failure else 9,'failed':1 if failure else 0,'total':9}),'file-io-contract exiting '+str(int(failure))];return ('\n'.join(lines)+'\n').encode()
good={'run':run,'host_exit':0,'natural_host_exit':True,'forced_cleanup':False,'wm_close_sent':False,'deadline_expired':False,'inputs_unchanged':True,'restoration':{'guest':{'restored':True},'configuration':{'restored':True}}}
zero=b'[Kernel]: Thread Main forcefully killed with category: None and exit code: 0\n';one=zero.replace(b'code: 0',b'code: 1')
check(validate.validate(record(),run)['passed']==9);check(validate.validate(record(True),run)['failed']==1)
check(gates.completion_gate(copy.deepcopy(good),zero,record(),validate)['status']=='PASS')
r=copy.deepcopy(good)
try:gates.completion_gate(r,one,record(True),validate);check(False)
except RuntimeError:check(r['normal_worker_completion']is True and r['api_assertions_failed']is True)
for host,body in [(b'WebKit panicked',record()),(one,record()),(zero,record(True)),(zero+b'Thread Unknown forcefully killed with category: None and exit code: 1',record()),(b'',record()),(zero,record()[:-20])]:
 try:gates.completion_gate(copy.deepcopy(good),host,body,validate);check(False)
 except RuntimeError:check(True)
for key,value in [('host_exit',1),('forced_cleanup',True),('deadline_expired',True),('wm_close_sent',True),('inputs_unchanged',False)]:
 r=copy.deepcopy(good);r[key]=value
 try:gates.completion_gate(r,zero,record(),validate);check(False)
 except RuntimeError:check(True)
check(validate.validate(record(),'stale')['status']=='FAIL');print(json.dumps({'status':'PASS','synthetic_checks':checks,'guest_started':False}))
