from pathlib import Path
import json,shlex,subprocess
root=Path('/workspace/EKA2L1-validation');base=root/'build';out=Path(__file__).resolve().parent
entries=[('mainwindow','src/emu/qt/src/mainwindow.cpp','src/emu/qt/CMakeFiles/eka2l1_qt.dir/src/mainwindow.cpp.o'),('dialogdriver','src/emu/qt/src/dialog_driver.cpp','src/emu/qt/CMakeFiles/eka2l1_qt.dir/src/dialog_driver.cpp.o'),('inputdialog','src/emu/qt/src/symbian_input_dialog.cpp','src/emu/qt/CMakeFiles/eka2l1_qt.dir/src/symbian_input_dialog.cpp.o'),('hui','src/emu/dispatch/src/hui.cpp','src/emu/dispatch/CMakeFiles/epocdispatch.dir/src/hui.cpp.o'),('register','src/emu/dispatch/src/register.cpp','src/emu/dispatch/CMakeFiles/epocdispatch.dir/src/register.cpp.o')]
records=[]
for key,file,target in entries:
 line=subprocess.check_output(['/workspace/symbian-sdk/bin/ninja','-C',str(base),'-t','commands',target],text=True).splitlines()[-1]
 args=shlex.split(line);prefix=2 if Path(args[0]).name=='ccache' else 1
 command=args[:prefix]+['-I'+str(out/'overlay/src/emu/drivers/include'),'-I'+str(out/'overlay/src/emu/dispatch/include'),'-I'+str(out/'overlay/src/emu/qt/include'),'-I'+str(root/'src/emu/qt/src')]
 skip=False
 for i,a in enumerate(args[prefix:],prefix):
  if skip:skip=False;continue
  if a in ('-MF','-MT','-o'):skip=True;continue
  if a=='-MD':continue
  command.append(str(out/'overlay'/file) if a==str(root/file) else a)
 command+=['-o',str(out/(key+'.o'))]
 with (out/(key+'-compile.log')).open('w') as f:r=subprocess.run(command,cwd=base,stdout=f,stderr=subprocess.STDOUT)
 records.append({'name':key,'command':command,'exit':r.returncode});(out/'compile-results.json').write_text(json.dumps(records,indent=2)+'\n')
 print(key,r.returncode,flush=True)
 if r.returncode:raise SystemExit(r.returncode)
