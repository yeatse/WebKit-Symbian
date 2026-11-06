// Copyright (C) 2026 The symbian-webkit authors. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause
#include "ScreenPlayUpdateSession.h"
#include "ScreenPlayUpdateProtocol.h"
#include <algorithm>
#include <iostream>
#include <sstream>

host::Backend host::backend;
void host::Backend::Async(TInt opcode,const TIpcArgs& args,TRequestStatus& status) {
    events.push_back({opcode,true,args.slots,{}, {}, &status});
    status=KRequestPending;
    in_flight.push_back(&status);
}
TInt host::Backend::Sync(TInt opcode,const TIpcArgs& args) {
    Event event{opcode,false,args.slots,{}, {}, nullptr};
    if (opcode==EScreenPlayUpdateServSubmitUpdate) {
        event.surface=reinterpret_cast<const TPckgC<TSurfaceId>*>(args.slots[1])->value.words;
        if (args.slots[3]) {
            const auto& desc=*reinterpret_cast<const TPtrC8*>(args.slots[3]);
            event.region.assign(desc.data,desc.data+desc.length);
        }
    }
    events.push_back(event);
    if (opcode==EScreenPlayUpdateServCancelAllNotifications) {
        for (auto* p:in_flight) {
            if (p->local_completions || p->server_completions) throw std::runtime_error("double server completion");
            p->code=KErrCancel; ++p->server_completions;
        }
        in_flight.clear();
    }
    return opcode==EScreenPlayUpdateServSubmitUpdate ? submit_result : KErrNone;
}

