/* Copyright (C) 2026 The symbian-webkit authors. SPDX-License-Identifier: BSD-2-Clause */
// Fixed public-color ROI observer; never captures a whole window or screen.
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/XRes.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "fixed_diagnostics.h"
struct Identity { long pid=0,ppid=0,pgid=0;unsigned long long start=0; };
static Identity process(long pid) {
    std::ifstream f("/proc/"+std::to_string(pid)+"/stat");std::string s;std::getline(f,s);
    auto end=s.rfind(')');if(end==std::string::npos)return {};
    std::istringstream stream(s.substr(end+2));std::vector<std::string> fields;
    for(std::string v;stream>>v;)fields.push_back(v);
    if(fields.size()<20)return {};
    return {pid,std::stol(fields[1]),std::stol(fields[2]),std::stoull(fields[19])};
}
static bool owned(long pid,long root,unsigned long long rootStart,long pgid,
                  unsigned long long start,unsigned long long pidns,const char* exe) {
    increment(diagnostics.proc_identity_checks);diagnostics.phase="PROC_LEAF_IDENTITY";
    auto p=process(pid);if(p.pid!=pid||p.start!=start||p.pgid!=pgid)return false;
    diagnostics.phase="PROC_ROOT_IDENTITY";auto r=process(root);if(r.start!=rootStart||r.pgid!=pgid)return false;
    diagnostics.phase="PROC_NAMESPACE";
    struct stat st{};if(stat(("/proc/"+std::to_string(pid)+"/ns/pid").c_str(),&st)||st.st_ino!=pidns)return false;
    diagnostics.phase="PROC_EXECUTABLE";
    if(std::filesystem::canonical("/proc/"+std::to_string(pid)+"/exe")!=std::filesystem::canonical(exe))return false;
    diagnostics.phase="PROC_ANCESTRY";
    for(unsigned n=0;n<64&&p.pid;n++){if(p.pid==root){increment(diagnostics.proc_identity_passes);return true;}p=process(p.ppid);if(p.pid&&p.pgid!=pgid)return false;}return false;
}
static unsigned xerrors=0;
static int xerror(Display*,XErrorEvent*) { ++xerrors; return 0; }
static bool client_pid(Display* d,Window w,long pid) {
    diagnostics.phase="QUERY_CLIENT_IDS";increment(diagnostics.query_client_ids_calls);
    XResClientIdSpec spec{w,XRES_CLIENT_ID_PID_MASK};long count=0;XResClientIdValue* values=nullptr;
    bool found=false;const auto rc=XResQueryClientIds(d,1,&spec,&count,&values);
    if(rc==Success)increment(diagnostics.query_client_ids_successes);
    if(rc==Success&&values&&count>0&&count<=8){
        increment(diagnostics.query_client_ids_values,static_cast<unsigned>(count));
        for(long i=0;i<count;i++)if(XResGetClientPid(&values[i])==pid){found=true;increment(diagnostics.query_client_ids_owner_matches);}
    }
    if(values)XResClientIdsDestroy(count,values);
    return found;
}
static bool declared_pid(Display*d,Window w,long outer,long inner) {
    diagnostics.phase="WM_PID_PROPERTY";
    Atom actual=None;int format=0;unsigned long n=0,left=0;unsigned char* data=nullptr;
    int rc=XGetWindowProperty(d,w,XInternAtom(d,"_NET_WM_PID",True),0,1,False,XA_CARDINAL,&actual,&format,&n,&left,&data);
    bool ok=rc==Success&&actual==XA_CARDINAL&&format==32&&n==1&&data&&
        (static_cast<long>(*reinterpret_cast<unsigned long*>(data))==inner||static_cast<long>(*reinterpret_cast<unsigned long*>(data))==outer);
    if(data)XFree(data);
    if(ok)increment(diagnostics.declared_pid_matches);
    return ok;
}
struct Patch { const char*name; int x,y,w,h,r,g,b,tolerance; };
static const Patch patches[]={
    {"opaque_red",24,56,32,24,255,0,0,0},
    {"opaque_green",88,56,32,24,0,255,0,0},
    {"opaque_blue",152,56,32,24,0,0,255,0},
    {"opaque_yellow",216,56,32,24,255,255,0,0},
    {"red_half_over_white",24,112,32,24,255,128,128,1},
    {"clip_white_left",84,112,12,24,255,255,255,0},
    {"clip_magenta_right",108,112,16,24,255,0,255,0},
    {"clip_outside_white",132,112,8,24,255,255,255,0},
    {"moving_final_teal",152,176,16,16,0,128,128,0},
    {"moving_vacated_white",24,176,16,16,255,255,255,0},
    {"dirty_final_cyan",24,232,32,16,0,255,255,0},
    {"resize_final_purple",104,232,48,16,128,0,128,0},
    {"resize_vacated_white",164,232,12,16,255,255,255,0},
    {"canvas2d_green",24,288,32,16,0,128,0,0}
};
struct Rect { int x,y,w,h; };
static Rect mapped(const Patch&p,int ox,int oy,int num,int den) {
    int left=p.x*num/den,top=p.y*num/den;
    return {ox+left,oy+top,((p.x+p.w)*num+den-1)/den-left,((p.y+p.h)*num+den-1)/den-top};
}
static bool fits(const Rect&r,int width,int height) {
    return r.x>=0&&r.y>=0&&r.w>0&&r.h>0&&r.x<=width-r.w&&r.y<=height-r.h;
}
static bool token_ok(const std::string&s) {
    return s.size()==32&&std::all_of(s.begin(),s.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
static int self_test() {
    // Pure geometry checks only. This path never opens an X connection.
    auto r=mapped(patches[0],10,20,1,1);
    if(r.x!=34||r.y!=76||r.w!=32||r.h!=24||!fits(r,360,640))return 2;
    r=mapped(patches[0],0,0,3,2);
    if(r.x!=36||r.y!=84||r.w!=48||r.h!=36)return 2;
    r=mapped(patches[5],0,0,1,3);
    if(r.x!=28||r.y!=37||r.w!=4||r.h!=9)return 2;
    if(fits({-1,0,1,1},360,640)||fits({0,0,361,1},360,640)||fits({360,0,1,1},360,640))return 2;
    if(!token_ok(std::string(32,'a'))||token_ok(std::string(32,'A'))||token_ok(std::string(31,'a')))return 2;
    for(const auto&p:patches)if(!fits(mapped(p,0,0,1,1),320,460))return 2;
    std::puts("{\"status\":\"PURE_ROI_GEOMETRY_CHECKS_PASS\",\"display_opened\":false,\"patches\":14}");return 0;
}
struct Geometry { int x=0,y=0,width=0,height=0; };
static Geometry geometry(Display*d,Window w) {
    diagnostics.phase="WINDOW_GEOMETRY";
    XWindowAttributes a{};Window ignored=None;int x=0,y=0;
    xerrors=0;
    if(!XGetWindowAttributes(d,w,&a)||a.map_state!=IsViewable||a.width<1||a.height<1||a.width>4096||a.height>4096||
       !XTranslateCoordinates(d,w,DefaultRootWindow(d),0,0,&x,&y,&ignored))throw 1;
    XSync(d,False);if(xerrors)throw 1;
    return {x,y,a.width,a.height};
}
int main(int argc,char**argv) {
    if(argc==2&&std::string(argv[1])=="--self-test")return self_test();
    // Same eight ownership arguments as owned-wm-close, followed by fresh run,
    // independently verified content origin (client relative), scale num/den.
    // No display connection is made for absent/invalid argument counts.
    if(argc!=14)return rejected("PUBLIC_ROIS_REJECTED","ARGUMENTS");
    Display*d=nullptr;
    try {
        long pid=std::stol(argv[1]),inner=std::stol(argv[2]),root=std::stol(argv[3]),pgid=std::stol(argv[5]);
        auto rootStart=std::stoull(argv[4]),start=std::stoull(argv[6]),pidns=std::stoull(argv[7]);
        std::string run=argv[9];int ox=std::stoi(argv[10]),oy=std::stoi(argv[11]),num=std::stoi(argv[12]),den=std::stoi(argv[13]);
        if(!token_ok(run)||ox<0||oy<0||ox>4096||oy>4096||num<1||num>4096||den<1||den>4096||den>4*num||num>4*den||
           !owned(pid,root,rootStart,pgid,start,pidns,argv[8]))throw 1;
        diagnostics.phase="X_DISPLAY_OPEN";d=XOpenDisplay(nullptr);if(!d)throw 1;XSetErrorHandler(xerror);
        diagnostics.phase="XRES_EXTENSION";int eventBase=0,errorBase=0;
        diagnostics.xres_extension_present=XResQueryExtension(d,&eventBase,&errorBase)!=0;
        if(!diagnostics.xres_extension_present){throw 1;}
        int major=0,minor=0;diagnostics.xres_version_supported=XResQueryVersion(d,&major,&minor)&&(major>1||(major==1&&minor>=2));
        if(!diagnostics.xres_version_supported){throw 1;}
        Atom del=XInternAtom(d,"WM_DELETE_WINDOW",False);std::vector<Window>targets,pending{DefaultRootWindow(d)};
        diagnostics.phase="WINDOW_TREE";
        for(size_t i=0;i<pending.size();i++){
            increment(diagnostics.windows_examined);
            if(pending.size()>4096)throw 1;
            Window w=pending[i],rootWindow=None,parent=None,*children=nullptr;unsigned count=0;
            bool tree_ok=XQueryTree(d,w,&rootWindow,&parent,&children,&count);
            if(tree_ok){
                if(count>4096||pending.size()+count>4096){if(children)XFree(children);throw 1;}
                if(children&&count)pending.insert(pending.end(),children,children+count);
            }
            if(children)XFree(children);
            if(!tree_ok||!client_pid(d,w,pid)||!declared_pid(d,w,pid,inner))continue;
            // Qt native children also expose WM_DELETE. Only visible owned
            // top clients, whose parent is not this owned client, qualify.
            diagnostics.phase="WINDOW_TREE";
            XWindowAttributes top_info{};
            if(!XGetWindowAttributes(d,w,&top_info)||top_info.c_class!=InputOutput||top_info.map_state!=IsViewable)continue;
            if(parent!=None&&client_pid(d,parent,pid))continue;
            Atom*protocols=nullptr;int n=0;
            if(XGetWMProtocols(d,w,&protocols,&n)&&protocols&&n>0&&n<=64&&std::find(protocols,protocols+n,del)!=protocols+n)targets.push_back(w);
            if(protocols)XFree(protocols);
        }
        diagnostics.phase="WM_DELETE_CANDIDATES";increment(diagnostics.wm_delete_candidate_count,static_cast<unsigned>(targets.size()));
        if(targets.size()!=1)throw 1;
        Window target=targets[0];auto startGeometry=geometry(d,target);
        std::vector<unsigned>counts,mismatches;unsigned total=0,failed=0;
        for(const auto&p:patches){
            if(!owned(pid,root,rootStart,pgid,start,pidns,argv[8])||!client_pid(d,target,pid))throw 1;
            diagnostics.phase="PUBLIC_ROI_BOUNDS";auto rect=mapped(p,ox,oy,num,den);
            if(!fits(rect,startGeometry.width,startGeometry.height))throw 1;
            Rect onRoot{startGeometry.x+rect.x,startGeometry.y+rect.y,rect.w,rect.h};
            if(!fits(onRoot,DisplayWidth(d,DefaultScreen(d)),DisplayHeight(d,DefaultScreen(d))))throw 1;
            xerrors=0;
            // Read only the fixed public rectangle from the visible display.
            // No full-window/root image is requested or serialized.
            diagnostics.phase="PUBLIC_ROI_READ";XImage*image=XGetImage(d,DefaultRootWindow(d),onRoot.x,onRoot.y,onRoot.w,onRoot.h,AllPlanes,ZPixmap);
            XSync(d,False);
            if(!image||xerrors){if(image)XDestroyImage(image);throw 1;}
            diagnostics.phase="PUBLIC_ROI_FORMAT";if(image->red_mask!=0xff0000||image->green_mask!=0xff00||image->blue_mask!=0xff||
               (image->bits_per_pixel!=24&&image->bits_per_pixel!=32)) {XDestroyImage(image);throw 1;}
            unsigned bad=0;
            for(int y=0;y<rect.h;y++)for(int x=0;x<rect.w;x++){
                unsigned long pixel=XGetPixel(image,x,y);
                if(std::abs(int((pixel>>16)&255)-p.r)>p.tolerance||std::abs(int((pixel>>8)&255)-p.g)>p.tolerance||
                   std::abs(int(pixel&255)-p.b)>p.tolerance)++bad;
            }
            XDestroyImage(image);
            unsigned count=static_cast<unsigned>(rect.w*rect.h);
            counts.push_back(count);mismatches.push_back(bad);total+=count;failed+=bad;
        }
        diagnostics.phase="STABLE_RECHECK";auto endGeometry=geometry(d,target);
        if(endGeometry.x!=startGeometry.x||endGeometry.y!=startGeometry.y||endGeometry.width!=startGeometry.width||
           endGeometry.height!=startGeometry.height||!owned(pid,root,rootStart,pgid,start,pidns,argv[8])||!client_pid(d,target,pid))throw 1;
        XCloseDisplay(d);d=nullptr;
        std::printf("{\"version\":1,\"run\":\"%s\",\"status\":\"%s\",\"window_owner_verified\":true,\"patches\":[",run.c_str(),failed?"PUBLIC_COLOR_ROIS_DIFFER":"PUBLIC_COLOR_ROIS_MATCH");
        for(size_t i=0;i<counts.size();i++)std::printf("%s{\"name\":\"%s\",\"pixel_count\":%u,\"mismatch_count\":%u}",i?",":"",patches[i].name,counts[i],mismatches[i]);
        std::printf("],\"pixel_count\":%u,\"mismatch_count\":%u,",total,failed);diagnostics.phase="COMPLETE";print_diagnostics();std::puts("}");return failed?1:0;
    }catch(...){if(d)XCloseDisplay(d);return rejected("PUBLIC_ROIS_REJECTED","OWNER_MAPPING_OR_READ_UNPROVEN");}
}
