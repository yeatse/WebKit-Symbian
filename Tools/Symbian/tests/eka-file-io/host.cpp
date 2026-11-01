#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#define LOG_ERROR(...) ((void)0)
#define LOG_WARN(...) ((void)0)
enum prot {prot_read=1,prot_write=2,prot_read_write=3};
namespace eka2l1 {
using address=uint32_t;
namespace mem {
struct page_info {void* host_addr=nullptr;int perm=prot_read_write;};
static std::unordered_map<uint32_t,page_info> pages;
struct control_base {
    page_info* get_page_info(int,uint32_t a){auto it=pages.find(a/4096);return it==pages.end()?nullptr:&it->second;}
    void* get_host_pointer(int id,uint32_t a){auto*p=get_page_info(id,a);return p&&p->host_addr?static_cast<char*>(p->host_addr)+(a%4096):nullptr;}
};
struct model {int address_space_id(){return 1;}};
}
namespace kernel {struct process {mem::model model;mem::model* get_mem_model(){return &model;}};}
struct memory_system {mem::control_base control;mem::control_base* get_control(){return &control;}int get_page_size(){return 4096;}};
static uint32_t next_address=0x10000;
static uint32_t bind(void* p,size_t n=16){
    if(!p)return 0;
    uint32_t address=next_address;next_address+=static_cast<uint32_t>(((n+4095)/4096+1)*4096);
    for(size_t off=0;off<n;off+=4096)mem::pages[(address+off)/4096]={static_cast<char*>(p)+off,prot_read_write};
    return address;
}
template<class T> struct ptr {
    uint32_t address=0;
    ptr()=default; explicit ptr(uint32_t a):address(a){}
    T* get(kernel::process*) const {mem::control_base c;return static_cast<T*>(c.get_host_pointer(1,address));}
    uint32_t ptr_address()const{return address;}
    template<class U> ptr<U> cast()const {return ptr<U>(address);}
};
namespace epoc {
#include "actual-descriptor.inc"
#include "actual-descriptor-methods.inc"
enum { error_none=0,error_general=-2,error_no_memory=-4,error_argument=-6,error_bad_handle=-8,error_overflow=-9,error_bad_descriptor=-38,error_access_denied=-21,error_write=-23 };
namespace fs { constexpr uint32_t ipc_arg_slot2_des=0x40000; }
}
enum class ipc_arg_type:int {flag_des=4,flag_16b=1,flag_const=2,des8=4,desc8=6,des16=5};
struct arglist { uint32_t args[4]{}; ipc_arg_type kind=ipc_arg_type::des8; ipc_arg_type get_arg_type(int)const{return kind;} };
struct thread {kernel::process process;kernel::process* owning_process(){return &process;}};
struct message {arglist args;uint32_t function=0;thread own;thread* own_thr=&own;};
struct kernel_system {bool eka1=false;bool is_eka1(){return eka1;}};
struct system {kernel_system kern;memory_system memory;kernel_system* get_kernel_system(){return &kern;}memory_system* get_memory_system(){return &memory;}};
enum class io_component_type {file,other};
enum class file_seek_mode {beg,end};
constexpr int WRITE_MODE=2;
struct file {
    io_component_type type=io_component_type::file;
    std::vector<char> bytes;uint64_t cursor=0;size_t writes=0,reads=0,seeks=0;
    bool fail_seek=false,fail_size=false,short_write=false,short_read=false,invalid_read=false,truncate_read=false;
    bool invalid_write=false;std::FILE* disk=nullptr;
    ~file(){if(disk)std::fclose(disk);}
    uint64_t size()const{if(fail_size)return UINT64_MAX; if(disk){auto p=std::ftell(disk);std::fseek(disk,0,SEEK_END);auto n=std::ftell(disk);std::fseek(disk,p,SEEK_SET);return n;}return bytes.size();}
    uint64_t tell(){return cursor;}
    uint64_t seek(int64_t n,file_seek_mode mode){++seeks;if(fail_seek)return UINT64_MAX;uint64_t target=mode==file_seek_mode::end?size()+n:n;if(disk&&std::fseek(disk,static_cast<long>(target),SEEK_SET))return UINT64_MAX;return cursor=target;}
    size_t write_file(const void* p,uint32_t size,uint32_t count){++writes;size_t n=size_t(size)*count;if(invalid_write)return SIZE_MAX;if(short_write)n/=2;if(disk){auto r=std::fwrite(p,1,n,disk);cursor+=r;return r;}if(cursor+n>bytes.size())bytes.resize(cursor+n);std::memcpy(bytes.data()+cursor,p,n);cursor+=n;return n;}
    size_t read_file(void* p,uint32_t size,uint32_t count){++reads;size_t n=size_t(size)*count;if(invalid_read)return SIZE_MAX;if(short_read||truncate_read)n/=2;if(disk){auto r=std::fread(p,1,n,disk);cursor+=r;return r;}n=std::min<uint64_t>(n,bytes.size()-std::min<uint64_t>(cursor,bytes.size()));std::memcpy(p,bytes.data()+cursor,n);cursor+=n;if(truncate_read)bytes.resize(cursor);return n;}
};
struct fs_node {std::unique_ptr<file> vfs_node=std::make_unique<file>();int open_mode=WRITE_MODE;};
namespace service {
struct ipc_context {
    eka2l1::system system;eka2l1::system* sys=&system;message message_;message* msg=&message_;
    std::optional<int32_t> handle=1,length=0,position=-1;std::optional<int64_t> position64=-1;
    int status=777,completions=0;bool reject_output=false;
    template<class T> std::optional<T> get_argument_value(int i){
        if constexpr(std::is_same_v<T,std::string>){auto*d=ptr<epoc::desc8>(msg->args.args[0]).get(msg->own_thr->owning_process());if(!d)return{};return d->to_std_string(msg->own_thr->owning_process());}
        else {auto v=i==3?handle:i==1?length:position;return v?std::optional<T>(*v):std::nullopt;}}
    template<class T> std::optional<T> get_argument_data_from_descriptor(int){return position64?std::optional<T>(*position64):std::nullopt;}
    bool write_data_to_descriptor_argument(int,const uint8_t* data,uint32_t count,int* error=nullptr){if(reject_output){if(error)*error=epoc::error_bad_descriptor;return false;}auto*d=ptr<epoc::desc8>(msg->args.args[0]).get(msg->own_thr->owning_process());if(!d)return false;return d->assign(msg->own_thr->owning_process(),data,count)==0;}
    void complete(int code){status=code;++completions;}
};
}
struct fs_server_client {fs_node node;bool bad_handle=false;fs_node* get_file_node(int h){return bad_handle||h!=1?nullptr:&node;}void file_write(service::ipc_context*);void file_read(service::ipc_context*);};
#include "actual-file-io.inc"
}
using namespace eka2l1;
static unsigned checks=0;
static void check(bool v,const char* what){++checks;if(!v){std::cerr<<"FAIL "<<checks<<": "<<what<<'\n';std::exit(1);}}
struct request {
    fs_server_client client;service::ipc_context ctx;epoc::ptr_des8 descriptor;std::vector<char> data;
    epoc::ptr_desc8 position_descriptor;int64_t position_value=0;
    explicit request(size_t n=16):data(n,'x') {descriptor.set_descriptor_type(epoc::ptr);descriptor.set_max_length(n);descriptor.set_length(nullptr,n);descriptor.data=ptr<char>(bind(n?data.data():nullptr,n));ctx.msg->args.args[0]=bind(&descriptor);ctx.length=n;}
    void position64(int64_t value){position_value=value;position_descriptor.info=(epoc::ptr_const<<28)|8;position_descriptor.data=ptr<char>(bind(&position_value,8));ctx.msg->args.args[2]=bind(&position_descriptor);ctx.msg->function=epoc::fs::ipc_arg_slot2_des;}
    file& f(){return *client.node.vfs_node;}
    int write(){client.file_write(&ctx);check(ctx.completions==1,"write completes once");return ctx.status;}
    int read(){client.file_read(&ctx);check(ctx.completions==1,"read completes once");return ctx.status;}
};
int main(){
    for(uint32_t type=0;type<epoc::des_type_end;++type){
        for(uint32_t len:{0u,1u,0xFFFFFFu,0x1000000u,0x1000001u,34u*1024*1024,0xFFFFFFFu}){
            epoc::ptr_des8 d;d.max_length=0xFFFFFFF;d.info=(type<<28)|len;
            check(d.get_length()==len,"all 28 length bits");check(d.get_descriptor_type()==type,"type separate from length");
            d.set_descriptor_type(epoc::ptr);check(d.get_length()==len,"change type preserves 28 length bits");
            d.set_length(nullptr,7);check(d.get_length()==7&&d.get_descriptor_type()==epoc::ptr,"shrink clears old high length bits");
            uint32_t packed=(type<<28)|len;epoc::set_descriptor_length_variable(packed,5);check(packed==((type<<28)|5),"variable setter preserves type and clears high length");
        }
    }
    {epoc::ptr_des8 d;d.set_descriptor_type(epoc::ptr);d.max_length=0xFFFFFFF;d.set_length(nullptr,0xFFFFFFF);check(d.get_max_length(nullptr)==0xFFFFFFF,"max length is separate field");d.set_length(nullptr,0x10000000);check(d.get_length()==0xFFFFFFF&&d.get_descriptor_type()==epoc::ptr,"oversize length cannot change type");d.max_length=7;d.set_length(nullptr,8);check(d.get_length()==0xFFFFFFF,"overcapacity setter rejected");}
    {epoc::buf_desc8 backing;backing.info=epoc::buf_const<<28;epoc::ptr_des8 d;d.set_descriptor_type(epoc::ptr_to_buf);d.max_length=0xFFFFFFF;d.data=ptr<char>(bind(&backing));d.set_length(nullptr,34*1024*1024);check(backing.get_length()==34*1024*1024&&backing.get_descriptor_type()==epoc::buf_const,"ptr_to_buf updates backing 28 bit length");d.set_length(nullptr,2);check(backing.get_length()==2,"backing length shrink clears high bits");}
    {request r(34*1024*1024);for(size_t i=0;i<r.data.size();++i)r.data[i]=char((i*37+(i>>20)*19+11)&255);r.f().disk=std::tmpfile();check(r.f().disk,"private temporary real file");check(r.write()==0,"single 34 MiB write succeeds");check(r.f().size()==r.data.size()&&r.f().writes==544,"full 34 MiB size from one IPC request with bounded host staging");auto expected=r.data;std::fill(r.data.begin(),r.data.end(),0);r.ctx.position=0;r.ctx.completions=0;check(r.read()==0,"single 34 MiB read succeeds");check(r.descriptor.get_length()==expected.size()&&r.data==expected,"all 34 MiB bytes match, not only prefix");}
    {request r;check(r.write()==0&&r.f().bytes==r.data,"ordinary write");}
    {request r;r.ctx.length=3;check(r.write()==0&&r.f().bytes.size()==3,"valid prefix write");}
    {request r;r.ctx.length=17;check(r.write()==epoc::error_argument&&r.f().writes==0,"length exceeds descriptor rejected before IO");}
    {request r;r.ctx.length=-1;check(r.write()==epoc::error_argument&&r.f().writes==0,"negative write length rejected");}
    {request r;r.ctx.length.reset();check(r.write()==epoc::error_argument,"missing write length rejected");}
    {request r;r.ctx.handle.reset();check(r.write()==epoc::error_argument,"missing write handle rejected");}
    {request r;r.client.bad_handle=true;check(r.write()==epoc::error_bad_handle,"bad file handle");}
    {request r;r.client.node.open_mode=0;check(r.write()==epoc::error_access_denied,"readonly write rejected");}
    {request r;r.ctx.msg->args.kind=ipc_arg_type::des16;check(r.write()==epoc::error_bad_descriptor,"16bit write descriptor rejected");}
    {request r;r.ctx.msg->args.args[0]=0;check(r.write()==epoc::error_bad_descriptor,"null descriptor rejected");}
    {request r;r.descriptor.info=0xF0000010;check(r.write()==epoc::error_bad_descriptor,"invalid descriptor type rejected");}
    {request r;r.descriptor.max_length=0x10000000;check(r.write()==epoc::error_bad_descriptor,"capacity exceeds 28bit ABI rejected");}
    {request r;r.descriptor.max_length=8;check(r.write()==epoc::error_bad_descriptor,"length exceeds capacity rejected");}
    {request r;r.descriptor.data=ptr<char>();check(r.write()==epoc::error_bad_descriptor,"null nonempty payload rejected");}
    {request r;r.ctx.position=-2;check(r.write()==epoc::error_argument,"negative write position rejected");}
    {request r;r.ctx.position.reset();check(r.write()==epoc::error_argument,"missing write position rejected");}
    {request r;r.position64(-2);check(r.write()==epoc::error_argument,"negative 64bit write position rejected");}
    {request r;r.position64(INT64_MAX);check(r.write()==0&&r.f().size()==16,"64bit position beyond EOF appends without allocating a gap");}
    {request r;r.f().fail_seek=true;check(r.write()==epoc::error_general&&r.f().writes==0,"seek failure blocks write");}
    {request r;r.f().fail_size=true;check(r.write()==epoc::error_general,"size failure blocks write");}
    {request r;r.f().short_write=true;check(r.write()==epoc::error_write,"short write never reports success");}
    {request r;r.f().invalid_write=true;check(r.write()==epoc::error_write,"negative native write never reports success");}
    {request r;r.ctx.position=130000;r.f().bytes={'a','b','c'};check(r.write()==0,"beyond EOF write");check(r.f().bytes.size()==19&&std::string(r.f().bytes.data(),3)=="abc"&&r.f().bytes[3]=='x',"position beyond EOF writes at EOF per RFile contract");check(r.f().writes==1,"no fabricated gap write");}
    {request r;r.ctx.position=130000;r.f().short_write=true;check(r.write()==epoc::error_write&&r.f().writes==1,"append shortwrite is still a failure");}
    {request r(0);r.ctx.position=100;check(r.write()==0&&r.f().size()==0&&r.f().writes==0,"zero write does not extend file");}
    {request r;r.f().bytes={'a','b','c'};check(r.read()==0&&r.descriptor.get_length()==3&&std::string(r.data.data(),3)=="abc","EOF clipped read");}
    {request r;r.ctx.position=100;r.f().bytes={'a'};check(r.read()==0&&r.descriptor.get_length()==0&&r.f().reads==0,"beyond EOF returns empty without unsigned underflow");}
    {request r(0);check(r.read()==0&&r.descriptor.get_length()==0,"zero capacity empty read");}
    {request r;r.ctx.length=-1;check(r.read()==epoc::error_argument&&r.f().reads==0,"negative read length rejected");}
    {request r;r.ctx.length=17;check(r.read()==epoc::error_overflow&&r.f().reads==0,"read beyond descriptor capacity rejected");}
    {request r;r.ctx.length.reset();check(r.read()==epoc::error_argument,"missing read length rejected");}
    {request r;r.ctx.msg->args.kind=ipc_arg_type::desc8;check(r.read()==epoc::error_bad_descriptor,"IPC const output descriptor rejected");}
    {request r;r.descriptor.set_descriptor_type(epoc::ptr_const);check(r.read()==epoc::error_bad_descriptor,"const output descriptor type rejected");}
    {request r;r.ctx.position=-2;check(r.read()==epoc::error_argument,"negative read position rejected");}
    {request r;r.position64(-2);check(r.read()==epoc::error_argument,"negative 64bit read position rejected");}
    {request r;r.f().fail_seek=true;check(r.read()==epoc::error_general,"read seek failure");}
    {request r;r.f().fail_size=true;check(r.read()==epoc::error_general,"read size failure");}
    {request r;r.f().bytes.resize(16);r.f().short_read=true;check(r.read()==epoc::error_general,"short read before EOF reports error");}
    {request r;r.f().bytes.resize(16);r.f().invalid_read=true;check(r.read()==epoc::error_general,"negative native read reports error without oversized copy");}
    {request r;r.f().bytes.resize(16);r.f().truncate_read=true;check(r.read()==0&&r.descriptor.get_length()==8,"concurrent truncation is genuine short EOF");}
    {request r;r.f().bytes.resize(16);mem::pages[r.ctx.msg->args.args[0]/4096].perm=prot_read;check(r.read()==epoc::error_bad_descriptor&&r.f().reads==0,"readonly descriptor header rejected before IO");}
    {request r(8192);mem::pages.erase(r.descriptor.data.ptr_address()/4096+1);check(r.write()==epoc::error_bad_descriptor&&r.f().writes==0,"unmapped second payload page rejected before write");}
    {request r(8192);mem::pages.erase(r.descriptor.data.ptr_address()/4096+1);r.f().bytes.resize(8192);check(r.read()==epoc::error_bad_descriptor&&r.f().reads==0,"unmapped second output page rejected before read");}
    {request r(8192);mem::pages[r.descriptor.data.ptr_address()/4096+1].perm=prot_read;r.f().bytes.resize(8192);check(r.read()==epoc::error_bad_descriptor&&r.f().reads==0,"readonly second output page rejected");}
    {request r(8192);std::vector<char> separate(4096,'z');mem::pages[r.descriptor.data.ptr_address()/4096+1].host_addr=separate.data();check(r.write()==0,"discontiguous host pages write succeeds");check(r.f().bytes.size()==8192&&r.f().bytes[4095]=='x'&&r.f().bytes[4096]=='z'&&r.f().bytes.back()=='z',"write follows guest virtual page order");r.ctx.completions=0;r.ctx.position=0;r.f().bytes.assign(8192,'q');check(r.read()==0&&r.data[0]=='q'&&separate[0]=='q'&&separate.back()=='q'&&r.data[4096]=='x',"read follows noncontiguous host backing without corrupting adjacent memory");}
    {request r(32);r.descriptor.data=ptr<char>(0xfffffff0u);check(r.write()==epoc::error_bad_descriptor&&r.f().writes==0,"guest payload end overflows 32bit address rejected");}
    {request r;std::array<char,4096> first{},second{};const auto old=r.ctx.msg->args.args[0];const auto base=bind(first.data(),first.size());mem::pages[base/4096+1]={second.data(),prot_read_write};std::memcpy(first.data()+4092,&r.descriptor,4);std::memcpy(second.data(),reinterpret_cast<char*>(&r.descriptor)+4,8);r.ctx.msg->args.args[0]=base+4092;check(r.write()==0,"header spanning discontiguous pages accepted");r.ctx.completions=0;r.ctx.position=0;r.f().bytes.assign(3,'j');check(r.read()==0,"cross-page output header accepted");uint32_t info;std::memcpy(&info,first.data()+4092,4);check((info&0x0fffffff)==3&&(info>>28)==epoc::ptr,"cross-page header length updated preserving type");r.ctx.msg->args.args[0]=old;}
    {request r;std::array<char,4096> first{};const auto base=bind(first.data(),first.size());std::memcpy(first.data()+4092,&r.descriptor,4);r.ctx.msg->args.args[0]=base+4092;check(r.write()==epoc::error_bad_descriptor&&r.f().writes==0,"header missing continuation page rejected");}
    {request r;r.f().bytes.resize(5);r.position64(3);check(r.write()==0&&r.f().size()==19&&r.f().bytes[0]==0&&r.f().bytes[3]=='x',"64bit position packet accepted");r.ctx.completions=0;r.position64(3);check(r.read()==0&&r.descriptor.get_length()==16,"64bit read position packet accepted");}
    {request r;r.position64(0);r.position_descriptor.info=(epoc::ptr_const<<28)|9;check(r.write()==epoc::error_argument&&r.f().writes==0,"wrong sized position packet rejected");}
    {request r;r.position64(0);mem::pages.erase(r.position_descriptor.data.ptr_address()/4096);check(r.read()==epoc::error_argument&&r.f().reads==0,"unmapped 64bit position buffer rejected");}
    std::cout<<"PASS: eka-file-io "<<checks<<" checks (34 MiB full-byte roundtrip and fault boundaries)\n";
}
