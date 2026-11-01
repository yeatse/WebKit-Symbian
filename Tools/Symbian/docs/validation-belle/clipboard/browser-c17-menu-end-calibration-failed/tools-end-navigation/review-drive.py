#!/usr/bin/env python3
"""Real menu navigation with root screenshot review before each Return."""
from pathlib import Path
import importlib.util,json,subprocess,time,sys,hashlib
BASE=Path('/workspace/symbian-clipboard-browser-work/drive.py')
spec=importlib.util.spec_from_file_location('clipboard_menu_base',BASE);base=importlib.util.module_from_spec(spec);spec.loader.exec_module(base)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
LABELS={'copy':'Copy','cut':'Cut','paste':'Paste','select-all':'Select all'}
class ReviewDriver(base.Driver):
 def command(self,action):
  if action['operation']=='start':return super().command(action)
  if self.args.mode!='menu':raise RuntimeError('review driver accepts menu mode only')
  operation=action['operation'];self.check_fep();self.log()
  plan=self.menu[operation]
  if not plan or plan[-1]!={'key':'Return','wait':.25}:raise RuntimeError('review plans must end in one explicit Return')
  directory=self.args.output/'menu-reviews'/('%02d-step%d-%s'%(sum(r['event']=='menu-review-approved' for r in self.records)+1,action['step'],operation));directory.mkdir(parents=True,exist_ok=False)
  def capture(name):
   self.x.check()
   if self.x.fep_window():raise RuntimeError('unexpected FEP while menu open; no screenshot/input guessing')
   path=directory/(name+'.png');subprocess.run(['import','-window',hex(self.x.window),str(path)],check=True,timeout=5)
   return {'path':str(path),'sha256':sha(path)}
  frames={}
  for number,item in enumerate(plan[:-1]):
   if set(item)-{'key','wait'} or item.get('key') not in {'F2','End','Up'}:raise RuntimeError('menu review permits F2/End/Up navigation only')
   self.x.key(item['key']);self.record('native-menu-action',step=action['step'],operation=operation,number=number+1,action=item);time.sleep(item.get('wait',.25))
   if number==0:frames['opened']=capture('opened')
   if item['key']=='End':frames['end']=capture('end')
  frames['selected']=capture('selected')
  gate={'run':self.args.run_token,'step':action['step'],'operation':operation,'expected_label':LABELS[operation],'frames':frames,'state':'WAITING_ROOT_REVIEW','no_return_sent':True}
  (directory/'gate.json').write_text(json.dumps(gate,indent=2)+'\n')
  self.record('menu-review-waiting',gate=str(directory/'gate.json'),selected_sha256=frames['selected']['sha256'])
  print('REVIEW '+str(directory/'gate.json')+' expected='+LABELS[operation],flush=True)
  deadline=min(self.deadline,time.monotonic()+32)
  approval=directory/'approval.json'
  while not approval.exists():
   self.x.check();self.log()
   if self.x.fep_window():raise RuntimeError('unexpected FEP while waiting menu review')
   if time.monotonic()>=deadline:raise RuntimeError('root menu screenshot review deadline; preserve incomplete run')
   time.sleep(.1)
  value=json.loads(approval.read_text())
  required={'run':self.args.run_token,'step':action['step'],'operation':operation,'observed_label':LABELS[operation],'selected_sha256':frames['selected']['sha256'],'screenshots_reviewed':True}
  if value!=required or sha(Path(frames['selected']['path']))!=frames['selected']['sha256']:raise RuntimeError('root review does not match exact screenshot/command/run')
  self.x.check();self.x.check_focus(self.x.window);self.log()
  self.record('menu-review-approved',gate=str(directory/'gate.json'),approval_sha256=sha(approval),observed_label=value['observed_label'])
  self.x.key('Return');self.record('native-menu-action',step=action['step'],operation=operation,number=len(plan),action=plan[-1]);time.sleep(.25)
  self.record('native-menu-operation',step=action['step'],operation=operation,action=action);time.sleep(self.args.settle)
base.Driver=ReviewDriver
if __name__=='__main__':raise SystemExit(base.main())
