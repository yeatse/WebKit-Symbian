#!/usr/bin/env python3
"""Independent public standard vectors; completion is not native PCM acceptance."""
import argparse,json,math
from pathlib import Path
from protocol import ORDER,TOKEN,check_event,number
STATE_KEYS={'suite','version','run','provenance','events','received_monotonic','errors','page_requests','serve_requested','deadline_expired'}
def validate(state,run,stage='complete'):
 failures=[]
 def check(ok,message):
  if not ok:failures.append(message)
 if not isinstance(state,dict):return{'status':'FAIL','failures':['state object required'],'native_audio_verified':False}
 check(set(state)==STATE_KEYS and state.get('suite')=='classic-webaudio-extra'and type(state.get('version'))is int and state['version']==1 and state.get('run')==run and type(run)is str and TOKEN.fullmatch(run)and state.get('provenance')=='browser-reported','fresh extra fixture identity differs')
 check(stage=='complete'and state.get('serve_requested')is True and state.get('deadline_expired')is False,'prepare-only, wrong stage, or expired run')
 check(state.get('errors')==[]and type(state.get('page_requests'))is int and state['page_requests']==1,'unknown request or repeated page')
 events=state.get('events');times=state.get('received_monotonic');check(type(events)is list and type(times)is list,'event/time arrays required')
 if type(events)is not list or type(times)is not list:events=[];times=[]
 check(len(events)==len(times)==3 and all(number(t)and t>0 for t in times)and all(a<=b for a,b in zip(times,times[1:])),'exact ordered three reports required')
 for index,event in enumerate(events):
  try:check_event(event,run);check(index<3 and event['kind']==ORDER[index]and event['sequence']==index and event['secure']is True,'event sequence/secure context differs')
  except(ValueError,KeyError,TypeError):check(False,'malformed or unknown public event')
 if not failures:
  a,b,c=events
  check(a['sample_rate']==b['sample_rate']==c['sample_rate']==32768,'actual sample rate differs')
  left=[0]*512;right=[0]*512;left[128]=1;left[129]=-.5;right[3]=.25
  check(a['samples']==[left,right],'case A full 512-frame stereo vector differs')
  check(b['samples']==[left],'case B full 512-frame mono vector differs')
  low=c['lowpass'];allpass=c['allpass'];check(low['type']=='lowpass'and low['frequency']==8192 and abs(low['q']+10*math.log10(2))<1e-6 and low['detune']==0 and low['frequencies']==[0,8192,16384],'lowpass public settings differ')
  check(all(abs(x-y)<=1e-5 for x,y in zip(low['magnitude'],[1,math.sqrt(.5),0]))and all(abs(x-y)<=1e-5 for x,y in zip(low['phase'],[0,-math.pi/2])),'lowpass standard magnitude/phase differs')
  check(allpass['type']=='allpass'and allpass['frequency']==8192 and allpass['q']==1 and allpass['detune']==0 and allpass['frequencies']==[0,4096,8192,12288,16384]and all(abs(x-1)<=1e-5 for x in allpass['magnitude']),'allpass unit magnitude differs')
 return{'status':'FAIL'if failures else'OFFLINE_EXTRA_API_COMPLETE','run':run,'failures':failures,'native_audio_verified':False,'scope':'three public offline numeric reports only; normal owned-parent close, health, unchanged pins and full restoration require wrapper; no PCM claim'}
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--state',type=Path,required=True);parser.add_argument('--run',required=True);args=parser.parse_args();result=validate(json.loads(args.state.read_text()),args.run);print(json.dumps(result,indent=2));raise SystemExit(result['status']=='FAIL')
