"""Allowlisted public API reports; importing performs no I/O."""
import math,re
ORDER=['capabilities','unsupported-media','gain','delay-channel','oscillator','analyser','wav-decode','unsupported-decode','native-ready','native-start','native-running','native-suspended','native-resumed','native-closed','quiet']
TOKEN=re.compile(r'wa-[0-9a-f]{32}\Z')
def require(ok):
 if not ok:raise ValueError('invalid fixed public event')
def number(value):return type(value)is int or type(value)is float and math.isfinite(value)
def bounded(value,low=0,high=1000000):return number(value)and low<=value<=high
def natural(value,high=1000000):return type(value)is int and 0<=value<=high
def layout(value):
 require(type(value)is dict and set(value)=={'innerWidth','innerHeight','devicePixelRatio','scrollX','scrollY','button','visualViewport'})
 require(bounded(value['devicePixelRatio'],.1,16))
 require(all(bounded(value[k],1,16384)for k in ['innerWidth','innerHeight'])and all(bounded(value[k],-16384,16384)for k in ['scrollX','scrollY']))
 r=value['button'];require(type(r)is dict and set(r)=={'x','y','width','height','disabled'})
 require(all(bounded(r[k],-16384,16384)for k in ['x','y'])and all(bounded(r[k],1,16384)for k in ['width','height'])and type(r['disabled'])is bool)
 r=value['visualViewport']
 if r is not None:
  require(type(r)is dict and set(r)=={'offsetLeft','offsetTop','width','height','scale'})
  require(all(bounded(r[k],-16384,16384)for k in ['offsetLeft','offsetTop'])and all(bounded(r[k],1,16384)for k in ['width','height'])and bounded(r['scale'],.01,100))
def check_event(value,run):
 require(type(value)is dict and value.get('run')==run and TOKEN.fullmatch(run)and natural(value.get('sequence'),15))
 kind=value.get('kind');fields={'run','kind','sequence'}
 if kind=='capabilities':
  keys={'secure','audio_context','offline_context','audio_worklet','audio_worklet_node','audio_worklet_processor','context_worklet','base_worklet'};fields|=keys;require(all(type(value.get(k))is bool for k in keys))
 elif kind=='unsupported-media':
  fields|={'state_before','state_after','time_before','time_after','factory','constructor'}
  require(all(value.get(k)in ['suspended','running','closed']for k in ['state_before','state_after'])and all(bounded(value.get(k))for k in ['time_before','time_after']))
  require(all(value.get(k)in ['NotSupportedError','TypeError','InvalidStateError','NoError']for k in ['factory','constructor']))
 elif kind in ['gain','delay-channel','wav-decode']:
  fields|={'sample_rate','channels','frames'};require(bounded(value.get('sample_rate'),3000,384000)and natural(value.get('channels'),2)and natural(value.get('frames'),4096))
  key,size=('head',8)if kind=='gain'else('probes',3)if kind=='delay-channel'else('samples',8);fields.add(key)
  require(type(value.get(key))is list and len(value[key])==size and all(bounded(x,-2,2)for x in value[key]))
  if kind!='wav-decode':fields.add('mismatches');require(natural(value.get('mismatches'),1024))
 elif kind=='oscillator':
  fields|={'sample_rate','frames','finite','rms','peak','frequency','crossings'}
  require(bounded(value.get('sample_rate'),3000,384000)and natural(value.get('frames'),4096)and type(value.get('finite'))is bool and all(bounded(value.get(k),0,384000)for k in ['rms','peak','frequency'])and natural(value.get('crossings'),4096))
 elif kind=='analyser':
  fields|={'sample_rate','fft_size','bin_count','peak_bin','peak_db','lower_gap_db','upper_gap_db'}
  require(bounded(value.get('sample_rate'),3000,384000)and all(natural(value.get(k),32768)for k in ['fft_size','bin_count','peak_bin'])and bounded(value.get('peak_db'),-1000,100)and all(bounded(value.get(k),-1000,200)for k in ['lower_gap_db','upper_gap_db']))
 elif kind=='unsupported-decode':fields.add('error_name');require(value.get('error_name')in ['EncodingError','NotSupportedError','TypeError','NoError'])
 elif kind=='native-ready':fields|={'state','current_time','sample_rate','layout'};require(bounded(value.get('sample_rate'),3000,384000))
 elif kind=='native-start':fields|={'trusted','state','current_time','frequency','gain'};require(bounded(value.get('frequency'),1,20000)and bounded(value.get('gain'),0,1))
 elif kind in ['native-running','native-suspended','native-resumed','quiet']:
  fields|={'state','time_before','time_after','elapsed_ms'};require(all(bounded(value.get(k))for k in ['time_before','time_after'])and bounded(value.get('elapsed_ms'),0,30000))
  if kind!='quiet':fields.add('layout')
  if kind in ['native-suspended','native-resumed']:fields.add('trusted')
 elif kind=='native-closed':fields|={'trusted','state','current_time','layout'}
 elif kind=='error':fields.add('code');require(value.get('code')in ['offline','native-start','native-suspend','native-resume','native-close'])
 else:raise ValueError('unknown public event')
 require(set(value)==fields)
 if 'state'in fields:require(value['state']in ['suspended','running','closed'])
 if 'current_time'in fields:require(bounded(value['current_time']))
 if 'trusted'in fields:require(type(value['trusted'])is bool)
 if 'layout'in fields:layout(value['layout'])
 return value
