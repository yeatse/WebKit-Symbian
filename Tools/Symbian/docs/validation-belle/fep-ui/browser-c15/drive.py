import argparse,ctypes as C,importlib.util,json,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--run',required=True);p.add_argument('--case',choices=['cancel','empty','submit','programmatic','readonly'],required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(exist_ok=False)
s=importlib.util.spec_from_file_location('native','/workspace/WebKit-Symbian-graphics/Tools/Symbian/tests/belle-ui/native-interact.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
guest=Path('/workspace/eka-validation/xdg/data/EKA2L1/data/drives/c/Data/WebKit/log.txt');marker='FEP-UI '+a.run+' '+a.case+' ';records=[];result={'status':'FAIL','run':a.run,'case':a.case,'actions':records};x=None
try:
 end=time.monotonic()+40;ready=None
 while time.monotonic()<end:
  for line in guest.read_text(errors='replace').splitlines() if guest.exists() else []:
   if marker+'READY ' in line:ready=json.loads(line.split(marker+'READY ',1)[1])
  if ready:break
  time.sleep(.1)
 assert ready and ready['viewport']==[360,532]
 pid=int(subprocess.check_output(['pgrep','-x','eka2l1_qt'],text=True));x=m.XInput(pid);assert x.check()[2:]==[900,700]
 result.update(pid=pid,ready=ready)
 def click(name):
  r=ready['rects'][name];point=[round(270+r['x']),round(77+r['y'])];x.click(*point);records.append({'action':'trusted '+name+' click','point':point,'monotonic':time.monotonic()})
 def key(w,name,ctrl=False):
  x.check();x.check_focus(w)
  code=lambda k:x.x.XKeysymToKeycode(x.display,x.x.XStringToKeysym(k.encode('ascii')))
  k=code(name);c=code('Control_L') if ctrl else None;assert k
  try:
   if c:x.t.XTestFakeKeyEvent(x.display,c,1,0)
   x.t.XTestFakeKeyEvent(x.display,k,1,0);x.x.XSync(x.display,0);time.sleep(.04)
  finally:
   x.t.XTestFakeKeyEvent(x.display,k,0,0)
   if c:x.t.XTestFakeKeyEvent(x.display,c,0,0)
   x.x.XSync(x.display,0)
 click('start');time.sleep(.7);assert not x.fep_window(),'button programmatic focus opened FEP';records.append({'action':'no FEP after programmatic focus','observed_seconds':.7})
 if a.case!='programmatic':
  click('field');time.sleep(.7);w=x.fep_window()
  if a.case=='readonly':assert not w,'readonly field opened FEP';records.append({'action':'readonly has no FEP'})
  else:
   assert w and x.geometry(w)[2:]==[400,300],'editable tap did not open calibrated FEP';result['fep_geometry']=x.geometry(w);subprocess.run(['import','-window',hex(w),str(a.output/'opened-fep.png')],check=True);records.append({'action':'same-PID FEP observed','geometry':x.geometry(w)})
   if a.case=='cancel':key(w,'Escape');records.append({'action':'native Escape reject'})
   else:
    x.click_window(w,x.geometry(w),80,40);key(w,'a',True);key(w,'BackSpace')
    if a.case=='submit':
     for char in 'native text':key(w,'space' if char==' ' else char)
    x.submit_fep([200,277,400,300]);records.append({'action':'actual Submit','case':a.case})
   time.sleep(.5);assert not x.fep_window(),'FEP did not close'
 click('finish');time.sleep(.3);subprocess.run(['import','-window','root',str(a.output/'after.png')],check=True)
 until=time.monotonic()+5
 while time.monotonic()<until:
  text=guest.read_text(errors='replace');assert marker+'FAIL ' not in text,'page rejected native UI result'
  if marker+'PAGE-PASS ' in text:break
  time.sleep(.1)
 else:raise AssertionError('missing matching PAGE-PASS')
 assert not x.fep_window();result['status']='NATIVE_INPUT_AND_PAGE_PASS_PENDING_RUNNER'
except BaseException as e:result['error']=str(e);raise
finally:
 if x:x.close()
 (a.output/'interaction.json').write_text(json.dumps(result,indent=2)+'\n')
print(result['status'])
