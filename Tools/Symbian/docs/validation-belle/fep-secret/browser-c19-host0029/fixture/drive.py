#!/usr/bin/env python3
"""Real-input candidate: never launches EKA, edits a guest, or supplies credentials."""
import argparse, hashlib, importlib.util, json, os, re, subprocess, time
from pathlib import Path
BASE=Path(__file__).resolve().parent
HELPER=Path('/workspace/WebKit-Symbian-graphics/Tools/Symbian/tests/belle-ui/native-interact.py')
HELPER_SHA='3b31229833fc4544cdd17b1a6357e766bb1de40c165e2efa4f7452e41947b2f0'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(path,value):path.write_text(json.dumps(value,indent=2)+'\n')
def pins(path):
 data=json.loads(path.read_text())
 if set(data)!={'browser','host','fep'}:raise ValueError('pin browser, host and fep')
 for role,item in data.items():
  if (not isinstance(item,dict) or set(item)!={'path','sha256'} or not isinstance(item['path'],str) or not Path(item['path']).is_absolute() or not isinstance(item['sha256'],str) or not re.fullmatch('[0-9a-f]{64}',item['sha256'])):raise ValueError(role+': fill the deployed candidate absolute path and SHA256; template pins cannot run')
  if sha(item['path'])!=item['sha256']:raise ValueError(role+': deployed file differs from pin')
 if sha(HELPER)!=HELPER_SHA:raise ValueError('native helper changed; review before repinning')
 return data

def calibration(path,fep=False):
 d=json.loads(path.read_text())
 if d.get('main_reviewed_actual_screenshot') is not True:raise ValueError('review current main screenshot and content rectangle first')
 for name,n in [('main_size',2),('content_rect',4)]:
  v=d.get(name)
  if not isinstance(v,list) or len(v)!=n or any(type(x)is not int or x<0 for x in v):raise ValueError('invalid '+name)
 w,h=d['main_size'];l,t,cw,ch=d['content_rect']
 if not(w>0 and h>0 and cw>0 and ch>0 and l+cw<=w and t+ch<=h):raise ValueError('invalid content rectangle')
 if fep:
  if d.get('fep_reviewed_actual_screenshot') is not True:raise ValueError('inspect opened-fep.png and calibrate actual secret editor/Submit first')
  v=d.get('fep_size')
  if not isinstance(v,list) or len(v)!=2 or any(type(x)is not int or x<=0 for x in v):raise ValueError('invalid fep_size')
  for name in ['edit_point','submit_point']:
   v=d.get(name)
   if not isinstance(v,list) or len(v)!=2 or any(type(x)is not int for x in v) or not(0<=v[0]<d['fep_size'][0] and 0<=v[1]<d['fep_size'][1]):raise ValueError('invalid '+name)
 return d

def events(a):
 marker='SECRET-FEP '+a.run+' '+a.case+' ';rows=[]
 for line in a.guest_log.read_text(errors='replace').splitlines() if a.guest_log.exists() else []:
  if marker in line:
   name,payload=line.split(marker,1)[1].split(' ',1);rows.append({'kind':name,'payload':json.loads(payload)})
 if any(r['kind']=='FAIL' for r in rows):raise RuntimeError('matching page FAIL; retain raw log and failed attempt')
 return rows

def event(rows,name):
 found=[r['payload'] for r in rows if r['kind']==name]
 if len(found)>1:raise RuntimeError('duplicate '+name+'; never replay a stale run')
 return found[0] if found else None

def wait(a,x,fn,seconds,what):
 deadline=time.monotonic()+seconds
 while time.monotonic()<deadline:
  if x:x.check()
  value=fn(events(a))
  if value:return value
  time.sleep(.1)
 raise RuntimeError('timed out waiting for '+what)

def identities():return {name:sha(BASE/name) for name in ['index.html','drive.py','server.py']}

