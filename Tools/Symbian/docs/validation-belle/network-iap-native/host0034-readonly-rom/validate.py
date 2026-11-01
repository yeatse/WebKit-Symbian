#!/usr/bin/env python3
"""Strict public normal-API record parser, never storage or routing approval."""
import argparse,json,re
from pathlib import Path
BOOLS={'initial_enumerated','start_requested','start_completed','timed_out','progress_connected',
       'iap_available','network_available','service_available','stable_snapshot','owned_handle_closed'}
RCS={'rc','start_rc','iap_rc','network_rc','service_rc'}
COUNTS={'initial_connections','active_connections'}
FIELDS={'suite','version','run','status',*BOOLS,*RCS,*COUNTS}

def strict_json(text):
 def pairs(rows):
  out={}
  for key,value in rows:
   if key in out:raise ValueError('duplicate field')
   out[key]=value
  return out
 def constant(_):raise ValueError('nonfinite value')
 return json.loads(text,object_pairs_hook=pairs,parse_constant=constant)

def validate(text,token):
 if not isinstance(token,str)or not re.fullmatch(r'[A-Za-z0-9_-]{8,64}',token):raise ValueError('token')
 if not isinstance(text,str)or len(text.encode())>16384:raise ValueError('bounded public record required')
 lines=text.splitlines()
 if len(lines)!=3 or not lines[0].startswith('BEGIN ')or not lines[1].startswith('RESULT ')or lines[2]!='iap-owned-start exiting 0':raise ValueError('fixed fresh completion')
 begin,result=strict_json(lines[0][6:]),strict_json(lines[1][7:])
 identity={'suite':'iap-owned-start','version':1,'run':token}
 if begin!=identity or type(begin.get('version'))is not int or not isinstance(result,dict)or set(result)!=FIELDS or any(result[k]!=v for k,v in identity.items())or type(result['version'])is not int:raise ValueError('identity/whitelist')
 if any(type(result[k])is not bool for k in BOOLS):raise ValueError('boolean fields')
 if any(type(result[k])is not int or not -(2**31)<=result[k]<2**31 for k in RCS):raise ValueError('native status fields')
 if any(type(result[k])is not int or not 0<=result[k]<2**32 for k in COUNTS):raise ValueError('aggregate fields')
 if result['status']not in ['AVAILABLE','UNAVAILABLE']or(result['status']=='AVAILABLE')!=(result['rc']==0):raise ValueError('status consistency')
 if result['start_requested']and(not result['initial_enumerated']or result['initial_connections']!=0):raise ValueError('existing connection owner refused')
 if result['start_requested']and not result['owned_handle_closed']:raise ValueError('owned close absent')
 if result['start_completed']and not result['start_requested']:raise ValueError('start completion without request')
 if result['timed_out']and(not result['start_requested']or result['rc']!=-33):raise ValueError('timeout consistency')
 if result['stable_snapshot']and not all(result[k]for k in ['progress_connected','iap_available','network_available','service_available']):raise ValueError('stable snapshot fields')
 if result['status']=='AVAILABLE':
  if result['timed_out']or not all(result[k]for k in BOOLS-{'timed_out'})or any(result[k]!=0 for k in RCS)or result['initial_connections']!=0 or result['active_connections']!=1:raise ValueError('complete available capability')
 return {'parse_status':'PASS','run':token,'capability':result['status'],'native_status':result['rc'],
         'native_api_statuses':{k:result[k]for k in sorted(RCS-{'rc'})},
         'timed_out':result['timed_out'],'owned_handle_closed':result['owned_handle_closed'],
         'stable_snapshot':result['stable_snapshot'],'routing_verified':False,
         'storage_scope_verified_by_record':False,'requires_outer_health_and_restore':True}

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--log',type=Path,required=True);parser.add_argument('--run',required=True);args=parser.parse_args()
 try:
  if not args.log.is_file()or args.log.is_symlink()or args.log.stat().st_size>16384:raise ValueError('public log required')
  result=validate(args.log.read_text(),args.run)
 except(ValueError,TypeError,KeyError,OSError):result={'parse_status':'FAIL','routing_verified':False};print(json.dumps(result));return 1
 print(json.dumps(result));return 0
if __name__=='__main__':raise SystemExit(main())
