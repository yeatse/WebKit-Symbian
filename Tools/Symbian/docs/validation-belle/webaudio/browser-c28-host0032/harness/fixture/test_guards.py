#!/usr/bin/env python3
"""Pure synthetic protocol/validator checks. No browser, socket, display, or audio."""
import copy,json,math
from fixture import Fixture,strict_json,WAV
from protocol import ORDER,check_event
from validate import validate,GAIN,PCM
RUN='wa-'+'1'*32
LAYOUT={'innerWidth':360,'innerHeight':640,'devicePixelRatio':1,'scrollX':0,'scrollY':0,'button':{'x':12,'y':90,'width':336,'height':64,'disabled':False},'visualViewport':None}
def synthetic():
 rows=[
 {'secure':True,'audio_context':True,'offline_context':True,'audio_worklet':False,'audio_worklet_node':False,'audio_worklet_processor':False,'context_worklet':False,'base_worklet':False},
 {'state_before':'suspended','state_after':'suspended','time_before':0,'time_after':0,'factory':'NotSupportedError','constructor':'NotSupportedError'},
 {'sample_rate':32768,'channels':1,'frames':128,'head':GAIN,'mismatches':0},
 {'sample_rate':32768,'channels':2,'frames':512,'probes':[.25,1,-.5],'mismatches':0},
 {'sample_rate':32768,'frames':4096,'finite':True,'rms':math.sqrt(.5),'peak':1,'frequency':1024,'crossings':119},
 {'sample_rate':32768,'fft_size':1024,'bin_count':512,'peak_bin':32,'peak_db':-6,'lower_gap_db':80,'upper_gap_db':80},
 {'sample_rate':32768,'channels':1,'frames':8,'samples':[x/32768 for x in PCM]},
 {'error_name':'EncodingError'},
 {'state':'suspended','current_time':0,'sample_rate':48000,'layout':copy.deepcopy(LAYOUT)},
 {'trusted':True,'state':'running','current_time':0,'frequency':997,'gain':.15},
 {'state':'running','time_before':0,'time_after':1,'elapsed_ms':1000,'layout':copy.deepcopy(LAYOUT)},
 {'trusted':True,'state':'suspended','time_before':1.1,'time_after':1.1,'elapsed_ms':700,'layout':copy.deepcopy(LAYOUT)},
 {'trusted':True,'state':'running','time_before':1.1,'time_after':2.1,'elapsed_ms':1000,'layout':copy.deepcopy(LAYOUT)},
 {'trusted':True,'state':'closed','current_time':2.2,'layout':copy.deepcopy(LAYOUT)},
 {'state':'closed','time_before':2.2,'time_after':2.2,'elapsed_ms':1000}]
 rows[13]['layout']['button']['disabled']=True
 return{'suite':'classic-webaudio','version':1,'run':RUN,'provenance':'browser-reported','events':[dict(value,run=RUN,sequence=i,kind=ORDER[i])for i,value in enumerate(rows)],'received_monotonic':[100+i for i in range(15)],'errors':[],'page_requests':1,'wav_requests':1,'serve_requested':True,'deadline_expired':False}
def main():
 count=0;base=synthetic();assert validate(base,RUN)['status']=='PUBLIC_API_COMPLETE';count+=1
 variants=[]
 for index,key,value in [(0,'audio_worklet',True),(1,'factory','NoError'),(1,'constructor','NoError'),(1,'time_after',1),(2,'head',[0]*8),(2,'mismatches',1),(3,'mismatches',1),(4,'rms',0),(4,'frequency',900),(5,'peak_bin',10),(5,'lower_gap_db',0),(6,'samples',[0]*8),(7,'error_name','NoError'),(8,'current_time',1),(9,'trusted',False),(11,'trusted',False),(11,'time_after',1.2),(12,'trusted',False),(13,'trusted',False),(14,'time_after',2.3)]:
  state=copy.deepcopy(base);state['events'][index][key]=value;variants.append(state)
 for key,value in [('run','wa-'+'2'*32),('errors',['invalid public request']),('serve_requested',False),('deadline_expired',True),('wav_requests',0),('page_requests',True),('version',True),('unknown',1)]:
  state=copy.deepcopy(base);state[key]=value;variants.append(state)
 state=copy.deepcopy(base);state['events'][2]['unknown']=1;variants.append(state)
 state=copy.deepcopy(base);state['events'][3]['sequence']=2;variants.append(state)
 state=copy.deepcopy(base);state['received_monotonic'][14]=state['received_monotonic'][13]+.1;variants.append(state)
 for state in variants:assert validate(state,RUN)['status']=='FAIL';count+=1
 for payload in [b'{"a":1,"a":2}',b'{"a":NaN}',b'{"a":Infinity}']:
  try:strict_json(payload)
  except ValueError:count+=1
  else:raise AssertionError('non-strict JSON accepted')
 for transform in [lambda v:v.update(extra='unknown'),lambda v:v.update(run='stale'),lambda v:v.update(frequency=float('nan')),lambda v:v.update(frequency=10**2000)]:
  event=copy.deepcopy(base['events'][9]);transform(event)
  try:check_event(event,RUN)
  except ValueError:count+=1
  else:raise AssertionError('unknown event accepted')
 assert len(WAV)==60 and WAV[:4]==b'RIFF'and WAV[44:]==__import__('struct').pack('<8h',*PCM);count+=1
 print(json.dumps({'status':'GUARDS_PASS_NOT_RUNTIME','checks':count,'synthetic_only':True,'browser_server_guest_display_audio_started':False}))
if __name__=='__main__':main()
