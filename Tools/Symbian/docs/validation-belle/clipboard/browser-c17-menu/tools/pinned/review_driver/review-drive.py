#!/usr/bin/env python3
"""Open each native menu, then click only a root-reviewed exact screenshot point."""
from pathlib import Path
import importlib.util,json,subprocess,time,sys,hashlib
BASE=Path('/workspace/symbian-clipboard-browser-work/drive.py')
spec=importlib.util.spec_from_file_location('clipboard_menu_base',BASE);base=importlib.util.module_from_spec(spec);spec.loader.exec_module(base)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
LABELS={'copy':'Copy','cut':'Cut','paste':'Paste','select-all':'Select all'}
def validate_point_review(value,gate,pid,window,geometry):
 required={'run':gate['run'],'step':gate['step'],'operation':gate['operation'],'observed_label':gate['expected_label'],'opened_sha256':gate['opened']['sha256'],'pid':pid,'window':hex(window),'geometry':geometry,'screenshots_reviewed':True}
 if not isinstance(value,dict) or set(value)!=set(required)|{'point'}:raise RuntimeError('review shape mismatch')
 if any(value[k]!=v for k,v in required.items()):raise RuntimeError('review does not match exact screenshot/PID/window/geometry/command/run')
 point=value['point']
 if not isinstance(point,list) or len(point)!=2 or any(type(n)is not int for n in point) or not (0<=point[0]<geometry[2] and 0<=point[1]<geometry[3]):raise RuntimeError('review point outside captured main window')
 return point
class ReviewDriver(base.Driver):
 def command(self,action):
  if action['operation']=='start':return super().command(action)
  if self.args.mode!='menu':raise RuntimeError('review driver accepts menu mode only')
  operation=action['operation'];self.check_fep();self.log()
  if self.menu[operation]!=[{'key':'F2','wait':.35}]:raise RuntimeError('point-review plan may only open Options with F2')
  directory=self.args.output/'menu-reviews'/('%02d-step%d-%s'%(sum(r['event']=='menu-review-approved' for r in self.records)+1,action['step'],operation));directory.mkdir(parents=True,exist_ok=False)
  self.x.key('F2');self.record('native-menu-action',step=action['step'],operation=operation,number=1,action={'key':'F2','wait':.35});time.sleep(.35)
  geometry=self.x.check()
  if self.x.fep_window():raise RuntimeError('unexpected FEP while opening menu; no screenshot/input guessing')
  image=directory/'opened.png';subprocess.run(['import','-window',hex(self.x.window),str(image)],check=True,timeout=5)
  gate={'run':self.args.run_token,'step':action['step'],'operation':operation,'expected_label':LABELS[operation],'pid':self.args.pid,'window':hex(self.x.window),'geometry':geometry,'opened':{'path':str(image),'sha256':sha(image)},'state':'WAITING_ROOT_POINT_REVIEW','no_edit_command_selected':True}
  (directory/'gate.json').write_text(json.dumps(gate,indent=2)+'\n')
  self.record('menu-review-waiting',gate=str(directory/'gate.json'),opened_sha256=gate['opened']['sha256'],geometry=geometry)
  print('REVIEW '+str(directory/'gate.json')+' expected='+LABELS[operation]+' pid='+str(self.args.pid),flush=True)
  deadline=min(self.deadline,time.monotonic()+32);approval=directory/'approval.json'
  while not approval.exists():
   if self.x.check()!=geometry:raise RuntimeError('main geometry changed after screenshot; recalibrate in a fresh run')
   self.log()
   if self.x.fep_window():raise RuntimeError('unexpected FEP while waiting menu review')
   if time.monotonic()>=deadline:raise RuntimeError('root menu screenshot review deadline; preserve incomplete run')
   time.sleep(.1)
  value=json.loads(approval.read_text())
  point=validate_point_review(value,gate,self.args.pid,self.x.window,self.x.check())
  if sha(image)!=gate['opened']['sha256']:raise RuntimeError('review screenshot changed')
  self.x.check_focus(self.x.window);self.log()
  self.record('menu-review-approved',gate=str(directory/'gate.json'),approval_sha256=sha(approval),observed_label=value['observed_label'],point=point,pid=self.args.pid,geometry=geometry)
  self.x.click_window(self.x.window,geometry,*point)
  self.record('native-menu-action',step=action['step'],operation=operation,number=2,action={'point':point},geometry=geometry)
  time.sleep(.25);self.record('native-menu-operation',step=action['step'],operation=operation,action=action);time.sleep(self.args.settle)
base.Driver=ReviewDriver
if __name__=='__main__':raise SystemExit(base.main())