def run(a):
 approved=pins(a.pins);cal=calibration(a.calibration,a.phase=='finish')
 if not re.fullmatch('[A-Za-z0-9_-]{1,64}',a.run):raise ValueError('invalid fresh token')
 a.guest_log=a.guest_log.resolve()
 if a.phase=='open':a.output.mkdir(parents=True,exist_ok=False)
 elif not a.output.is_dir() or (a.output/'interaction.json').exists():raise ValueError('finish requires one open session, no previous finish attempt')
 r={'status':'FAIL','run':a.run,'case':a.case,'phase':a.phase,'pid':a.pid,'guest_log':str(a.guest_log),'pins':approved,'identities':identities(),'actions':[],'screenshots':[],'mask_review_required':True,'runner_normal_close_required':True};x=None
 resultpath=a.output/('open-result.json' if a.phase=='open' else 'interaction.json')
 try:
  session=None
  if a.phase=='finish':
   session=json.loads((a.output/'session.json').read_text())
   for k in ['run','case','pid','guest_log','pins','identities']:
    if session[k]!=r[k]:raise ValueError('opened-session '+k+' changed; start fresh')
   if time.time()-session['opened_at_unix']>240:raise ValueError('open session expired; retain INCOMPLETE and start fresh')
   if session['main_calibration']!={k:cal[k] for k in ['main_size','content_rect']}:raise ValueError('main calibration changed after input')
  ready=wait(a,None,lambda rows:event(rows,'READY'),40,'fresh matching READY')
  if ready.get('run')!=a.run or ready.get('case')!=a.case or ready.get('publicDataOnly')is not True:raise ValueError('READY identity mismatch')
  if ready['viewport']!=cal['content_rect'][2:]:raise ValueError('viewport differs from calibrated unscaled content rectangle')
  if a.phase=='open' and any(e['kind']!='READY' for e in events(a)):raise ValueError('run already started; use fresh token')
  spec=importlib.util.spec_from_file_location('secret_native_input',HELPER);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);x=m.XInput(a.pid)
  if x.check()[2:]!=cal['main_size']:raise ValueError('main window differs from reviewed calibration')
  r.update(main_window=hex(x.window),ready=ready)
  def shot(name,w):
   path=a.output/(name+'.png')
   if path.exists():raise RuntimeError('screenshot exists; never overwrite evidence')
   subprocess.run(['import','-window',hex(w),str(path)],check=True,timeout=10)
   r['screenshots'].append({'path':path.name,'sha256':sha(path),'monotonic':time.monotonic()})
  def click(name):
   p=ready['rects'][name];l,t,w,h=cal['content_rect']
   if not(0<p['x']<w and 0<p['y']<h and p['width']>0 and p['height']>0):raise ValueError('DOM target outside visible viewport')
   point=[round(l+p['x']),round(t+p['y'])];x.click(*point);r['actions'].append({'action':'trusted '+name+' click','point':point,'monotonic':time.monotonic()})
  if a.phase=='open':
   if x.fep_window():raise RuntimeError('unexpected pre-existing FEP')
   shot('before-start',x.window);click('start');wait(a,x,lambda rows:event(rows,'ARMED'),5,'trusted Start');time.sleep(.7)
   if x.fep_window():raise RuntimeError('Start programmatic focus unexpectedly opened FEP')
   r['actions'].append({'action':'programmatic focus did not open FEP','observed_seconds':.7})
   click('field');wait(a,x,lambda rows:event(rows,'FIELD-TAP'),5,'trusted password field tap');w=wait(a,x,lambda rows:x.fep_window(),5,'same-PID native FEP');time.sleep(.25);shot('opened-fep',w)
   r.update(status='OPENED_PENDING_ACTUAL_MASK_REVIEW_AND_CALIBRATION',fep_window=hex(w),fep_geometry=x.geometry(w),opened_at_unix=time.time(),main_calibration={k:cal[k] for k in ['main_size','content_rect']});write(a.output/'session.json',r)
  else:
   if r['main_window']!=session['main_window'] or ready!=session['ready']:raise RuntimeError('main window or READY changed')
   if event(events(a),'PAGE-PASS'):raise RuntimeError('unexpected earlier PAGE-PASS')
   w=x.fep_window()
   if not w or hex(w)!=session['fep_window'] or x.geometry(w)!=session['fep_geometry']:raise RuntimeError('opened FEP identity or geometry changed')
   if x.geometry(w)[2:]!=cal['fep_size']:raise ValueError('secret modal differs from actual calibration')
   r.update(fep_window=hex(w),fep_geometry=x.geometry(w))
   def key(name,ctrl=False):
    x.check();x.check_focus(w);code=lambda s:x.x.XKeysymToKeycode(x.display,x.x.XStringToKeysym(s.encode('ascii')));k=code(name);c=code('Control_L') if ctrl else None
    if not k or (ctrl and not c):raise ValueError('unknown X11 keycode')
    try:
     if c and not x.t.XTestFakeKeyEvent(x.display,c,1,0):raise RuntimeError('native Ctrl press failed')
     if not x.t.XTestFakeKeyEvent(x.display,k,1,0):raise RuntimeError('native key press failed')
     x.x.XSync(x.display,0);time.sleep(.025)
    finally:
     x.t.XTestFakeKeyEvent(x.display,k,0,0)
     if c:x.t.XTestFakeKeyEvent(x.display,c,0,0)
     x.x.XSync(x.display,0)
   x.click_window(w,x.geometry(w),*cal['edit_point']);key('a',True);shot('selected-original-fep',w);key('BackSpace');shot('cleared-fep',w)
   text='public-mask-draft' if a.case=='cancel' else 'public-mask-edited' if a.case=='submit' else ''
   for i,char in enumerate(text,1):key('minus' if char=='-' else char);shot('typed-%02d-immediate-fep'%i,w);time.sleep(.06)
   time.sleep(.6);shot('typed-settled-fep',w);r['actions'].append({'action':'native Ctrl+A, Backspace and fixed public test input','typed_utf16_length':len(text)})
   if a.case=='cancel':key('Escape');r['actions'].append({'action':'native Escape after editing public draft'})
   else:x.submit_fep(cal['submit_point']+cal['fep_size']);r['actions'].append({'action':'actual native Submit','empty':a.case=='empty'})
   wait(a,x,lambda rows:not x.fep_window(),5,'native FEP completion');time.sleep(.5);click('finish');page=wait(a,x,lambda rows:event(rows,'PAGE-PASS'),5,'exact native result assertion')
   expected={'cancel':(17,[1,4]),'submit':(18,[18,18]),'empty':(0,[0,0])}[a.case]
   if page.get('type')!='password' or page.get('trustedTap')is not True or (page.get('utf16Length'),page.get('selection'))!=expected or page.get('maskReviewRequired')is not True:raise RuntimeError('unexpected PAGE-PASS payload')
   shot('after-page-check',x.window);r.update(status='NATIVE_INPUT_AND_PAGE_PASS_PENDING_RUNNER_AND_MASK_REVIEW',page_pass=page)
 except BaseException as error:r['error']=str(error);raise
 finally:
  if x:x.close()
  write(resultpath,r)
 print(r['status'])

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--phase',choices=['open','finish'],required=True);p.add_argument('--pid',type=int,required=True);p.add_argument('--run',required=True);p.add_argument('--case',choices=['cancel','submit','empty'],required=True);p.add_argument('--guest-log',type=Path,required=True);p.add_argument('--pins',type=Path,required=True);p.add_argument('--calibration',type=Path,required=True);p.add_argument('--output',type=Path,required=True);run(p.parse_args())
if __name__=='__main__':main()
