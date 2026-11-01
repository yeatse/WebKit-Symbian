#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>
#include "/workspace/symbian-work/symbian-platform/cpp/symbian/runtime/abi.h"
static std::atomic<unsigned> ticks{1000};
static std::atomic<unsigned> nanoCalls{0},tickCalls{0};
static std::atomic<int> entered{0};
static int nanoPeriod=1000,tickPeriod=15625;
static thread_local bool delayed=false;
extern "C" unsigned SymbianRuntimeNanoTickCount(){++nanoCalls;if(delayed){entered.store(1);while(entered.load()!=2)std::this_thread::yield();return 1000;}return ticks.load();}
extern "C" unsigned SymbianRuntimeTickCount(){++tickCalls;return ticks.load();}
extern "C" int SymbianRuntimeNanoTickPeriodMicros(){return nanoPeriod;}
extern "C" int SymbianRuntimeTickPeriodMicros(){return tickPeriod;}
extern "C" [[noreturn]] void SymbianRuntimeExit(SymbianRuntimeExitReason reason){std::printf("native clock capability failure reason=%d\n",static_cast<int>(reason));std::fflush(stdout);std::_Exit(reason==SymbianRuntimeExitReason::kRuntimeContractFailure?6:99);}
static std::int64_t now(){return SymbianRuntimeSteadyClockNanoseconds();}
int main(int argc,char**argv){assert(argc==2);
 if(!std::strcmp(argv[1],"fraction")){
  assert(now()==1000000000LL);ticks=1001;assert(now()==1001000000LL);
  double seconds=now()/1.0e9;assert(seconds>1.0009&&seconds<1.0011);
  ticks=1999;assert(now()==1999000000LL);assert(nanoCalls>0&&tickCalls==0);
 }else if(!std::strcmp(argv[1],"wrap-and-stale")){
  ticks=0xfffffff0;auto a=now();ticks=0x10;auto b=now();assert(b-a==32000000LL);
  ticks=0x8;assert(now()==b);ticks=0x20;assert(now()==b+16000000LL);
 }else if(!std::strcmp(argv[1],"fallback")){
  nanoPeriod=-5;ticks=64;assert(now()==1000000000LL);ticks=65;assert(now()==1015625000LL);
  assert(nanoCalls==0&&tickCalls==2);
 }else if(!std::strcmp(argv[1],"concurrent-stale")){
  ticks=999;assert(now()==999000000LL);std::int64_t stale=0;
  std::thread a([&]{delayed=true;stale=now();});while(entered.load()!=1)std::this_thread::yield();
  ticks=1010;auto newer=now();entered=2;a.join();assert(stale==newer&&newer==1010000000LL);
 }else if(!std::strcmp(argv[1],"threads")){
  std::atomic<int> bad{0};std::vector<std::thread> workers;
  for(int t=0;t<8;++t)workers.emplace_back([&]{std::int64_t previous=0;for(int i=0;i<10000;++i){ticks.fetch_add(1);auto value=now();if(value<previous)++bad;previous=value;}});
  for(auto&t:workers)t.join();assert(bad==0);assert(now()==81000000000LL);
 }else if(!std::strcmp(argv[1],"invalid-periods")){
  nanoPeriod=0;tickPeriod=-5;now();return 98;
 }else if(!std::strcmp(argv[1],"half-wrap-gap-boundary")){
  ticks=1000;auto a=now();ticks=1000u+0x80000001u;assert(now()==a);
  std::puts("Documented bound: samples separated by more than half a 32-bit wrap cannot be reconstructed from the counter alone.");
 }else return 97;
 std::printf("PASS %s\n",argv[1]);
}
