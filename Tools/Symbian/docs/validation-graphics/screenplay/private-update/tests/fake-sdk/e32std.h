// Copyright (C) 2026 The symbian-webkit authors. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
using TInt = std::int32_t;
using TUint = std::uint32_t;
using TUint8 = std::uint8_t;
using TUint32 = std::uint32_t;
static_assert(sizeof(TInt) == 4 && sizeof(TUint) == 4 && sizeof(TUint32) == 4);
constexpr TInt KErrNone=0, KErrNotSupported=-5, KErrArgument=-6, KErrAlreadyExists=-11, KErrCancel=-3;
constexpr TInt KRequestPending=-1000, KNullHandle=0;
#define _L(x) x
#define _LIT(name, value) static constexpr const char* name = value
#define __ASSERT_ALWAYS(condition, panic) do { if (!(condition)) { panic; } } while (false)
struct TVersion { TInt iMajor, iMinor, iBuild; TVersion(TInt a,TInt b,TInt c):iMajor(a),iMinor(b),iBuild(c) {} };
struct TRequestStatus {
    TInt code=74;
    int local_completions=0, server_completions=0;
    TRequestStatus& operator=(TInt c) { code=c; return *this; }
    operator TInt() const { return code; }
};
namespace User {
inline void Panic(const char*, TInt) { throw std::runtime_error("unexpected client panic"); }
inline void RequestComplete(TRequestStatus*& p, TInt code) {
    if (!p) throw std::runtime_error("null completion");
    if (p->local_completions || p->server_completions) throw std::runtime_error("double completion");
    p->code=code; ++p->local_completions; p=nullptr;
}
}
template<class T> struct TPckgBuf { T value{}; };
template<class T> struct TPckgC { const T& value; explicit TPckgC(const T& t):value(t){} };
struct TRect { TInt left, top, right, bottom; };
class TRegion {
    std::vector<TRect> rects;
public:
    TRegion()=default;
    TRegion(std::initializer_list<TRect> r):rects(r){}
    TInt Count() const { return static_cast<TInt>(rects.size()); }
    const TRect* RectangleList() const { return rects.data(); }
};
struct TPtrC8 {
    const TUint8* data=nullptr;
    TInt length=0;
    TPtrC8()=default;
    TPtrC8(const TUint8* p, TInt n):data(p),length(n){}
    void Set(const TUint8* p, TInt n) { data=p; length=n; }
};
struct TIpcArgs {
    std::array<std::uintptr_t, 4> slots{};
    TIpcArgs()=default;
    template<class T> static std::uintptr_t Value(T value) {
        if constexpr (std::is_pointer_v<T>) return reinterpret_cast<std::uintptr_t>(value);
        else return static_cast<std::uintptr_t>(value);
    }
    template<class A> explicit TIpcArgs(A a) { slots[0]=Value(a); }
    template<class A,class B,class C> TIpcArgs(A a,B b,C c) { slots[0]=Value(a); slots[1]=Value(b); slots[2]=Value(c); }
    template<class A> void Set(TInt slot,A value) { slots.at(slot)=Value(value); }
};
namespace host {
struct Event {
    TInt opcode;
    bool async;
    std::array<std::uintptr_t,4> slots;
    std::array<TUint32,4> surface{};
    std::vector<TUint8> region;
    TRequestStatus* status=nullptr;
};
struct Backend {
    TInt connect_result=0, submit_result=0;
    TInt connect_calls=0, close_calls=0;
    std::string server;
    std::array<TInt,3> version{};
    TInt slots=0;
    std::vector<Event> events;
    std::vector<TRequestStatus*> in_flight;
    void Reset() { *this=Backend(); }
    void Async(TInt opcode, const TIpcArgs& args, TRequestStatus& status);
    TInt Sync(TInt opcode, const TIpcArgs& args);
};
extern Backend backend;
}
class RHandleBase {
protected:
    TInt handle=0;
public:
    TInt Handle() const { return handle; }
    void Close() { if (handle) { ++host::backend.close_calls; handle=0; } }
};
class RSessionBase : public RHandleBase {
protected:
    TInt CreateSession(const char* name, const TVersion& v, TInt slots) {
        auto& b=host::backend;
        ++b.connect_calls; b.server=name; b.version={v.iMajor,v.iMinor,v.iBuild}; b.slots=slots;
        if (!b.connect_result) handle=1;
        return b.connect_result;
    }
    void SendReceive(TInt op, TRequestStatus& status) { host::backend.Async(op,TIpcArgs(),status); }
    void SendReceive(TInt op,const TIpcArgs& args,TRequestStatus& status) { host::backend.Async(op,args,status); }
    TInt SendReceive(TInt op,const TIpcArgs& args=TIpcArgs()) { return host::backend.Sync(op,args); }
};
