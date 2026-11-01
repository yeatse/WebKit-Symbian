#!/usr/bin/env python3
"""Root-only metadata freezing after actual C28 success; no deployment or runtime."""
import argparse,json
from pathlib import Path
import run as wrapper
def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--browser',required=True,type=Path);parser.add_argument('--browser-sha256',required=True);parser.add_argument('--output',required=True,type=Path);args=parser.parse_args()
 try:
  wrapper.require(args.output.is_absolute()and args.output==args.output.resolve()and not args.output.exists(),'fresh canonical absolute metadata output required')
  for parent in args.output.parents:wrapper.require(not parent.is_symlink(),'metadata output symlink ancestor refused')
  wrapper.require(args.browser.parent==wrapper.CANDIDATE_ROOT,'freeze only the actual candidate28 delivery')
  wrapper.candidate_pins(argparse.Namespace(browser=args.browser,browser_sha256=args.browser_sha256,resource_dir=None))
  wrapper.require(wrapper.sha(wrapper.safe_file(args.browser))==args.browser_sha256,'actual browser bytes differ from Root frozen SHA-256')
  value={'schema':'classic-webaudio-c28-v1','candidate_id':'C28','browser':{'path':str(args.browser),'sha256':args.browser_sha256},'enable_web_audio':True,'runtime_audio_worklet_enabled':False,'build_inputs':[],'build_identities':{},'source_identities':{},'source_files':[]}
  for role,path in wrapper.BUILD_INPUTS.items():value['build_inputs'].append({'role':role,'path':str(wrapper.safe_file(path)),'sha256':wrapper.sha(path)})
  for field,filename in [('build_identities','build-identities.json'),('source_identities','source-identities.json')]:
   path=wrapper.safe_file(wrapper.CANDIDATE_ROOT/filename);value[field]={'path':str(path),'sha256':wrapper.sha(path)}
  for relative in sorted(wrapper.REQUIRED_SOURCES):
   path=wrapper.CANDIDATE_ROOT/'source'/relative
   if not path.is_file():
    matches=list((wrapper.CANDIDATE_ROOT/'source').rglob(Path(relative).name));wrapper.require(len(matches)==1,'unique C28 source snapshot missing: '+relative);path=matches[0]
   path=wrapper.safe_file(path);expected=wrapper.sha(path);actual=wrapper.safe_file(Path('/workspace/WebKit-Symbian-graphics')/relative);wrapper.require(wrapper.sha(actual)==expected,'C28 snapshot differs from actual build source: '+relative);value['source_files'].append({'relative':relative,'path':str(path),'sha256':expected})
  temp=args.output.with_name(args.output.name+'.prepare-tmp');wrapper.require(not temp.exists()and not temp.is_symlink(),'fresh metadata temporary required');temp.write_text(json.dumps(value,indent=2)+'\n')
  try:
   pins=wrapper.candidate_metadata_pins(argparse.Namespace(browser=args.browser,browser_sha256=args.browser_sha256,candidate_metadata=temp,candidate_metadata_sha256=wrapper.sha(temp)))
   for path,expected in pins.values():wrapper.require(wrapper.sha(wrapper.safe_file(path))==expected,'metadata input changed while freezing')
   temp.replace(args.output)
  finally:temp.unlink(missing_ok=True)
  print(json.dumps({'status':'FROZEN_METADATA_ONLY','path':str(args.output),'sha256':wrapper.sha(args.output),'runtime_started':False,'native_audio_verified':False},indent=2));return 0
 except(OSError,ValueError,TypeError,RuntimeError)as error:print('FAIL: '+str(error));return 1
if __name__=='__main__':raise SystemExit(main())
