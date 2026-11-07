#pragma once
// Host adapter for Surface Manager and Window Server calls.
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
using TInt=int;using TUint=unsigned;using TInt32=int32_t;using TInt64=int64_t;
using TUint8=unsigned char;using TUint32=uint32_t;using TBool=bool;using TAny=void;
constexpr TBool ETrue=true,EFalse=false;
constexpr TInt KErrNone=0,KErrNotFound=-1,KErrGeneral=-2,KErrCancel=-3,KErrNoMemory=-4,
 KErrNotSupported=-5,KErrArgument=-6,KErrAlreadyExists=-11,KErrInUse=-14,KErrNotReady=-18,
 KErrCorrupt=-20,KErrAccessDenied=-21,KErrOverflow=-22,KErrNotVisible=-1000;
constexpr TInt KRequestPending=-1000000,KMaxTInt=0x7fffffff;
enum TLeave{ELeave};
template<class T>T Min(T a,T b){return std::min(a,b);}template<class T>T Max(T a,T b){return std::max(a,b);}template<class T>T Abs(T a){return a<0?-a:a;}
struct TPoint{TInt iX=0,iY=0;TPoint()=default;TPoint(TInt x,TInt y):iX(x),iY(y){}bool operator==(const TPoint&)const=default;};
struct TSize{TInt iWidth=0,iHeight=0;TSize()=default;TSize(TInt w,TInt h):iWidth(w),iHeight(h){}bool operator==(const TSize&)const=default;};
struct TRect{
 TPoint iTl,iBr;TRect()=default;explicit TRect(TSize s):iBr(s.iWidth,s.iHeight){}
 TRect(TPoint p,TSize s):iTl(p),iBr(p.iX+s.iWidth,p.iY+s.iHeight){}TRect(TPoint p,TPoint q):iTl(p),iBr(q){}
 TRect(TInt l,TInt t,TInt r,TInt b):iTl(l,t),iBr(r,b){}
 TInt Width()const{return iBr.iX-iTl.iX;}TInt Height()const{return iBr.iY-iTl.iY;}TSize Size()const{return TSize(Width(),Height());}
 TBool IsEmpty()const{return Width()<=0||Height()<=0;}
 void Intersection(const TRect&r){iTl=TPoint(Max(iTl.iX,r.iTl.iX),Max(iTl.iY,r.iTl.iY));iBr=TPoint(Min(iBr.iX,r.iBr.iX),Min(iBr.iY,r.iBr.iY));if(IsEmpty())*this=TRect();}
 void BoundingRect(const TRect&r){if(r.IsEmpty())return;if(IsEmpty()){*this=r;return;}iTl=TPoint(Min(iTl.iX,r.iTl.iX),Min(iTl.iY,r.iTl.iY));iBr=TPoint(Max(iBr.iX,r.iBr.iX),Max(iBr.iY,r.iBr.iY));}
 bool operator==(const TRect&)const=default;
};
struct TRegion{virtual~TRegion()=default;};
template<int N>struct TRegionFix:TRegion{TRect value;explicit TRegionFix(TRect r):value(r){}};
struct TRequestStatus{TInt value=KErrNone;TInt Int()const{return value;}operator TInt()const{return value;}TRequestStatus&operator=(TInt v){value=v;return*this;}};
struct FakeLeave:std::runtime_error{TInt code;explicit FakeLeave(TInt c):std::runtime_error("owned adapter leave"),code(c){}};
namespace fake{inline int allocations_before_failure=-1;inline bool initial_status_pending=false;void*Allocate(std::size_t);}
class CBase{public:virtual~CBase()=default;
 static void*operator new(std::size_t n){return fake::Allocate(n);}
 static void*operator new(std::size_t n,TLeave){return operator new(n);}
 static void operator delete(void*p)noexcept{std::free(p);}static void operator delete(void*p,TLeave)noexcept{std::free(p);}
};
struct CleanupStack{inline static std::vector<CBase*>entries;
 template<class T>static void PushL(T*p){entries.push_back(p);}template<class T>static void Pop(T*p){assert(!entries.empty()&&entries.back()==p);entries.pop_back();}
 static void Unwind(){while(!entries.empty()){auto*p=entries.back();entries.pop_back();delete p;}}
};
namespace fake{inline std::vector<std::string>events;inline int wait_calls=0;inline void event(const char*n){events.emplace_back(n);}}
struct User{
 static TUint32 FastCounter(){static TUint32 ticks=0;return ++ticks;}
 static void*Alloc(TInt n){return n>0?std::malloc(n):nullptr;}static void Free(void*p){std::free(p);}[[noreturn]]static void Leave(TInt e){CleanupStack::Unwind();throw FakeLeave(e);}
 static void LeaveIfError(TInt e){if(e<0)Leave(e);}static void WaitForRequest(TRequestStatus&s){++fake::wait_calls;fake::event("wait");if(s.Int()==KRequestPending)throw std::runtime_error("undrained owned status");}
};
namespace fake{inline void*Allocate(std::size_t n){if(allocations_before_failure==0)User::Leave(KErrNoMemory);if(allocations_before_failure>0)--allocations_before_failure;if(void*p=std::calloc(1,n))return p;User::Leave(KErrNoMemory);}}
class CActive:public CBase{
public:enum{EPriorityStandard=0};explicit CActive(TInt){iStatus= fake::initial_status_pending?KRequestPending:KErrNone;}virtual~CActive()=default;
 TBool IsActive()const{return active_;}void SetActive(){assert(!active_);active_=true;}
 void Cancel(){if(!active_)return;DoCancel();User::WaitForRequest(iStatus);active_=false;}
 void DispatchForTest(){if(active_&&iStatus.Int()!=KRequestPending){active_=false;RunL();}}
protected:virtual void RunL()=0;virtual void DoCancel()=0;TRequestStatus iStatus;
private:TBool active_=false;
};
struct CActiveScheduler{inline static std::vector<CActive*>items;static void Add(CActive*a){items.push_back(a);}static void RunCompleted(){for(auto*a:items)a->DispatchForTest();}};
struct TSurfaceId{TUint32 iInternal[4]{};static TSurfaceId CreateNullId(){return TSurfaceId();}
 TBool IsNull()const{return iInternal[0]==0;}bool operator==(const TSurfaceId&r)const{return std::memcmp(iInternal,r.iInternal,sizeof iInternal)==0;}};
