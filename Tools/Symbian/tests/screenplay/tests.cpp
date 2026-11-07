#include "ScreenPlaySurface.h"
#include "adapter_state.h"
#include <functional>
#include <iostream>

namespace {
int checks=0,cases=0;
void Check(bool good,const char*message){++checks;if(!good)throw std::runtime_error(message);}
#define CHECK(x) Check((x),#x)
struct Observer final:MScreenPlaySurfaceObserver{int callbacks=0;void ScreenPlaySurfaceReady()override{++callbacks;}};
struct Harness{
 RWsSession ws;RWindow window;Observer observer;std::unique_ptr<CScreenPlaySurface> presenter;
 Harness(){fake::Reset();presenter.reset(CScreenPlaySurface::NewL(ws,window,0,observer));}
 ~Harness(){presenter.reset();}
 CScreenPlaySurface::TFrame Begin(TRect dirty=TRect(TSize(8,6)),TSize size=TSize(8,6)){
  CScreenPlaySurface::TFrame frame;CHECK(presenter->BeginFrame(size,dirty,frame)==KErrNone);return frame;
 }
 void Submit(const CScreenPlaySurface::TFrame&frame){CHECK(presenter->SubmitFrame(frame)==KErrNone);}
};
void Paint(const CScreenPlaySurface::TFrame&f,TUint32 color){
 for(TInt y=f.iDirty.iTl.iY;y<f.iDirty.iBr.iY;++y){auto*row=reinterpret_cast<TUint32*>(f.iPixels+y*f.iStride);
  for(TInt x=f.iDirty.iTl.iX;x<f.iDirty.iBr.iX;++x)row[x]=color;}
}
TUint32 Pixel(const CScreenPlaySurface::TFrame&f,TInt x=0,TInt y=0){return reinterpret_cast<TUint32*>(f.iPixels+y*f.iStride)[x];}
TUint32 SurfacePixel(const CScreenPlaySurface::TFrame&f,TInt buffer,TInt x=0,TInt y=0){auto&r=fake::resources.begin()->second;TUint32 v;std::memcpy(&v,r.bytes->data()+r.offsets[buffer]+y*f.iStride+x*4,4);return v;}
std::size_t Event(const char*name){auto it=std::find(fake::events.begin(),fake::events.end(),name);CHECK(it!=fake::events.end());return it-fake::events.begin();}
void Case(const char*name,std::function<void()>body){
 try{body();CHECK(fake::resources.empty());CHECK(fake::pending.empty());++cases;std::cout<<"PASS "<<name<<"\n";}
 catch(const std::exception&e){std::cerr<<"FAIL "<<name<<": "<<e.what()<<"\n";throw;}
}
}

