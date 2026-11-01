"""Real input into a top-parent button, with console-only proof and no HTTP."""
import json,re
from mask_review import require
KEYS={'run','source','page_path','x','y','width','height'}
def console_records(path,label,run):
 out=[]
 for line in path.read_text(errors='replace').splitlines():
  if 'console:' not in line or label+' ' not in line:continue
  raw=line.split(label+' ',1)[1]
  try:v=json.loads(raw)
  except (ValueError,TypeError):continue
  if isinstance(v,dict) and v.get('run')==run:out.append(v)
 return out

def perform(driver):
 driver.main_focus_acceptable();require(not driver.x.fep_window(),'native FEP still present before parent input')
 path='/landed' if driver.ready['case'].startswith('navigate') else '/'
 records=console_records(driver.args.guest_log,'AUTH-PARENT-PROBE-READY',driver.ready['run']);records=[p for p in records if p.get('page_path')==path]
 require(records,'current parent probe geometry absent');v=records[-1];require(set(v)==KEYS and v['source']=='top-parent' and all(type(v[k])in (int,float) for k in ('x','y','width','height')) and v['width']>0 and v['height']>0 and 0<v['x']<v['width'] and 0<v['y']<v['height'],'parent probe metrics invalid')
 rect=driver.ui['content_rect'];x=round(rect[0]+rect[2]*v['x']/v['width']);y=round(rect[1]+rect[3]*v['y']/v['height']);before=console_records(driver.args.guest_log,'AUTH-PARENT-PROBE-CLICK',driver.ready['run']);require(not before,'parent probe already clicked; fresh run required');driver.x.click(x,y);driver.record('trusted-parent-probe-clicked',point=[x,y],page_path=path)
 def received():
  found=console_records(driver.args.guest_log,'AUTH-PARENT-PROBE-CLICK',driver.ready['run'])
  if not found:return None
  require(len(found)==1 and set(found[0])==KEYS|{'trusted'} and found[0]==dict(v,trusted=True),'parent callback is untrusted, stale, or changed');return found[0]
 driver.wait(received,'real trusted top-parent response');require(not driver.x.fep_window(),'native editor returned after parent input')
 proof=dict(kind='trusted-parent-button',run=driver.ready['run'],pid=driver.args.pid,window=hex(driver.x.window),geometry=driver.x.check(),fep_absent=True,trusted=True,source='top-parent',page_path=path,recorded_after_interaction=True)
 driver.output['native_query_closure']=proof;driver.record('trusted-parent-probe',**proof)

CLOSURE_KEYS={'kind','run','pid','window','geometry','fep_absent','trusted','source','page_path','recorded_after_interaction'}
def validate_closure(interaction,guest=None):
 if interaction.get('case','').startswith('exit'):return []
 p=interaction.get('native_query_closure');case=interaction.get('case','');path='/landed' if case.startswith('navigate') else '/'
 try:
  require(isinstance(p,dict) and set(p)==CLOSURE_KEYS,'closure proof shape changed');require(p['kind']=='trusted-parent-button' and p['run']==interaction.get('run') and p['pid']==interaction.get('pid') and type(p['pid'])is int and p['pid']>0 and re.fullmatch(r'0x[0-9a-f]+',p['window']),'closure proof identity differs')
  geom=p['geometry'];require(isinstance(geom,list) and len(geom)==4 and all(type(n)is int for n in geom) and geom[2]>0 and geom[3]>0,'closure geometry invalid');require(all(p[k]is True for k in ('fep_absent','trusted','recorded_after_interaction')) and p['source']=='top-parent' and p['page_path']==path,'closure lacks actual trusted parent input')
  records=interaction.get('records',[]);quiet=[i for i,r in enumerate(records) if isinstance(r,dict) and r.get('event')=='quiet-observation'];proofs=[(i,r) for i,r in enumerate(records) if isinstance(r,dict) and r.get('event')=='trusted-parent-probe'];clicks=[i for i,r in enumerate(records) if isinstance(r,dict) and r.get('event')=='trusted-parent-probe-clicked'];require(len(quiet)==len(proofs)==len(clicks)==1 and quiet[0]<clicks[0]<proofs[0][0] and {k:proofs[0][1].get(k) for k in CLOSURE_KEYS}==p,'closure input ordering invalid')
  if guest is not None:
   values=console_records(guest,'AUTH-PARENT-PROBE-CLICK',interaction['run']);require(len(values)==1 and set(values[0])==KEYS|{'trusted'} and values[0]['trusted']is True and values[0]['source']=='top-parent' and values[0]['page_path']==path,'fresh guest trusted callback missing')
 except (OSError,ValueError,KeyError,TypeError,RuntimeError):return ['fresh native-query closure lacks real trusted parent input']
 return []
