from pathlib import Path
import json,subprocess,shlex,re,hashlib
root=Path('/workspace/symbian-eka-fep-secret-work/guest')
base=Path('/workspace/symbian-fep-sdk-build-work/build')
row=next(e for e in json.loads((base/'compile_commands.json').read_text()) if e['file'].endswith('HostBridgedImeFEP.cpp'))
args=shlex.split(row['command']);args.insert(1,'-I'+str(root/'overlay/src/patch/avkonfep/inc'))
args.insert(2,'-I/workspace/symbian-fep-sdk-build-work/portable-candidate/Tools/Symbian/eka2l1/fep/official')
args[args.index('-o')+1]=str(root/'HostBridgedImeFEP.o');args[-1]=str(root/'overlay/src/patch/avkonfep/src/HostBridgedImeFEP.cpp')
commands=[args]
with (root/'arm-compile.log').open('w') as f:
 result=subprocess.run(args,cwd=base,stdout=f,stderr=subprocess.STDOUT)
 if result.returncode:raise SystemExit(result.returncode)
raw=(root/'overlay/src/patch/avkonfep/src/Dispatch.s').read_text()
raw=raw.replace('.include "../../../priv/inc/sv.S"','.include "/workspace/EKA2L1-validation/src/patch/priv/inc/sv.s"')
raw=re.sub(r'([A-Za-z_][A-Za-z0-9_]*):(\n[ \t]+CallHleDispatch[^\n]+)',r'.type \1, %function\n\1:\2\n.size \1, .-\1',raw)
(root/'Dispatch.S').write_text(raw)
args=['/workspace/symbian-sdk/bin/clang++','--target=armv6-none-eabi','-march=armv6','-marm','-c',str(root/'Dispatch.S'),'-o',str(root/'Dispatch.o')];commands.append(args)
with (root/'arm-dispatch.log').open('w') as f:subprocess.run(args,stdout=f,stderr=subprocess.STDOUT,check=True)
(root/'arm-commands.json').write_text(json.dumps(commands,indent=2)+'\n')
for obj in ('HostBridgedImeFEP.o','Dispatch.o'):
 s=subprocess.check_output(['/workspace/symbian-sdk/bin/llvm-nm',str(root/obj)],text=True)
 (root/(obj+'.symbols.txt')).write_text(s)
 assert 'EHUIOpenGlobalTextViewWithSecret' in s
print('PASS: actual native-leaves ARM FEP object and A0/A6 dispatch object')
