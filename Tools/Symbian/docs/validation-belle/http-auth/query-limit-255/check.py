#!/usr/bin/env python3
"""Native SDK/portable controller checks only; no guest/display/debug shim."""
from pathlib import Path
import hashlib,json,os,re,resource,shlex,subprocess
W=Path(__file__).resolve().parent;R=Path('/workspace/WebKit-Symbian-graphics');S=Path('/workspace/symbian-sdk')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
commands=[]
def execute(name,cmd,cwd=None,expected=0):
 p=subprocess.run(cmd,cwd=cwd,capture_output=True,text=True,env={**os.environ,'SYMBIAN_SDK_PREFIX':str(S)},preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)))
 (W/(name+'.log')).write_text(p.stdout+p.stderr);commands.append({'name':name,'command':cmd,'cwd':str(cwd or W),'exit_code':p.returncode,'expected_exit':expected})
 if expected=='nonzero':assert p.returncode!=0,name
 else:assert p.returncode==expected,name+': '+(p.stdout+p.stderr)[-3000:]
 print(name,p.returncode,flush=True)
 return p
header=(S/'include/native/eikseced.h').read_text();bound=int(re.search(r'EMaxSecEdSecArrayLength\s*=\s*(\d+)',header)[1]);assert bound==255
assert header.count('@panic EIKCTL 13')>=2
fake=(W/'overlay/Tools/Symbian/tests/http-auth/fakes/eikseced.h').read_text();assert int(re.search(r'EMaxSecEdSecArrayLength\s*=\s*(\d+)',fake)[1])==bound
resources={}
for v in ['before','overlay']:
 gen=W/'generated'/v;gen.mkdir(parents=True,exist_ok=True)
 execute(v+'-resource-preprocess',[str(S/'bin/clang'),'-E','-P','-x','c','-I'+str(S/'include/platform'),'-I'+str(S/'include/native'),'-I'+str(W/'support/Tools/Symbian/browser'),str(W/v/'Tools/Symbian/browser/browser_ui.rss'),'-o',str(gen/'browser_ui.rpp')])
 execute(v+'-rcomp',[str(S/'bin/rcomp'),'-u','-s'+str(gen/'browser_ui.rpp'),'-o'+str(gen/'browser_ui.rsc'),'-h'+str(gen/'browser_ui.rsg')])
 source=(W/v/'Tools/Symbian/browser/browser_ui.rss').read_text();n=int(re.search(r'control\s*=\s*SECRETED\s*\{\s*num_letters\s*=\s*(\d+)',source)[1]);ids=dict(re.findall(r'#define\s+(R_BROWSER_\w+)\s+(0x[0-9a-fA-F]+)',(gen/'browser_ui.rsg').read_text()))
 resources[v]={'secret_num_letters':n,'within_official_limit':n<=bound,'ids':ids,'rsc_bytes':(gen/'browser_ui.rsc').stat().st_size,'rsc_sha256':sha(gen/'browser_ui.rsc')}
assert resources['before']['secret_num_letters']==256 and resources['overlay']['secret_num_letters']==255
assert resources['before']['ids']==resources['overlay']['ids']
a=(W/'generated/before/browser_ui.rsc').read_bytes();b=(W/'generated/overlay/browser_ui.rsc').read_bytes();assert len(a)==len(b)==242
changes=[{'offset':i,'before':x,'after':y} for i,(x,y) in enumerate(zip(a,b)) if x!=y];assert changes==[{'offset':169,'before':0,'after':255},{'offset':170,'before':1,'after':0}]
(W/'resource-contract.json').write_text(json.dumps({'status':'PASS','official_secret_limit':bound,'SDK_header_sha256':sha(S/'include/native/eikseced.h'),'resource_ids_preserved':True,'resources':resources,'compiled_rsc_only_changed_word':{'offset':169,'before_little_endian':256,'after_little_endian':255,'changed_bytes':changes}},indent=2)+'\n')
# Use the actual current ARM translation-unit flags and generated resource header.
db=Path('/workspace/symbian-graphics-work/build/browser-belle20/compile_commands.json');entry=next(x for x in json.loads(db.read_text()) if x['file'].endswith('/AuthenticationUI.cpp'))
vfs={'version':0,'roots':[{'type':'file','name':str(R/'Tools/Symbian/browser/AuthenticationUI.cpp'),'external-contents':str(W/'overlay/Tools/Symbian/browser/AuthenticationUI.cpp')},{'type':'file','name':'/workspace/symbian-graphics-work/build/browser-belle20/browser_ui.rsg','external-contents':str(W/'generated/overlay/browser_ui.rsg')}]};(W/'overlay.json').write_text(json.dumps(vfs,indent=2)+'\n')
base=shlex.split(entry['command']);i=base.index('-o');del base[i:i+2];base.remove('-c');base[1:1]=['-ivfsoverlay',str(W/'overlay.json')]
execute('AuthenticationUI-arm-syntax',base+['-fsyntax-only','-fno-color-diagnostics'],entry['directory'])
execute('AuthenticationUI-arm-object',base+['-c','-o',str(W/'AuthenticationUI.o'),'-fno-color-diagnostics'],entry['directory'])
# Existing native-flow cases plus exact API limits and valid boundary credentials.
includes=['-I'+str(W/'overlay/Tools/Symbian/tests/http-auth/fakes'),'-I'+str(W/'support/Tools/Symbian/tests/http-auth/fakes'),'-I'+str(W/'support/Tools/Symbian/browser'),'-I'+str(W/'support/Source/WebKitLegacy/symbian')]
common=['c++','-std=c++20','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']+includes
for v in ['before','overlay']:
 binary=W/(v+'-native-ui')
 execute(v+'-native-build',common+[str(W/'overlay/Tools/Symbian/tests/http-auth/native-ui.cpp'),str(W/v/'Tools/Symbian/browser/AuthenticationUI.cpp'),'-o',str(binary)])
 execute(v+'-native-run',[str(binary)],expected='nonzero' if v=='before' else 0)
(W/'check-commands.json').write_text(json.dumps(commands,indent=2)+'\n')
(W/'validation.json').write_text(json.dumps({'status':'PASS','scope':'SDK resource and actual ARM TU/object plus ASAN/UBSAN native-controller contract, no UI/network execution','username_maximum':256,'password_maximum':255,'SDK_secret_limit':bound,'resource_ids_preserved':True,'before_invalid_resource_detected':True,'before_api_misuse_rejected_by_host_contract':True,'overlay_host_success':True,'old_flow_cases_preserved':True,'all_native_allocations_released':True,'guest_ui_status':'NOT_RUN; frozen public calibration02/03 remain non-PASS','checks':commands},indent=2)+'\n')
