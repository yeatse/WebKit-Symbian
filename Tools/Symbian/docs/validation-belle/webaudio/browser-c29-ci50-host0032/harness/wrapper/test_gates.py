#!/usr/bin/env python3
"""Synthetic acceptance guards only; never starts guest/server/display/audio."""
import argparse,ast,copy,importlib.util,json,tempfile
from pathlib import Path
import run as wrapper
def main():
 checks=0
 good={'host_exit':0,'wm_close_sent':True,'wm_close_after_functional':True,'forced_cleanup':False,'deadline_expired':False,'inputs_unchanged':True,'owned_host_stopped_before_restore':True,'restoration':{'guest':{'restored':True},'configuration':{'restored':True}}};validation={'status':'PUBLIC_API_COMPLETE','native_audio_verified':False}
 accepted=wrapper.completion_gate(good,b'',b'',validation);assert accepted['status']=='CI_BROWSER_PAYLOAD_RUN_COMPLETE'and accepted['native_audio_verified']is False;checks+=1
 for key,value in [('host_exit',1),('host_exit',True),('wm_close_sent',False),('wm_close_after_functional',False),('forced_cleanup',True),('deadline_expired',True),('inputs_unchanged',False),('owned_host_stopped_before_restore',False)]:
  state=copy.deepcopy(good);state[key]=value
  try:wrapper.completion_gate(state,b'',b'',validation)
  except RuntimeError:checks+=1
  else:raise AssertionError(key+' accepted')
 for category in ['guest','configuration']:
  state=copy.deepcopy(good);state['restoration'][category]['restored']=False
  try:wrapper.completion_gate(state,b'',b'',validation)
  except RuntimeError:checks+=1
  else:raise AssertionError('incomplete restore accepted')
 for log in [b'Thread WebKit panicked with code 3',b'Thread Main terminated peacefully with category: None and exit code: 1',b'SIGABRT',b'Unable to launch process']:
  try:wrapper.completion_gate(good,b'',log,validation)
  except RuntimeError:checks+=1
  else:raise AssertionError('fatal accepted with host0')
 for value in [{'status':'OFFLINE_API_COMPLETE','native_audio_verified':False},{'status':'FAIL','native_audio_verified':False},{'status':'PUBLIC_API_COMPLETE','native_audio_verified':True}]:
  try:wrapper.completion_gate(good,b'',b'',value)
  except RuntimeError:checks+=1
  else:raise AssertionError('partial/wrong validator accepted')
 line=b'console: WEBAUDIO-RESULT {"run":"fresh","kind":"result","status":"PUBLIC_API_COMPLETE"}\n';assert wrapper.result_record(line,'stale')is None;checks+=1
 assert wrapper.result_record(line,'fresh')['status']=='PUBLIC_API_COMPLETE';checks+=1
 try:wrapper.result_record(line+line,'fresh')
 except RuntimeError:checks+=1
 else:raise AssertionError('duplicate result accepted')
 for browser,expected in [(None,'a'*64),(Path('/missing-browser.exe'),None),(Path('/missing-browser.exe'),'PENDING'),(Path('/missing-browser.exe'),'a'*64)]:
  args=argparse.Namespace(browser=browser,browser_sha256=expected,resource_dir=None)
  try:wrapper.candidate_pins(args)
  except RuntimeError:checks+=1
  else:raise AssertionError('missing/invalid candidate pin accepted')
 spec=importlib.util.spec_from_file_location('preserved',wrapper.STATIC['preserved_runner'][0]);preserved=importlib.util.module_from_spec(spec);spec.loader.exec_module(preserved)
 for key in ['preservation','configuration','close_window']:assert wrapper.STATIC[key]==preserved.INPUTS[key];checks+=1
 assert 'icu'not in wrapper.STATIC and'fontconfig'not in wrapper.STATIC and not hasattr(wrapper,'RESOURCES');checks+=1
 assert 'CMakeCache.txt'not in Path(wrapper.__file__).read_text()and 'BUILD_INPUTS'not in Path(wrapper.__file__).read_text();checks+=1
 tree=ast.parse(Path(wrapper.__file__).read_text());calls=[node for node in ast.walk(tree)if isinstance(node,ast.Call)and isinstance(node.func,ast.Attribute)and node.func.attr=='PrivateConfigurations']
 required=ast.dump(ast.parse("(DATA/'devices.yml',False)",mode='eval').body)
 assert len(calls)==1 and isinstance(calls[0].args[1],ast.List)and required in [ast.dump(row)for row in calls[0].args[1].elts];checks+=1
 rows=wrapper.native_rows();assert len(rows)==14 and len({row['path']for row in rows})==14 and not any('.elf.map'in row['path']for row in rows);checks+=1
 assert wrapper.NATIVE['mediaclientaudiostream_general.dll']==(40384,'8559122f1caeea69e9612ee68f641f9ca90501a12183c5fbd3b77368cdbede04');checks+=1
 assert wrapper.NATIVE['mediaclientaudiostream.dll.map']==(5027,'e92b9ede1b1a11355120e7a661be025f795e86841deec5c41b78fe7b691bc4ce');checks+=1
 assert 'Source/WebCore/Modules/webaudio/AudioNodeOutput.cpp'in wrapper.REQUIRED_SOURCES and len(wrapper.REQUIRED_SOURCES)==12;checks+=1
 args=argparse.Namespace(browser=Path(__file__).resolve(),browser_sha256=wrapper.C25_SHA,resource_dir=None)
 try:wrapper.candidate_pins(args)
 except RuntimeError:checks+=1
 else:raise AssertionError('known C25 SHA accepted')
 for path,expected in [(None,'a'*64),(Path('/missing-metadata.json'),None),(Path('/missing-metadata.json'),'PENDING')]:
  args=argparse.Namespace(browser=Path('/future-c29/browser.exe'),browser_sha256='a'*64,candidate_metadata=path,candidate_metadata_sha256=expected,ci_source_commit=wrapper.CI_SOURCE_COMMIT)
  try:wrapper.candidate_metadata_pins(args)
  except RuntimeError:checks+=1
  else:raise AssertionError('missing/invalid metadata pin accepted')
 value={'schema':'classic-webaudio-c29-v1','candidate_id':'C29','browser':{'path':'/future-c29/browser.exe','sha256':'a'*64},'enable_web_audio':True,'runtime_audio_worklet_enabled':False,'build_inputs':[],'build_identities':{},'source_identities':{},'source_files':[]}
 with tempfile.TemporaryDirectory(prefix='metadata-guards-',dir=wrapper.ROOT)as directory:
  path=Path(directory)/'metadata.json'
  for key,replacement in [('candidate_id','C25'),('enable_web_audio',False),('runtime_audio_worklet_enabled',True),('browser',{'path':'/old/browser.exe','sha256':'a'*64})]:
   bad=copy.deepcopy(value);bad[key]=replacement;path.write_text(json.dumps(bad));args=argparse.Namespace(browser=Path('/future-c29/browser.exe'),browser_sha256='a'*64,candidate_metadata=path,candidate_metadata_sha256=wrapper.sha(path),ci_source_commit=wrapper.CI_SOURCE_COMMIT)
   try:wrapper.candidate_metadata_pins(args)
   except RuntimeError:checks+=1
   else:raise AssertionError('wrong C29 feature/source metadata accepted')
 print(json.dumps({'status':'GUARDS_PASS_NOT_RUNTIME','checks':checks,'synthetic_only':True,'runtime_started':False,'native_audio_verified':False}))
if __name__=='__main__':main()