struct TUidPixelFormat{TInt value=0;bool operator==(const TUidPixelFormat&)const=default;};
inline constexpr TUidPixelFormat EUidPixelFormatARGB_8888_PRE{1},EUidPixelFormatXRGB_8888{2};
template<class T>struct TPckgBuf{T data{};T&operator()(){return data;}const T&operator()()const{return data;}};
struct RChunk{std::shared_ptr<std::vector<TUint8>>bytes;TUint8*Base()const{return bytes?bytes->data():nullptr;}
 TInt Size()const{return bytes?static_cast<TInt>(bytes->size()):0;}TInt Handle()const{return bytes?1:0;}
 void Close(){fake::event("chunk-close");bytes.reset();}};
struct CFbsBitGc{enum TGraphicsOrientation{EGraphicsOrientationNormal};};
struct TSurfaceConfiguration{TSurfaceId id;TRect extent,viewport;
 TInt SetSurfaceId(const TSurfaceId&s){id=s;return KErrNone;}void GetSurfaceId(TSurfaceId&s)const{s=id;}
 TInt SetExtent(const TRect&r){extent=r;return KErrNone;}TInt SetViewport(const TRect&r){viewport=r;return KErrNone;}
 TInt SetOrientation(CFbsBitGc::TGraphicsOrientation){return KErrNone;}};
struct RWsSession{TInt RegisterSurface(TInt,const TSurfaceId&);void UnregisterSurface(TInt,const TSurfaceId&);TInt Finish();};
struct RWindow{TSurfaceId current;TInt GetBackgroundSurface(TSurfaceConfiguration&)const;
 TInt SetBackgroundSurface(const TSurfaceConfiguration&,TBool);TInt SetBackgroundSurface(const TSurfaceId&);void RemoveBackgroundSurface(TBool);};
struct RSurfaceManager{
 enum TCacheAttribute{ECached,ENotCached};enum TSyncOperation{ESyncBeforeNonCPURead,ESyncBeforeNonCPUWrite,ESyncAfterNonCPUWrite};
 struct TSurfaceCreationAttributes{TSize iSize;TInt iBuffers=0;TUidPixelFormat iPixelFormat;
  TInt iStride=0,iOffsetToFirstBuffer=0,iAlignment=0;TBool iContiguous=false;TCacheAttribute iCacheAttrib=ENotCached;
  TInt iOffsetBetweenBuffers=0;void*iSurfaceHints=nullptr;TInt iHintCount=0;TBool iMappable=false;};
 struct TSurfaceInfoV01{TSize iSize;TInt iBuffers=0;TUidPixelFormat iPixelFormat;TInt iStride=0;
  TBool iContiguous=false;TCacheAttribute iCacheAttrib=ENotCached;TBool iMappable=false;};
 using TSurfaceCreationAttributesBuf=TPckgBuf<TSurfaceCreationAttributes>;using TInfoBuf=TPckgBuf<TSurfaceInfoV01>;
 TInt Open();void Close();TInt CreateSurface(const TSurfaceCreationAttributesBuf&,TSurfaceId&);TInt SurfaceInfo(const TSurfaceId&,TInfoBuf&);
 TInt GetBufferOffset(const TSurfaceId&,TInt,TInt&);TInt MapSurface(const TSurfaceId&,RChunk&);
 TInt SynchronizeCache(const TSurfaceId&,TInt,TSyncOperation);TInt CloseSurface(const TSurfaceId&);
};
constexpr TInt KAllScreens=0x00ffffff;
struct RSurfaceUpdateSession{TRequestStatus*staged=nullptr;bool connected=false;
 TInt Connect(TInt=6);void Close();TInt Handle()const{return connected?1:0;}
 void NotifyWhenAvailable(TRequestStatus&);TInt SubmitUpdate(TInt,const TSurfaceId&,TInt,const TRegion* =nullptr);
 void CancelAllUpdateNotifications();};
namespace Mem{inline void FillZ(void*p,std::size_t n){std::memset(p,0,n);}inline void Copy(void*p,const void*q,std::size_t n){std::memcpy(p,q,n);}inline void Move(void*p,const void*q,std::size_t n){std::memmove(p,q,n);}}
namespace Mem{inline void Fill(void*p,std::size_t n,TInt value){std::memset(p,value,n);}}
