#!/usr/bin/env python3
"""Apply unchanged strict validator; root UI observations never inferred."""
import argparse,json
from pathlib import Path
from support import ROOT,sha,load_module,require
from public_review import validate_public_evidence
FLAGS=('native_query_reviewed','native_query_dismissed_reviewed','masked_password_reviewed')
def finish(output,review=None,suffix='reviewed'):
 output=Path(output)
 required=('observer-result.json','fixture/ready.json','fixture/events.jsonl','interaction.json','guest.log','emulator.log')
 missing=[name for name in required if not (output/name).is_file()]
 if missing:
  result={'status':'FAIL','strict_validator_status':'NOT_RUN_MISSING_REQUIRED_EVIDENCE','failures':['missing required fresh evidence: '+', '.join(missing)]}
  target=output/('validation-'+suffix+'.json');require(not target.exists(),'do not replace earlier validation');target.write_text(json.dumps(result,indent=2)+'\n');return result
 observer=json.loads((output/'observer-result.json').read_text());ready=json.loads((output/'fixture/ready.json').read_text());pins=json.loads((ROOT/'pins.json').read_text())['files']
 require(sha(ROOT/'upstream/validate.py')==pins['upstream_validate.py']['sha256'],'strict validator source changed')
 report={'run':ready['run'],'url':ready['url'],'browser_sha256':observer['inputs']['browser']['sha256'],'emulator_sha256':observer['inputs']['host']['sha256'],'host_exit_code':observer.get('host_exit_code'),'normal_close':observer.get('normal_close',False),'forced_cleanup':observer.get('forced_cleanup',False),'wm_close_sent':observer.get('wm_close_sent',False),'curl_trace_disabled':observer.get('curl_trace_disabled',False),'finished_utc':observer.get('finished_utc'),**{k:False for k in FLAGS}}
 if review:
  value=json.loads(Path(review).read_text());require(set(value)=={'run','url',*FLAGS,'native_query_dismissed_observation'},'UI observation requires run/URL, three explicit booleans and this-run closure observation');require(value['run']==ready['run'] and value['url']==ready['url'],'UI observation belongs to another run/URL');require(all(type(value[k])is bool for k in FLAGS),'UI observation flags must be boolean')
  require(isinstance(value['native_query_dismissed_observation'],str) and len(value['native_query_dismissed_observation'])<=2000,'closure observation must be bounded text')
  if value['native_query_dismissed_reviewed'] and not ready['case'].startswith('exit'):require(bool(value['native_query_dismissed_observation'].strip()),'true dismissal review requires Root explanation of this-run trusted probe/closed query')
  report.update({k:value[k] for k in FLAGS});report['native_query_dismissed_observation']=value['native_query_dismissed_observation']
 report_path=output/('runner-report-'+suffix+'.json');require(not report_path.exists(),'do not replace an earlier runner observation');report_path.write_text(json.dumps(report,indent=2)+'\n')
 validator=load_module('strict_auth_validator',ROOT/'upstream/validate.py');helper=output/'app-exit-request.log'
 try:strict=validator.validate(output/'fixture',output/'interaction.json',output/'guest.log',output/'emulator.log',report_path,helper if helper.exists() else None)
 except (OSError,ValueError,TypeError,KeyError) as error:strict={'status':'FAIL','failures':[type(error).__name__+': malformed or missing evidence']}
 strict_path=output/('strict-validation-'+suffix+'.json');require(not strict_path.exists(),'do not replace earlier strict validation');strict_path.write_text(json.dumps(strict,indent=2)+'\n')
 revised=load_module('revised_auth_validator',ROOT/'revised_validator.py')
 try:
  interaction=json.loads((output/'interaction.json').read_text());probe=validate_public_evidence(interaction,output,output/'guest.log')
  result=revised.validate(output/'fixture',output/'interaction.json',output/'guest.log',output/'emulator.log',report_path,helper if helper.exists() else None,observer=observer,public_probe=probe)
 except (OSError,ValueError,TypeError,KeyError,RuntimeError) as error:result={'status':'FAIL','failures':[type(error).__name__+': malformed or missing revised evidence']}
 result['strict_validator_status']=strict['status'];result['strict_validator_file']=strict_path.name;outer=[]
 if observer.get('target_guest_failed'):outer.append('explicit WebKit target-thread panic in fresh host log')
 if observer.get('driver_contract',{}).get('status')=='FAIL':outer.append('native input completion contract failed')
 if observer.get('status')=='FAIL':outer.append('observer/input/normal-close or restoration failed')
 if not observer.get('inputs_unchanged'):outer.append('pinned public inputs changed during run')
 if not observer.get('private_config_profile_restored') or not observer.get('restoration',{}).get('restored'):outer.append('original config/profile/resources not fully restored')
 if outer:result['status']='FAIL';result['observer_failures']=outer
 target=output/('validation-'+suffix+'.json');require(not target.exists(),'do not replace earlier validation');target.write_text(json.dumps(result,indent=2)+'\n');return result
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--review',type=Path,required=True);p.add_argument('--suffix',default='reviewed');a=p.parse_args();require(a.suffix.isidentifier(),'simple fresh suffix required');r=finish(a.output,a.review,a.suffix);print(json.dumps(r,indent=2));return {'PASS':0,'FAIL':1,'INCOMPLETE':2}[r['status']]
if __name__=='__main__':raise SystemExit(main())
