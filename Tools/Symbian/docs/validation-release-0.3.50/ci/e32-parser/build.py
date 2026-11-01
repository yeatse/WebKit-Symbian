from pathlib import Path
import hashlib,json,subprocess
r=Path(__file__).resolve().parent;e=Path('/workspace/EKA2L1-validation');b=e/'build';incs=[e/'src/emu/common/include',e/'src/emu/loader/include',b/'src/emu/common/include',e/'src/external/fmt/include',e/'src/external/spdlog/include',Path('/workspace/host-sysroot/usr/include')]
compile=['/usr/bin/g++','-std=c++17','-O2','-fno-exceptions','-fno-rtti',*['-I'+str(x) for x in incs],'-c',str(r/'inspect-general.cpp'),'-o',str(r/'inspect-general.o')]
libs=[b/'src/emu/loader/libepocloader.a',b/'src/emu/common/libcommon.a',b/'src/external/fmt/libfmt.a',b/'src/external/spdlog/libspdlog.a',b/'src/external/re2/libre2.a',b/'src/external/libzlibstatic.a']
link=['/usr/bin/g++',str(r/'inspect-general.o'),'-Wl,--start-group',*[str(x) for x in libs],'-Wl,--end-group','-pthread','-o',str(r/'inspect-general')]
with (r/'build.log').open('w') as log:
 for argv in [compile,link]:subprocess.run(argv,stdout=log,stderr=subprocess.STDOUT,check=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
(r/'build-identities.json').write_text(json.dumps({'method':'Existing EKA2L1 normal E32 parser and read-only metadata frontend','compile_argv':compile,'link_argv':link,'source_sha256':sha(r/'inspect-general.cpp'),'tool_sha256':sha(r/'inspect-general'),'tool_bytes':(r/'inspect-general').stat().st_size,'libraries':{str(p):sha(p) for p in libs},'parser_source_sha256':sha(e/'src/emu/loader/src/e32img.cpp'),'guest_or_loader_execution':False},indent=2)+'\n')
print('GENERAL_E32_PARSER_FRONTEND_BUILT')