static int checks=0;
static void Check(bool b,const char* message) { ++checks; if(!b) throw std::runtime_error(message); }
static const TSurfaceId surface{{17,29,41,53}};
static RScreenPlayUpdateSession Connected() {
    host::backend.Reset();
    RScreenPlayUpdateSession session;
    Check(session.Connect()==0,"connect");
    return session;
}
static void CacheAll(RScreenPlayUpdateSession& session,TRequestStatus& a,TRequestStatus& d,TRequestStatus& x,TScreenPlayUpdateTimeStamp& t,TInt count=3) {
    session.NotifyWhenAvailable(a); session.NotifyWhenDisplayed(d,t); session.NotifyWhenDisplayedXTimes(count,x);
}
static void Opcodes(std::initializer_list<TInt> wanted) {
    std::vector<TInt> actual;
    for (const auto& e:host::backend.events) actual.push_back(e.opcode);
    Check(actual==std::vector<TInt>(wanted),"IPC order");
}
static void TestConnect() {
    host::backend.Reset(); RScreenPlayUpdateSession s;
    Check(s.Connect()==0,"default connect result");
    Check(host::backend.server=="!SURFACE_UPDATE_SERVER" && host::backend.connect_calls==1,"protected server name first");
    Check(host::backend.version==std::array<TInt,3>{1,1,3},"version must be exactly 1.1.3");
    Check(host::backend.slots==6,"default slots");
    Check(s.Connect()==KErrAlreadyExists && host::backend.connect_calls==1,"AlreadyExists no second CreateSession");
    s.Close(); s.Close(); Check(host::backend.close_calls==1,"Close is idempotent");
    host::backend.Reset(); host::backend.connect_result=KErrNotSupported;
    Check(s.Connect(9)==KErrNotSupported && s.Handle()==0,"connect error is unchanged");
    Check(host::backend.slots==9 && host::backend.connect_calls==1,"caller slots/no version retry");
    host::backend.connect_result=0;
    Check(s.Connect(8)==0 && host::backend.connect_calls==2,"explicit reconnect allowed"); s.Close();
    host::backend.Reset(); host::backend.registered="SURFACE_UPDATE_SERVER";
    Check(s.Connect()==0 && host::backend.connect_calls==2,"falls back to the unprotected name");
    Check(host::backend.server=="SURFACE_UPDATE_SERVER" && host::backend.version==std::array<TInt,3>{1,1,3},"fallback name and version"); s.Close();
    host::backend.Reset(); host::backend.registered="";
    Check(s.Connect()==KErrNotFound && s.Handle()==0 && host::backend.connect_calls==2,"no server: KErrNotFound after both names");
}
static void TestCachedAndOverwrite() {
    auto s=Connected(); TRequestStatus old,latest,d,x; TScreenPlayUpdateTimeStamp t;
    s.NotifyWhenAvailable(old); CacheAll(s,latest,d,x,t);
    Check(host::backend.events.empty(),"Notify has no IPC");
    Check(old.code==74 && latest.code==74 && d.code==74 && x.code==74,"Notify does not set pending");
    s.CancelAllUpdateNotifications();
    Opcodes({7});
    Check(old.code==74 && old.local_completions==0,"last registration wins without completing overwritten status");
    Check(latest.code==KErrCancel && d.code==KErrCancel && x.code==KErrCancel,"cached canceled");
    Check(latest.local_completions==1 && d.local_completions==1 && x.local_completions==1,"cached cancel local once");
    s.CancelAllUpdateNotifications(); Opcodes({7,7});
    Check(latest.local_completions==1,"repeat cancel does not double complete"); s.Close();
}
static void TestOverwriteAll() {
    auto s=Connected(); TRequestStatus oldA,oldD,oldX,a,d,x; TScreenPlayUpdateTimeStamp oldT,t;
    CacheAll(s,oldA,oldD,oldX,oldT,2); CacheAll(s,a,d,x,t,7);
    Check(s.SubmitUpdate(KAllScreens,surface,0)==0,"last registrations Submit"); Opcodes({3,4,5,6});
    const auto& e=host::backend.events;
    Check(e[0].status==&a && e[1].status==&d && e[2].status==&x,"all notification kinds retain last status");
    Check(e[1].slots[0]==reinterpret_cast<std::uintptr_t>(&t) && e[2].slots[0]==7,"last timestamp and count retained");
    Check(oldA.code==74 && oldD.code==74 && oldX.code==74,"overwritten registrations untouched");
    s.CancelAllUpdateNotifications();
    Check(a.server_completions==1 && d.server_completions==1 && x.server_completions==1,"only latest statuses submitted"); s.Close();
}
static void TestInvalidSubmit() {
    for (int bad=0;bad<3;++bad) {
        auto s=Connected(); TRequestStatus a,d,x; TScreenPlayUpdateTimeStamp t; CacheAll(s,a,d,x,t);
        const TInt rc=s.SubmitUpdate(bad==0?-1:KAllScreens,bad==2?TSurfaceId{}:surface,bad==1?-1:0);
        Check(rc==KErrArgument && host::backend.events.empty(),"invalid submission no IPC");
        Check(a.code==KErrArgument && d.code==KErrArgument && x.code==KErrArgument,"all cached invalid statuses complete");
        Check(a.local_completions==1 && d.local_completions==1 && x.local_completions==1,"local argument completion once");
        s.CancelAllUpdateNotifications(); Check(a.local_completions==1,"invalid clears cache"); s.Close();
    }
}
static void TestValidSubmit() {
    auto s=Connected(); TRequestStatus a,d,x; TScreenPlayUpdateTimeStamp t; CacheAll(s,a,d,x,t);
    Check(s.SubmitUpdate(KAllScreens,surface,2)==0,"valid Submit"); Opcodes({3,4,5,6});
    const auto& e=host::backend.events;
    Check(e[0].async && e[0].status==&a && e[0].slots==std::array<std::uintptr_t,4>{},"Available async no args");
    Check(e[1].async && e[1].status==&d && e[1].slots[0]==reinterpret_cast<std::uintptr_t>(&t),"Displayed timestamp in slot zero");
    Check(e[2].async && e[2].status==&x && e[2].slots[0]==3,"XTimes count in slot zero");
    Check(!e[3].async && e[3].slots[0]==KAllScreens && e[3].slots[2]==2 && e[3].slots[3]==0,"Submit scalar layout/null region");
    Check(e[3].surface==surface.words,"surface descriptor exact value");
    Check(a.code==KRequestPending && d.code==KRequestPending && x.code==KRequestPending,"server owns submitted statuses");
    Check(!a.local_completions && !d.local_completions && !x.local_completions,"no local completion of submitted statuses");
    s.CancelAllUpdateNotifications();
    Check(a.server_completions==1 && d.server_completions==1 && x.server_completions==1,"submitted Cancel completes only via fake server");
    Check(!a.local_completions && !d.local_completions && !x.local_completions,"postSubmit local cache is clear");
    s.CancelAllUpdateNotifications(); Check(a.server_completions==1,"second server cancel has no completed request"); s.Close();
}
static void TestDirtyRegion() {
    auto s=Connected(); TRegion empty;
    Check(s.SubmitUpdate(2,surface,1,&empty)==0,"empty region Submit");
    Check(host::backend.events.back().slots[3]==0,"empty region omitted");
    TRegion region{{1,2,3,4},{5,6,7,8}};
    Check(s.SubmitUpdate(2,surface,1,&region)==0,"nonempty region Submit");
    const auto& e=host::backend.events.back();
    Check(e.region.size()==2*sizeof(TRect),"region length");
    Check(!std::memcmp(e.region.data(),region.RectangleList(),e.region.size()),"region bytes"); s.Close();
}
static void TestXTimesValidation() {
    for (TInt count: {0,-1}) {
        auto s=Connected(); TRequestStatus a,d,x; TScreenPlayUpdateTimeStamp t; CacheAll(s,a,d,x,t,count);
        Check(s.SubmitUpdate(KAllScreens,surface,0)==0,"bad XTimes does not suppress Submit"); Opcodes({3,4,6});
        Check(x.code==KErrArgument && x.local_completions==1 && !x.server_completions,"bad XTimes completed locally");
        Check(a.code==KRequestPending && d.code==KRequestPending,"other notifications still submitted");
        s.CancelAllUpdateNotifications(); Check(x.local_completions==1,"invalid XTimes removed from cache"); s.Close();
    }
}
static void TestSubmitErrorAndCancel() {
    auto s=Connected(); TRequestStatus a,d,x; TScreenPlayUpdateTimeStamp t; CacheAll(s,a,d,x,t);
    host::backend.submit_result=-33;
    Check(s.SubmitUpdate(KAllScreens,surface,0)==-33,"sync Submit error passed unchanged");
    Check(!a.local_completions && !d.local_completions && !x.local_completions,"Submit error cannot locally complete server-owned statuses");
    s.CancelAllUpdateNotifications();
    Check(a.server_completions==1 && d.server_completions==1 && x.server_completions==1,"cancel after Submit error drains fake server"); s.Close();
}
int main() {
    try {
        TestConnect(); TestCachedAndOverwrite(); TestOverwriteAll(); TestInvalidSubmit(); TestValidSubmit(); TestDirtyRegion(); TestXTimesValidation(); TestSubmitErrorAndCancel();
        std::cout << "{\"result\":\"pass\",\"checks\":" << checks << ",\"cases\":8}\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "host lifecycle check failed: " << e.what() << '\n'; return 1;
    }
}
