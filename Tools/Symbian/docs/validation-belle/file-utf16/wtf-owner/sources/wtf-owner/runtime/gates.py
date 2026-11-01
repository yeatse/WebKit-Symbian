import re
FATAL=re.compile(r'access violation|\bpanicked\b|KERN-EXEC|USER-EXEC|segmentation fault|\bSIGSEGV\b|\bSIGABRT\b|\bfatal\b|Main loop exited with exception|Unable to launch process',re.I)
def require(v,m):
 if not v:raise RuntimeError(m)
def completion_gate(result,host,guest,validator):
 require(not FATAL.search(host.decode('utf-8',errors='replace'))and not FATAL.search(guest.decode('utf-8',errors='replace')),'host/guest fatal marker')
 report=validator.validate(guest,result['run']);require(report.get('complete')is True and report.get('cases')==3,'complete fresh three-case/ABI/exit record required')
 require(type(result.get('host_exit'))is int and result['host_exit']==0 and result.get('natural_host_exit')is True,'natural CLI host0 required')
 require(result.get('forced_cleanup')is False and result.get('wm_close_sent')is False and result.get('deadline_expired')is False,'cleanup/deadline cannot pass')
 require(result['restoration']['guest'].get('restored')is True and result['restoration']['configuration'].get('restored')is True and result.get('inputs_unchanged')is True,'whole original restoration/pins required')
 text=host.decode('utf-8',errors='replace');records=re.findall(r'\[Kernel\]: Thread Main forcefully killed with category: (Kill|None|) and exit code: (-?\d+)',text);require(records and any(int(code)==report['worker_exit']for _,code in records),'positive Main exit must match worker explicit exit')
 for line in text.splitlines():
  m=re.search(r'Thread (.*?) (?:forcefully killed|terminated peacefully) with category: (.*?) and exit code: (-?\d+)',line)
  if m and int(m.group(3)):require(m.group(1)=='Main'and m.group(2)in('Kill','None','')and int(m.group(3))==report['worker_exit']==1,'nonzero thread exit is not the completed worker assertion exit1')
 result['normal_worker_completion']=True;result['api_assertions_failed']=bool(report['failed']);result['comparison']=report
 require(not report['failed'],'complete normal API measurement: assertion failures; worker explicit exit1, not crash or PASS')
 return{'status':'PASS','normal_worker_completion':True,'fixture':report,'scope':'fixed three public files through actual WTF FileSystem owner APIs; actual metadata/bytes/SHA and normal exit/restoration'}