int main(){try{
 Case("cached-frame-copied-to-padded-surface",[]{Harness h;auto f=h.Begin();CHECK(f.iBuffer==0);CHECK(f.iStride>8*4);CHECK(f.iDirty==TRect(TSize(8,6)));CHECK(Pixel(f)==0xffffffffu);Paint(f,0xff102030u);h.Submit(f);CHECK(Pixel(f)==0xff102030u);CHECK(fake::submit_count==1);CHECK(Event("notify")<Event("submit"));CHECK(Event("sync")<Event("submit"));
  auto&r=fake::resources.begin()->second;CHECK(f.iPixels!=r.bytes->data()+r.offsets[0]);CHECK(SurfacePixel(f,0)==0xff102030u);CHECK(SurfacePixel(f,0,7,5)==0xff102030u);CHECK((*r.bytes)[0]==0x5a);CHECK((*r.bytes)[r.offsets[0]+8*4]==0xff);});
 Case("no-damage-does-not-flip",[]{Harness h;auto f=h.Begin();h.Submit(f);CScreenPlaySurface::TFrame idle;CHECK(h.presenter->BeginFrame(TSize(8,6),TRect(),idle)==KErrAlreadyExists);CHECK(idle.iBuffer==-1);CHECK(fake::submit_count==1);});
 Case("two-slots-defer-and-reuse-damage",[]{Harness h;auto a=h.Begin();Paint(a,0xff111111);h.Submit(a);
  TRect changedA(4,2,7,5),changedB(0,0,2,1);auto b=h.Begin(changedA);CHECK(b.iBuffer==1);CHECK(b.iDirty==changedA);CHECK(Pixel(b,0,0)==0xff111111);Paint(b,0xff222222);h.Submit(b);CHECK(SurfacePixel(b,1,0,0)==0xff111111);CHECK(SurfacePixel(b,1,4,2)==0xff222222);
  CScreenPlaySurface::TFrame busy;CHECK(h.presenter->BeginFrame(TSize(8,6),changedB,busy)==KErrInUse);CHECK(busy.iBuffer==-1);CHECK(h.observer.callbacks==0);fake::Complete(a.iBuffer);CActiveScheduler::RunCompleted();CHECK(h.observer.callbacks==1);
  auto c=h.Begin(TRect());CHECK(c.iBuffer==a.iBuffer);CHECK(c.iDirty==changedB);CHECK(Pixel(c,7,5)==0xff111111);CHECK(Pixel(c,4,2)==0xff222222);CHECK(Pixel(c,0,0)==0xffffffff);Paint(c,0xff333333);h.Submit(c);
  CHECK(SurfacePixel(c,0,0,0)==0xff333333);CHECK(SurfacePixel(c,0,4,2)==0xff222222);CHECK(SurfacePixel(c,0,7,5)==0xff111111);
  CHECK(h.presenter->BeginFrame(TSize(8,6),TRect(),busy)==KErrAlreadyExists);});
 Case("abort-restores-damage-and-rejects-stale-abort",[]{Harness h;auto first=h.Begin();auto wrong=first;++wrong.iSerial;h.presenter->AbortFrame(wrong);CScreenPlaySurface::TFrame busy;CHECK(h.presenter->BeginFrame(TSize(8,6),TRect(),busy)==KErrInUse);
  h.presenter->AbortFrame(first);auto again=h.Begin(TRect());CHECK(again.iDirty==TRect(TSize(8,6)));CHECK(again.iSerial!=first.iSerial);Paint(again,0xff334455);h.Submit(again);});
 Case("explicit-alpha-flattened-over-white",[]{Harness h;auto f=h.Begin();Paint(f,0);auto*row=reinterpret_cast<TUint32*>(f.iPixels);row[1]=0x80402010u;row[2]=0xff102030;row[3]=0x00808080u;h.Submit(f);
  CHECK(Pixel(f,0)==0xffffffff);CHECK(Pixel(f,1)==0xffbf9f8f);CHECK(Pixel(f,2)==0xff102030);CHECK(Pixel(f,3)==0xffffffff);});
 Case("background-removal-repaints-and-reattaches",[]{Harness h;auto a=h.Begin();h.Submit(a);h.window.current=TSurfaceId();auto b=h.Begin(TRect());CHECK(b.iDirty==TRect(TSize(8,6)));h.Submit(b);CHECK(!h.window.current.IsNull());});
 Case("foreign-background-before-submit-preserved",[]{Harness h;h.window.current.iInternal[0]=900000;auto a=h.Begin();CHECK(h.presenter->SubmitFrame(a)==KErrAccessDenied);h.presenter.reset();CHECK(h.window.current.iInternal[0]==900000);CHECK(std::count(fake::events.begin(),fake::events.end(),"remove-bg")==0);});
 Case("foreign-background-after-submit-preserved",[]{Harness h;auto a=h.Begin();h.Submit(a);h.window.current.iInternal[0]=900001;CScreenPlaySurface::TFrame next;CHECK(h.presenter->BeginFrame(TSize(8,6),TRect(0,0,1,1),next)==KErrAccessDenied);h.presenter.reset();CHECK(h.window.current.iInternal[0]==900001);CHECK(std::count(fake::events.begin(),fake::events.end(),"remove-bg")==0);});
 Case("detach-remove-finish-cancel-drain-close",[]{Harness h;auto a=h.Begin();h.Submit(a);auto b=h.Begin(TRect(0,0,1,1));h.Submit(b);fake::events.clear();h.presenter->Detach();CHECK(fake::pending.empty());CHECK(h.window.current.IsNull());CHECK(fake::wait_calls>=2);CHECK(Event("remove-bg")<Event("finish"));CHECK(Event("finish")<Event("cancel-all"));CHECK(Event("cancel-all")<Event("unregister"));CHECK(Event("unregister")<Event("chunk-close"));CHECK(Event("chunk-close")<Event("close-surface"));CHECK(h.observer.callbacks==0);});
 Case("resize-retires-old-and-begins-full-new",[]{Harness h;auto a=h.Begin();h.Submit(a);auto b=h.Begin(TRect(),TSize(5,4));CHECK(b.iDirty==TRect(TSize(5,4)));CHECK(fake::create_count==2);CHECK(fake::close_count==1);CHECK(fake::pending.empty());h.Submit(b);});
 Case("resize-finish-error-does-not-create-new",[]{Harness h;auto a=h.Begin();h.Submit(a);fake::errors["finish"]=KErrGeneral;CScreenPlaySurface::TFrame next;CHECK(h.presenter->BeginFrame(TSize(5,4),TRect(),next)==KErrGeneral);CHECK(fake::create_count==1);CHECK(fake::close_count==1);});
 Case("availability-error-propagates-and-schedules-fallback",[]{Harness h;auto a=h.Begin();h.Submit(a);fake::Complete(a.iBuffer,KErrNotVisible);CActiveScheduler::RunCompleted();CHECK(h.presenter->LastError()==KErrNotVisible);CHECK(h.observer.callbacks==1);CScreenPlaySurface::TFrame next;CHECK(h.presenter->BeginFrame(TSize(8,6),TRect(),next)==KErrNotVisible);});
 Case("old-frame-submission-fails-closed",[]{Harness h;auto a=h.Begin();h.presenter->AbortFrame(a);auto b=h.Begin();CHECK(h.presenter->SubmitFrame(a)==KErrArgument);CHECK(fake::submit_count==0);h.presenter->AbortFrame(b);});
 for(TSize invalid:{TSize(),TSize(-1,2),TSize(2,-1),TSize(KMaxTInt,2),TSize(1,KMaxTInt),TSize(KMaxTInt,KMaxTInt),TSize(-KMaxTInt,-KMaxTInt)})
  Case("invalid-dimensions-before-create",[=]{Harness h;CScreenPlaySurface::TFrame f;CHECK(h.presenter->BeginFrame(invalid,TRect(),f)==KErrArgument);CHECK(fake::create_count==0);});
 for(const char*mode:{"size","buffers","format","mappable","short-stride","unaligned-stride","large-stride"})
  Case((std::string("info-")+mode).c_str(),[=]{Harness h;fake::info_mode=mode;CScreenPlaySurface::TFrame f;TInt e=h.presenter->BeginFrame(TSize(8,6),TRect(),f);CHECK(e==(std::string(mode)=="large-stride"?KErrCorrupt:KErrNotSupported));CHECK(fake::resources.empty());CHECK(fake::close_count==1);});
 for(const char*mode:{"negative","unaligned","overlap","outside"})
  Case((std::string("offset-")+mode).c_str(),[=]{Harness h;fake::offset_mode=mode;CScreenPlaySurface::TFrame f;CHECK(h.presenter->BeginFrame(TSize(8,6),TRect(),f)==KErrCorrupt);CHECK(fake::resources.empty());});
 Case("empty-map-rejected",[]{Harness h;fake::map_mode="empty";CScreenPlaySurface::TFrame f;CHECK(h.presenter->BeginFrame(TSize(8,6),TRect(),f)==KErrCorrupt);CHECK(fake::resources.empty());});
 for(const char*operation:{"create","info","map","offset","register"})
  Case((std::string("begin-error-")+operation).c_str(),[=]{Harness h;fake::errors[operation]=KErrNotSupported;CScreenPlaySurface::TFrame f;CHECK(h.presenter->BeginFrame(TSize(8,6),TRect(),f)==KErrNotSupported);CHECK(fake::resources.empty());CHECK(fake::submit_count==0);});
 for(const char*operation:{"sync","get-bg","set-bg","finish","submit"})
  Case((std::string("submit-error-")+operation).c_str(),[=]{Harness h;auto a=h.Begin();fake::errors[operation]=KErrNoMemory;CHECK(h.presenter->SubmitFrame(a)==KErrNoMemory);CHECK(fake::pending.empty());h.presenter.reset();CHECK(fake::resources.empty());});
 for(const char*operation:{"manager-open","updates-connect"})
  Case((std::string("new-error-")+operation).c_str(),[=]{fake::Reset();fake::errors[operation]=KErrNotSupported;RWsSession ws;RWindow win;Observer obs;bool caught=false;try{delete CScreenPlaySurface::NewL(ws,win,0,obs);}catch(const FakeLeave&e){caught=e.code==KErrNotSupported;}CHECK(caught);CHECK(CleanupStack::entries.empty());CHECK(fake::resources.empty());});
 Case("never-armed-initially-pending-status-not-waited",[]{fake::Reset();fake::initial_status_pending=true;RWsSession ws;RWindow win;Observer obs;auto*p=CScreenPlaySurface::NewL(ws,win,0,obs);p->Detach();delete p;CHECK(fake::wait_calls==0);});
 for(int successful_allocations:{1,2})
  Case("partial-new-cleanup-never-waits-unarmed-status",[=]{fake::Reset();fake::initial_status_pending=true;fake::allocations_before_failure=successful_allocations;RWsSession ws;RWindow win;Observer obs;bool caught=false;try{delete CScreenPlaySurface::NewL(ws,win,0,obs);}catch(const FakeLeave&e){caught=e.code==KErrNoMemory;}CHECK(caught);CHECK(fake::wait_calls==0);CHECK(CleanupStack::entries.empty());});
 std::cout<<"RESULT cases="<<cases<<" checks="<<checks<<"\n";
 return 0;
}catch(...){return 1;}}
