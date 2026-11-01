"""Only three public offline reports, no runtime access on import."""
import math,re
ORDER=['swapped-delay-channel','mono-gain-delay','biquad-response']
TOKEN=re.compile(r'wa-[0-9a-f]{32}\Z')
def require(ok):
 if not ok:raise ValueError('invalid fixed offline event')
def number(value):return type(value)is int or type(value)is float and math.isfinite(value)
def bounded(value,low,high):return number(value)and low<=value<=high
def array(value,count,low,high):require(type(value)is list and len(value)==count and all(bounded(item,low,high)for item in value))
def filter_response(value,count,phase_count):
 require(type(value)is dict and set(value)=={'type','frequency','q','detune','frequencies','magnitude','phase'})
 require(value['type']in ['lowpass','allpass']and bounded(value['frequency'],0,16384)and bounded(value['q'],-100,100)and bounded(value['detune'],-100,100))
 array(value['frequencies'],count,0,16384);array(value['magnitude'],count,0,100);array(value['phase'],phase_count,-4,4)
def check_event(value,run):
 require(type(value)is dict and value.get('run')==run and TOKEN.fullmatch(run)and type(value.get('sequence'))is int and 0<=value['sequence']<=3 and type(value.get('secure'))is bool)
 fields={'run','sequence','kind','secure'};kind=value.get('kind')
 if kind in ['swapped-delay-channel','mono-gain-delay']:
  fields|={'sample_rate','channels','frames','samples'};channels=2 if kind=='swapped-delay-channel'else 1
  require(bounded(value.get('sample_rate'),3000,384000)and type(value.get('channels'))is int and value['channels']==channels and type(value.get('frames'))is int and value['frames']==512)
  require(type(value.get('samples'))is list and len(value['samples'])==channels)
  for channel in value['samples']:array(channel,512,-2,2)
 elif kind=='biquad-response':
  fields|={'sample_rate','lowpass','allpass'};require(bounded(value.get('sample_rate'),3000,384000));filter_response(value.get('lowpass'),3,2);filter_response(value.get('allpass'),5,5)
 elif kind=='error':fields.add('code');require(value.get('code')in ['case-a','case-b','case-c'])
 else:raise ValueError('unknown offline event')
 require(set(value)==fields);return value
