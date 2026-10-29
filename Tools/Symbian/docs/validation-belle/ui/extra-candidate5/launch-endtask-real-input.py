import importlib.util,time,json,subprocess,ctypes as C,sys
from pathlib import Path
out=Path(sys.argv[1]);pid=int(subprocess.check_output(['pgrep','-x','eka2l1_qt']).strip());s=importlib.util.spec_from_file_location('ui','/workspace/WebKit-Symbian-graphics/Tools/Symbian/tests/belle-ui/native-interact.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m);x=m.XInput(pid);a=[]
for label,point in [('open picker',(449,270)),('Emulation',(75,10)),('Launch process',(112,77))]:
 x.click(*point);a.append({'action':label,'point':point,'monotonic':time.monotonic()});time.sleep(.5)
 if label=='open picker':subprocess.run(['import','-window','root',str(out/'picker-open.png')],check=True)
for window in x.owned_windows():
 name=C.c_void_p()
 try:
  if x.x.XFetchName(x.display,window,C.byref(name)) and name and C.string_at(name)==b'Launch process':x.window=window;break
 finally:
  if name:x.x.XFree(name)
else:raise RuntimeError('launch dialog absent')
x.initial_size=x.geometry(x.window)[2:];x.click(400,58);shift=x.x.XKeysymToKeycode(x.display,x.x.XStringToKeysym(b'Shift_L'))
for c in 'C:\\sys\\bin\\browser_exit_request.exe':
 shifted=c.isupper() or c in ':_';name=c.lower() if c.isalpha() else {':':'semicolon','_':'minus','\\':'backslash','.':'period'}.get(c,c)
 if shifted:x.t.XTestFakeKeyEvent(x.display,shift,1,0);x.x.XSync(x.display,0)
 x.key(name)
 if shifted:x.t.XTestFakeKeyEvent(x.display,shift,0,0);x.x.XSync(x.display,0)
subprocess.run(['import','-window','root',str(out/'helper-path-entered.png')],check=True);x.click(748,186);a.append({'action':'typed official helper path and clicked Launch','monotonic':time.monotonic()});x.close();(out/'actions.json').write_text(json.dumps(a,indent=2)+'\n')
