/* Copyright (C) 2026 The symbian-webkit authors. SPDX-License-Identifier: BSD-2-Clause */
// Prepare-only. Use actual XRes headers; do not guess a ctypes library ABI.
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
int main(int argc,char**argv) {
    // outerPID, innerPID, ownedRootPID, ownedRootStart, ownedPGID, hostStart,
    // privatePIDns inode, expected owned host executable path.
    if(argc!=9)return rejected("","ARGUMENTS",true);
    try {
        long pid=std::stol(argv[1]),inner=std::stol(argv[2]),root=std::stol(argv[3]),pgid=std::stol(argv[5]);
        auto rootStart=std::stoull(argv[4]),start=std::stoull(argv[6]),pidns=std::stoull(argv[7]);
        if(!owned(pid,root,rootStart,pgid,start,pidns,argv[8]))throw 1;
        diagnostics.phase="X_DISPLAY_OPEN";Display*d=XOpenDisplay(nullptr);if(!d)throw 1;XSetErrorHandler(xerror);
        diagnostics.phase="XRES_EXTENSION";int eventBase=0,errorBase=0;
        diagnostics.xres_extension_present=XResQueryExtension(d,&eventBase,&errorBase)!=0;
        if(!diagnostics.xres_extension_present){XCloseDisplay(d);throw 1;}
        int major=0,minor=0;diagnostics.xres_version_supported=XResQueryVersion(d,&major,&minor)&&(major>1||(major==1&&minor>=2));
        if(!diagnostics.xres_version_supported){XCloseDisplay(d);throw 1;}
        Atom del=XInternAtom(d,"WM_DELETE_WINDOW",False);std::vector<Window> targets;
        // Include WM-reparented client windows, but never inspect titles.
        std::vector<Window> pending{DefaultRootWindow(d)};
        diagnostics.phase="WINDOW_TREE";
        for(size_t i=0;i<pending.size();i++){
            increment(diagnostics.windows_examined);
            if(pending.size()>4096){XCloseDisplay(d);throw 1;}
            Window w=pending[i],rootWindow=None,parent=None,*children=nullptr;unsigned count=0;
            bool tree_ok=XQueryTree(d,w,&rootWindow,&parent,&children,&count);
            if(tree_ok){
                if(count>4096||pending.size()+count>4096){if(children)XFree(children);XCloseDisplay(d);throw 1;}
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
            Atom*protocols=nullptr;int n=0;if(XGetWMProtocols(d,w,&protocols,&n)&&protocols&&n>0&&n<=64&&std::find(protocols,protocols+n,del)!=protocols+n)targets.push_back(w);
            if(protocols)XFree(protocols);
        }
        diagnostics.phase="WM_DELETE_CANDIDATES";increment(diagnostics.wm_delete_candidate_count,static_cast<unsigned>(targets.size()));
        if(targets.size()!=1||!owned(pid,root,rootStart,pgid,start,pidns,argv[8])||!client_pid(d,targets[0],pid)){XCloseDisplay(d);throw 1;}
        diagnostics.phase="WM_DELETE_SEND";XEvent e{};e.xclient.type=ClientMessage;e.xclient.window=targets[0];e.xclient.message_type=XInternAtom(d,"WM_PROTOCOLS",False);e.xclient.format=32;e.xclient.data.l[0]=del;e.xclient.data.l[1]=CurrentTime;
        if(!XSendEvent(d,targets[0],False,NoEventMask,&e)){XCloseDisplay(d);throw 1;}XFlush(d);XCloseDisplay(d);
        std::printf("{\"wm_close_sent\":true,\"window_owner_verified\":true,");diagnostics.phase="COMPLETE";print_diagnostics();std::puts("}");return 0;
    }catch(...){return rejected("","OWNER_OR_WINDOW_UNPROVEN",true);}
}
