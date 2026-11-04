/* Copyright (C) 2026 The symbian-webkit authors. SPDX-License-Identifier: BSD-2-Clause */
// Geometry-only helper. Does not request image pixels or send window events.
#include <X11/Xlib.h>
#include <X11/Xatom.h>
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
static int xerror(Display*,XErrorEvent*) { return 0; }
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
static bool render_marker(Display*d,Window w) {
    diagnostics.phase="RENDER_MARKER";
    static const std::string marker="EKA2L1 ScreenPlay render area v1";
    Atom property=XInternAtom(d,"_NET_WM_NAME",True),utf8=XInternAtom(d,"UTF8_STRING",True);
    if(property==None||utf8==None)return false;
    Atom actual=None;int format=0;unsigned long count=0,left=0;unsigned char*data=nullptr;
    int rc=XGetWindowProperty(d,w,property,0,32,False,utf8,&actual,&format,&count,&left,&data);
    bool ok=rc==Success&&actual==utf8&&format==8&&left==0&&count==marker.size()&&data&&
        std::string(reinterpret_cast<const char*>(data),count)==marker;
    if(data)XFree(data);
    if(ok)increment(diagnostics.render_marker_count);
    return ok;
}
int main(int argc,char**argv) {
    // Same eight verified ownership arguments as owned-wm-close, plus fresh run.
    if(argc!=10)return rejected("OWNED_GEOMETRY_REJECTED","ARGUMENTS");
    Display*d=nullptr;
    try {
        long pid=std::stol(argv[1]),inner=std::stol(argv[2]),root=std::stol(argv[3]),pgid=std::stol(argv[5]);
        auto rootStart=std::stoull(argv[4]),start=std::stoull(argv[6]),pidns=std::stoull(argv[7]);
        std::string run=argv[9];
        if(run.size()!=32||!std::all_of(run.begin(),run.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');})||
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
        diagnostics.phase="WINDOW_GEOMETRY";Window main=targets[0];XWindowAttributes mainInfo{};
        if(!XGetWindowAttributes(d,main,&mainInfo)||mainInfo.map_state!=IsViewable||mainInfo.width<1||mainInfo.height<1||
           mainInfo.width>4096||mainInfo.height>4096)throw 1;
        struct Leaf { Window window; int x,y,width,height,depth; };
        std::vector<Leaf>leaves;pending={main};
        diagnostics.phase="WINDOW_TREE";
        for(size_t i=0;i<pending.size();i++){
            increment(diagnostics.windows_examined);
            Window w=pending[i],ignoredRoot=None,parent=None,*children=nullptr;unsigned count=0;
            if(!XQueryTree(d,w,&ignoredRoot,&parent,&children,&count))throw 1;
            if(count>4096||pending.size()+count>4096){if(children)XFree(children);throw 1;}
            if(children&&count)pending.insert(pending.end(),children,children+count);
            if(children)XFree(children);
            if(!client_pid(d,w,pid))continue;
            bool marked_render_area=render_marker(d,w);
            if(marked_render_area){increment(diagnostics.marker_all_nodes);if(count)increment(diagnostics.marker_nonleaf_nodes);}
            // The pinned native render widget may itself own native children.
            if(w==main||!marked_render_area)continue;
            XWindowAttributes info{};int x=0,y=0;Window child=None;
            if(!XGetWindowAttributes(d,w,&info)||info.c_class!=InputOutput||info.map_state!=IsViewable||info.width<160||info.height<240||
               !XTranslateCoordinates(d,w,main,0,0,&x,&y,&child))continue;
            if(x<0||y<0||x>mainInfo.width-info.width||y>mainInfo.height-info.height)continue;
            leaves.push_back({w,x,y,info.width,info.height,info.depth});
        }
        diagnostics.phase="NATIVE_LEAF_CANDIDATES";
        if(leaves.size()!=1||!owned(pid,root,rootStart,pgid,start,pidns,argv[8])||!client_pid(d,main,pid)||!client_pid(d,leaves[0].window,pid)||!render_marker(d,leaves[0].window))throw 1;
        const auto&leaf=leaves[0];
        XCloseDisplay(d);d=nullptr;
        std::printf("{\"version\":1,\"run\":\"%s\",\"status\":\"OWNED_NATIVE_RENDER_AREA\",\"window_owner_verified\":true,\"candidate_count\":1,\"main_window\":{\"width\":%d,\"height\":%d},\"native_leaf\":{\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d,\"depth\":%d},\"render_widget_identity_verified\":true,",
            run.c_str(),mainInfo.width,mainInfo.height,leaf.x,leaf.y,leaf.width,leaf.height,leaf.depth);
        diagnostics.phase="COMPLETE";print_diagnostics();std::puts("}");return 0;
    }catch(...){if(d)XCloseDisplay(d);return rejected("OWNED_GEOMETRY_REJECTED","OWNER_OR_UNIQUE_LEAF_UNPROVEN");}
}
