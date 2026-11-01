#!/usr/bin/env python3
"""Synthetic standard-vector and strict-report checks only; no runtime/socket."""
import copy,json,math
from fixture import strict_json
from protocol import ORDER,check_event
from validate import validate
RUN='wa-'+'1'*32
def synthetic():
 left=[0]*512;right=[0]*512;left[128]=1;left[129]=-.5;right[3]=.25
 rows=[{'sample_rate':32768,'channels':2,'frames':512,'samples':[left,right]}, {'sample_rate':32768,'channels':1,'frames':512,'samples':[left.copy()]}, {'sample_rate':32768,'lowpass':{'type':'lowpass','frequency':8192,'q':-10*math.log10(2),'detune':0,'frequencies':[0,8192,16384],'magnitude':[1,math.sqrt(.5),0],'phase':[0,-math.pi/2]},'allpass':{'type':'allpass','frequency':8192,'q':1,'detune':0,'frequencies':[0,4096,8192,12288,16384],'magnitude':[1]*5,'phase':[0,-1,-math.pi,-2,0]}}]
 return{'suite':'classic-webaudio-extra','version':1,'run':RUN,'provenance':'browser-reported','events':[dict(row,run=RUN,sequence=i,kind=ORDER[i],secure=True)for i,row in enumerate(rows)],'received_monotonic':[100,101,102],'errors':[],'page_requests':1,'serve_requested':True,'deadline_expired':False}
def main():
 count=0;base=synthetic();assert validate(base,RUN)['status']=='OFFLINE_EXTRA_API_COMPLETE';count+=1
 variants=[]
 for index,channel,frame,value in [(0,0,128,0),(0,0,129,0),(0,0,127,1),(0,1,3,0),(0,1,511,.125),(1,0,128,0),(1,0,129,0),(1,0,511,.125)]:
  state=copy.deepcopy(base);state['events'][index]['samples'][channel][frame]=value;variants.append(state)
 for index,key,value in [(0,'secure',False),(1,'sample_rate',44100),(2,'sequence',1),(2,'unknown',1)]:
  state=copy.deepcopy(base);state['events'][index][key]=value;variants.append(state)
 for section,key,value in [('lowpass','q',math.sqrt(.5)),('lowpass','magnitude',[1,1,0]),('lowpass','magnitude',[1,math.sqrt(.5),.000011]),('lowpass','phase',[0,0]),('lowpass','phase',[0,-math.pi/2,0]),('lowpass','detune',1),('allpass','q',-3),('allpass','magnitude',[1,1,1.000011,1,1]),('allpass','phase',[0,0,None,0,0])]:
  state=copy.deepcopy(base);state['events'][2][section][key]=value;variants.append(state)
 for key,value in [('run','wa-'+'2'*32),('page_requests',2),('serve_requested',False),('deadline_expired',True),('errors',['invalid public request']),('extra',0)]:
  state=copy.deepcopy(base);state[key]=value;variants.append(state)
 state=copy.deepcopy(base);state['events'][0]['samples'][1].pop();variants.append(state)
 state=copy.deepcopy(base);state['events'].pop();variants.append(state)
 for state in variants:assert validate(state,RUN)['status']=='FAIL';count+=1
 state=copy.deepcopy(base);state['events'][2]['lowpass']['magnitude'][2]=.000009;assert validate(state,RUN)['status']=='OFFLINE_EXTRA_API_COMPLETE';count+=1
 for body in [b'{"a":1,"a":2}',b'{"a":NaN}',b'{"a":Infinity}']:
  try:strict_json(body)
  except ValueError:count+=1
  else:raise AssertionError('strict JSON guard failed')
 event=copy.deepcopy(base['events'][2]);event['allpass']['magnitude'][0]=float('nan')
 try:check_event(event,RUN)
 except ValueError:count+=1
 else:raise AssertionError('nonfinite report accepted')
 print(json.dumps({'status':'GUARDS_PASS_NOT_RUNTIME','checks':count,'synthetic_only':True,'server_guest_display_audio_started':False}))
if __name__=='__main__':main()
