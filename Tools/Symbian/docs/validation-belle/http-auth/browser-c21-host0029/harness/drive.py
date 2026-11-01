#!/usr/bin/env python3
"""Role-calibrated C21 auth input; preparation never opens credentials or display."""
import argparse,ctypes as C,json,os,time
from pathlib import Path
from support import ROOT,CASES,SCHEMES,sha,load_module,checked_inputs,check_calibration,require
from mask_review import perform_probe
from heading_review import perform_heading
from parent_probe import perform as perform_parent_probe
legacy=load_module('auth_input_upstream',ROOT/'upstream/drive.py')
class Drive(legacy.Drive):
 def __init__(self,args,device,calibration,start):
  self.ui=calibration;self.pending_role=None
  require(device.check()==calibration['main_geometry'],'main geometry changed; public recalibration required')
  user=calibration['username'];args.start_point=start;args.fep_edit=user['edit_point'];args.fep_submit=user['submit_point']+user['fep_geometry'][2:];args.prompt_buttons=user['ok_point']+user['cancel_point'];args.native_editor=user['native_editor_point'];args.exit_calibration=None
  # Legacy constructor privately reads only this fresh fixture's credential
  # file. No existing run credentials are used by preparation or copied here.
  super().__init__(args,device)
  self.output['calibration']={'main_geometry':calibration['main_geometry'],'start_point':start,'username':calibration['username'],'password':calibration['password'],'sha256':sha(args.calibration),'credential_stage_screenshots':False}
  self.output['driver_sha256']=sha(Path(__file__));self.output['upstream_driver_sha256']=sha(ROOT/'upstream/drive.py')
  self.output.update(credential_stage_screenshots=False,public_probe_only_screenshots=True,password_mask_reviews=[],heading_reviews=[])
  self.ledger=os.fdopen(os.open(str(args.output)+'.actions.jsonl',os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600),'w')
 def record(self,event,**fields):
  super().record(event,**fields)
  if event in ('public-mask-probe-captured','public-heading-captured'):self.output['screenshots_taken']=True
  if hasattr(self,'ledger'):self.ledger.write(json.dumps(self.records[-1])+'\n');self.ledger.flush()
 def main_focus_acceptable(self):
  self.x.check();focus,revert=C.c_ulong(),C.c_int();self.x.x.XGetInputFocus(self.x.display,C.byref(focus),C.byref(revert))
  if focus.value not in (0,1,self.x.root):self.x.check_focus(self.x.window)
 def edit_fep(self,role,value):
  require(role in ('username','password'),'unknown calibrated editor role');ui=self.ui[role];self.pending_role=role
  if role=='username' and not self.output['heading_reviews']:
   perform_heading(self.x,self.key_window,ui,self.ready,self.args.pid,self.args.output.parent,self.output['heading_reviews'],self.record,self.wait,min(self.args.mask_review_timeout,self.args.timeout),content_rect=self.ui["content_rect"])
  if not self.x.fep_window() and ui['native_editor_point'] is not None:
   self.main_focus_acceptable();self.x.click(*ui['native_editor_point'])
  window=self.wait(self.x.fep_window,'same-PID calibrated '+role+' FEP')
  g=self.x.geometry(window);require(g==ui['fep_geometry'],role+' FEP geometry differs; do not type before public recalibration')
  self.x.click_window(window,g,*ui['edit_point'])
  if role=='password':
   perform_probe(self.x,self.key_window,ui,self.ready,self.args.pid,self.args.output.parent,
                 self.output['password_mask_reviews'],self.record,min(self.args.mask_review_timeout,self.args.timeout))
   self.record('private-password-input-begins' if value else 'empty-password-submission-after-probe',instance=len(self.output['password_mask_reviews']))
  else:
   self.key_window(window,'a','Control_L');self.key_window(window,'BackSpace')
   self.record('private-username-input-begins' if value else 'empty-username-submission-after-heading')
  for character in value:self.key_window(window,character)
  self.x.submit_fep(ui['submit_point']+g[2:]);self.wait(lambda:not self.x.fep_window(),'calibrated FEP submission');time.sleep(self.args.settle)
  self.record('editor-submitted',role=role,geometry=g,widget=ui.get('widget'))
 def execute(self):
  super().execute()
  if not self.ready['case'].startswith('exit'):perform_parent_probe(self)
 def native_button(self,accept):
  require(self.pending_role in ('username','password'),'native query stage unknown');self.main_focus_acceptable();ui=self.ui[self.pending_role]
  self.x.click(*(ui['ok_point'] if accept else ui['cancel_point']));self.record('native-accept' if accept else 'native-cancel',role=self.pending_role);time.sleep(self.args.settle)
 def launch_exit_helper(self):
  ui=self.ui['exit_launch'];require(ui.get('calibrated') is True,'official helper Launch process UI uncalibrated')
  data={k:ui[k] for k in ('menu','launch_item','dialog_size','path_field','launch_button')};p=self.args.output.parent/'launch-calibration.public.json';p.write_text(json.dumps(data,indent=2)+'\n');self.args.exit_calibration=p
  super().launch_exit_helper()

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--execute',action='store_true');p.add_argument('--scheme',choices=SCHEMES,default='basic');p.add_argument('--case',choices=CASES,default='success');p.add_argument('--calibration',type=Path,default=ROOT/'calibration.example.json');p.add_argument('--pid',type=int);p.add_argument('--fixture',type=Path);p.add_argument('--guest-log',type=Path);p.add_argument('--output',type=Path);p.add_argument('--run-token');p.add_argument('--timeout',type=float,default=30);p.add_argument('--settle',type=float,default=.7);p.add_argument('--quiet',type=float,default=5);p.add_argument('--mask-review-timeout',type=float,default=30);a=p.parse_args()
 checked_inputs();ui,start,needs=check_calibration(a.calibration,a.scheme,a.case)
 require(1<=a.timeout<=45 and .1<=a.settle<=2 and 3<=a.quiet<=10 and 1<=a.mask_review_timeout<=30,'invalid bounded timing')
 if not a.execute:print(json.dumps({'status':'PREPARED_NEEDS_CALIBRATION' if needs else 'PREPARED_NOT_EXECUTED','scheme':a.scheme,'case':a.case,'needs':needs,'credentials_opened':False,'display_used':False},indent=2));return 0
 require(not needs,'public role calibration incomplete');require(a.pid and a.fixture and a.guest_log and a.output and a.run_token,'execution needs owned PID/fresh fixture/log/output/token')
 ready=json.loads((a.fixture/'ready.json').read_text());require((ready['run'],ready['scheme'],ready['case'])==(a.run_token,a.scheme,a.case),'fixture identity mismatch');a.input_module=Path(json.loads((ROOT/'pins.json').read_text())['files']['input']['path'])
 a.output.parent.mkdir(mode=0o700,parents=True,exist_ok=True);stream=os.fdopen(os.open(a.output,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600),'w');device=driver=None;result={'status':'FAIL','run':a.run_token,'scheme':a.scheme,'case':a.case,'screenshots_taken':False,'error':'input setup incomplete'}
 try:
  device=legacy.load_input(a.input_module)(a.pid);driver=Drive(a,device,ui,start);driver.main_focus_acceptable();driver.execute();result=driver.output
 except Exception as error:result=driver.output if driver else result;result.update(status='FAIL',error=str(error))
 finally:
  if device:device.close()
  if driver and hasattr(driver,'ledger'):driver.ledger.close()
  json.dump(result,stream,indent=2);stream.write('\n');stream.close()
 print(json.dumps({k:result.get(k) for k in ('status','run','scheme','case','error')}));return 1 if result['status']=='FAIL' else 0
if __name__=='__main__':raise SystemExit(main())
