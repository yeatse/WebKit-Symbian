
#include <cstdint>
#include <iostream>
namespace fixture {
struct system {};
namespace epoc {struct desc16 {};struct request_status {};}
namespace eka2l1 {template<class T> struct ptr {std::uint32_t address;};}
#define BRIDGE_FUNC_DISPATCHER(ret, name, ...) ret name(system *sys, const std::uint32_t func_num, ##__VA_ARGS__)
int length_seen;bool secret_seen;std::uint32_t status_seen;epoc::desc16* text_seen;
std::int32_t open_input_view(system*,epoc::desc16* text,std::int32_t length,bool secret,eka2l1::ptr<epoc::request_status> status){text_seen=text;length_seen=length;secret_seen=secret;status_seen=status.address;return 0;}

BRIDGE_FUNC_DISPATCHER(std::int32_t, ehui_open_input_view, epoc::desc16 *initial_text, const std::int32_t max_length, eka2l1::ptr<epoc::request_status> status) {
        return open_input_view(sys, initial_text, max_length, false, status);
    }
BRIDGE_FUNC_DISPATCHER(std::int32_t, ehui_open_input_view_with_secret, epoc::desc16 *initial_text, const std::int32_t max_length, const std::int32_t secret, eka2l1::ptr<epoc::request_status> status) {
        return open_input_view(sys, initial_text, max_length, secret != 0, status);
    }

int run(){system s;epoc::desc16 text;unsigned checks=0;
#define CHECK(value) do{++checks;if(!(value)){std::cerr<<"selector failure "<<checks<<"\n";return 1;}}while(false)
CHECK(ehui_open_input_view(&s,0xA0,&text,64,{17})==0);CHECK(!secret_seen);CHECK(length_seen==64&&status_seen==17&&text_seen==&text);
for(int secret:{0,1,256}){CHECK(ehui_open_input_view_with_secret(&s,0xA6,&text,1048576,secret,{23})==0);CHECK(secret_seen==(secret!=0));CHECK(length_seen==1048576&&status_seen==23&&text_seen==&text);}
std::cout<<checks<<" production selector parameter checks PASS; no guest/display\n";return 0;}

}
int main(){return fixture::run();}
