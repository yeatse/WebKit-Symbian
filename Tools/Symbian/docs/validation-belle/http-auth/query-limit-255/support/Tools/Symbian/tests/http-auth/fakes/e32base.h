#pragma once
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
using TInt = int;
using TBool = bool;
using TAny = void;
using TText = char16_t;
enum TLeave { ELeave };
constexpr bool ETrue = true, EFalse = false;
#define TRAPD(error, expression) int error = 0; try { expression; } catch (int caught) { error = caught; }
#define TRAP_IGNORE(expression) try { expression; } catch (int) { }
inline unsigned nativeAllocations = 0;
struct User { static void Free(void* p) { if (p) { --nativeAllocations; std::free(p); } } };
struct CBase {
    virtual ~CBase() = default;
    static void* operator new(size_t n, TLeave) { auto* p = std::calloc(1, n); if (!p) throw -4; ++nativeAllocations; return p; }
    static void operator delete(void* p) { User::Free(p); }
    static void operator delete(void* p, TLeave) { User::Free(p); }
};
class TDesC {
public:
    virtual ~TDesC() = default;
    virtual const TText* Ptr() const = 0;
    virtual TInt Length() const = 0;
};
class TPtrC : public TDesC {
public:
    TPtrC(const TText* p, TInt n) : p(p), n(n) { }
    const TText* Ptr() const override { return p; }
    TInt Length() const override { return n; }
private:
    const TText* p;
    TInt n;
};
#define _L(s) TPtrC(u##s, sizeof(u##s) / sizeof(char16_t) - 1)
class TDes : public TDesC {
public:
    explicit TDes(unsigned capacity) : storage(capacity) { }
    const TText* Ptr() const override { return storage.data(); }
    TInt Length() const override { return length; }
    TInt MaxLength() const { return storage.size(); }
    void SetLength(TInt n) { assert(n >= 0 && n <= MaxLength()); length = n; }
    void Append(const TDesC& s) { assert(length + s.Length() <= MaxLength()); std::copy_n(s.Ptr(), s.Length(), storage.begin() + length); length += s.Length(); }
    void AppendNum(unsigned n) { auto text = std::to_string(n); for (char c : text) { TText v = c; Append(TPtrC(&v, 1)); } }
    void Copy(const std::u16string& s) { SetLength(0); Append(TPtrC(s.data(), s.size())); }
private:
    std::vector<TText> storage;
    TInt length = 0;
};
template<int N> struct TBuf : TDes { TBuf() : TDes(N) { } };
