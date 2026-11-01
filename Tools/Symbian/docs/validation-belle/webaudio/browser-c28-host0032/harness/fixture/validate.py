#!/usr/bin/env python3
"""Strict public-report validator. It never establishes native sound or whole-run PASS."""
import argparse,json,math
from pathlib import Path
from protocol import ORDER,TOKEN,check_event,number
GAIN=[.5,-.5,.25,-.25,.125,-.125,0,.0625]
PCM=[-32768,-16384,-1,0,1,8192,16384,32767]
STATE_KEYS={'suite','version','run','provenance','events','received_monotonic','errors','page_requests','wav_requests','serve_requested','deadline_expired'}
def validate(state,run,stage='complete'):
 failures=[]
 def check(ok,message):
  if not ok:failures.append(message)
 if not isinstance(state,dict):return{'status':'FAIL','failures':['state object required'],'native_audio_verified':False}
 check(set(state)==STATE_KEYS,'state fields differ')
 check(state.get('suite')=='classic-webaudio'and type(state.get('version'))is int and state.get('version')==1 and state.get('run')==run and isinstance(run,str)and TOKEN.fullmatch(run)and state.get('provenance')=='browser-reported','fresh fixture identity differs')
 check(stage in ['offline','complete'],'unknown stage')
 check(state.get('serve_requested')is True and state.get('deadline_expired')is False,'prepare-only or expired fixture cannot complete')
 check(state.get('errors')==[]and type(state.get('page_requests'))is int and state.get('page_requests')==1 and type(state.get('wav_requests'))is int and state.get('wav_requests')==1,'unknown request, repeated page, or missing WAV request')
 events=state.get('events');times=state.get('received_monotonic');count=9 if stage=='offline'else 15
 if not isinstance(events,list)or not isinstance(times,list):check(False,'event/time arrays required');events=[];times=[]
 check(len(events)==len(times)==count,'fixed complete event count differs')
 for index,event in enumerate(events):
  try:
   check_event(event,run);check(index<len(ORDER)and event['kind']==ORDER[index]and event['sequence']==index,'event order/sequence differs')
  except(ValueError,TypeError,KeyError):check(False,'unknown or malformed public event')
 check(all(number(t)and t>0 for t in times)and all(a<=b for a,b in zip(times,times[1:])),'server receive times differ')
 # Invalid shapes are rejected before accessing values; no unknown fields are retained.
 if failures:return{'status':'FAIL','run':run,'stage':stage,'failures':failures,'native_audio_verified':False}
 values={e['kind']:e for e in events}
 caps=values['capabilities'];check(all(caps[k]is True for k in ['secure','audio_context','offline_context'])and all(caps[k]is False for k in ['audio_worklet','audio_worklet_node','audio_worklet_processor','context_worklet','base_worklet']),'classic APIs/secure localhost/explicit worklet absence differ')
 media=values['unsupported-media'];check(media['state_before']==media['state_after']=='suspended'and media['time_before']==media['time_after']==0 and media['factory']==media['constructor']=='NotSupportedError','both media-element entry points must reject before context activation')
 gain=values['gain'];check(gain['sample_rate']==32768 and gain['channels']==1 and gain['frames']==128 and gain['head']==GAIN and gain['mismatches']==0,'exact public gain vector differs')
 delay=values['delay-channel'];check(delay['sample_rate']==32768 and delay['channels']==2 and delay['frames']==512 and delay['probes']==[.25,1,-.5]and delay['mismatches']==0,'exact delay/channel swap vector differs')
 osc=values['oscillator'];check(osc['sample_rate']==32768 and osc['frames']==4096 and osc['finite']is True and abs(osc['rms']-math.sqrt(.5))<.01 and .98<osc['peak']<=1.01 and abs(osc['frequency']-1024)<2 and 117<=osc['crossings']<=121,'offline oscillator frequency/RMS differs')
 fft=values['analyser'];check(fft['sample_rate']==32768 and fft['fft_size']==1024 and fft['bin_count']==512 and abs(fft['peak_bin']-32)<=1 and -30<fft['peak_db']<=1 and fft['lower_gap_db']>=20 and fft['upper_gap_db']>=20,'known sine buffer FFT peak/separation differs')
 wav=values['wav-decode'];check(wav['sample_rate']==32768 and wav['channels']==1 and wav['frames']==8 and wav['samples']==[x/32768 for x in PCM],'same-rate PCM16 WAV exact decode differs')
 check(values['unsupported-decode']['error_name']=='EncodingError','unsupported bytes must reject as EncodingError')
 ready=values['native-ready'];check(ready['state']=='suspended'and ready['current_time']==0 and ready['layout']['button']['disabled']is False,'native context activated before first trusted click')
 if stage=='complete':
  start=values['native-start'];check(start['trusted']is True and start['state']=='running'and start['frequency']==997 and abs(start['gain']-.15)<1e-6,'first trusted start/997 Hz parameters differ')
  for kind,expected in [('native-running','running'),('native-suspended','suspended'),('native-resumed','running'),('quiet','closed')]:
   event=values[kind];delta=event['time_after']-event['time_before'];check(event['state']==expected and 600<=event['elapsed_ms']<=15000,kind+' state/bounded observation differs')
   if expected=='running':check(.1<delta<15 and delta<=event['elapsed_ms']/1000+.5,kind+' currentTime did not progress')
   else:check(delta==0,kind+' currentTime did not hold')
   if kind in ['native-suspended','native-resumed']:check(event['trusted']is True,kind+' trusted click missing')
   if kind!='quiet':check(event['layout']['button']['disabled']is False,kind+' next button not enabled')
  closed=values['native-closed'];check(closed['trusted']is True and closed['state']=='closed'and closed['layout']['button']['disabled']is True,'trusted close/closed state missing')
  check(start['current_time']<=values['native-running']['time_before']<=values['native-running']['time_after']<=values['native-suspended']['time_before']<=values['native-resumed']['time_before']<=values['native-resumed']['time_after']<=closed['current_time']==values['quiet']['time_before'],'native context time order differs')
  check(times[10]-times[9]>=.8 and times[14]-times[13]>=.8,'server did not observe bounded running/closed quiet windows')
 return{'status':'FAIL'if failures else'OFFLINE_API_COMPLETE'if stage=='offline'else'PUBLIC_API_COMPLETE','run':run,'stage':stage,'failures':failures,'native_audio_verified':False,'scope':'public browser reports only; Root must verify captured native PCM, trusted XTest input, normal owned-parent close, unchanged pins, and full restoration'}
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--state',type=Path,required=True);p.add_argument('--run',required=True);p.add_argument('--stage',choices=['offline','complete'],default='complete');args=p.parse_args();result=validate(json.loads(args.state.read_text()),args.run,args.stage);print(json.dumps(result,indent=2));return result['status']=='FAIL'
if __name__=='__main__':raise SystemExit(main())
