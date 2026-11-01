from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parent;root=r/'source';here=root/'Tools/Symbian/tests/native-filesystem'
with tempfile.TemporaryDirectory() as tmp:
 out=Path(tmp)
 argv=['c++','-std=c++20','-fshort-wchar','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(here/'include'),'-I'+str(root/'Source/WTF'),str(here/'filesystem.cpp'),str(root/'Source/WTF/wtf/symbian/FileSystemSymbian.cpp'),'-Wl,--gc-sections','-Wl,--wrap=read,--wrap=write,--wrap=rename,--wrap=lstat,--wrap=access,--wrap=getcwd,--wrap=opendir,--wrap=readdir,--wrap=fstat','-o',str(out/'filesystem')]
 with (r/'sanitized-build.log').open('w') as log:subprocess.run(argv,check=True,stdout=log,stderr=subprocess.STDOUT)
 (out/'C:').mkdir();(out/'E:').mkdir()
 with (r/'sanitized-run.log').open('w') as log:subprocess.run([str(out/'filesystem')],cwd=out,check=True,stdout=log,stderr=subprocess.STDOUT)
print('SANITIZED_HOST_FILESYSTEM_PASS')
