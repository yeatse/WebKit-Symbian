from pathlib import Path
import subprocess
out=Path(__file__).resolve().parent
text=(out/'overlay/src/emu/dispatch/src/hui.cpp').read_text()
def method(signature):
 start=text.index(signature);i=text.index('{',start);depth=1;i+=1
 while depth:
  if text[i]=='{':depth+=1
  if text[i]=='}':depth-=1
  i+=1
 return text[start:i]
functions='\n'.join(method(s) for s in ('BRIDGE_FUNC_DISPATCHER(std::int32_t, ehui_open_input_view,','BRIDGE_FUNC_DISPATCHER(std::int32_t, ehui_open_input_view_with_secret,'))
prefix=r'''
#include <cstdint>
#include <iostream>
namespace fixture {
struct system {};
namespace epoc {struct desc16 {};struct request_status {};}
namespace eka2l1 {template<class T> struct ptr {std::uint32_t address;};}
#define BRIDGE_FUNC_DISPATCHER(ret, name, ...) ret name(system *sys, const std::uint32_t func_num, ##__VA_ARGS__)
int length_seen;bool secret_seen;std::uint32_t status_seen;epoc::desc16* text_seen;
std::int32_t open_input_view(system*,epoc::desc16* text,std::int32_t length,bool secret,eka2l1::ptr<epoc::request_status> status){text_seen=text;length_seen=length;secret_seen=secret;status_seen=status.address;return 0;}
'''
main=r'''
int run(){system s;epoc::desc16 text;unsigned checks=0;
#define CHECK(value) do{++checks;if(!(value)){std::cerr<<"selector failure "<<checks<<"\n";return 1;}}while(false)
CHECK(ehui_open_input_view(&s,0xA0,&text,64,{17})==0);CHECK(!secret_seen);CHECK(length_seen==64&&status_seen==17&&text_seen==&text);
for(int secret:{0,1,256}){CHECK(ehui_open_input_view_with_secret(&s,0xA6,&text,1048576,secret,{23})==0);CHECK(secret_seen==(secret!=0));CHECK(length_seen==1048576&&status_seen==23&&text_seen==&text);}
std::cout<<checks<<" production selector parameter checks PASS; no guest/display\n";return 0;}
'''
source=out/'selector-tests.cpp';source.write_text(prefix+'\n'+functions+'\n'+main+'\n}\nint main(){return fixture::run();}\n')
with (out/'selector-build.log').open('w') as f:subprocess.run(['g++','-std=c++20',str(source),'-o',str(out/'selector-tests')],stdout=f,stderr=subprocess.STDOUT,check=True)
with (out/'selector-test.log').open('w') as f:subprocess.run([str(out/'selector-tests')],stdout=f,stderr=subprocess.STDOUT,check=True)
print((out/'selector-test.log').read_text())
