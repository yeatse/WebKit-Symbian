#pragma once
#include <cstdlib>
#include <new>
using TInt = int;
constexpr int KErrNone = 0, KErrNoMemory = -4, KErrNotReady = -18;
constexpr unsigned EChangesFreeMemory = 0x20, EChangesLowMemory = 0x80;
struct TRequestStatus { int value = 0; int Int() const { return value; } };
class CActive;
namespace NativeTest {
inline bool scheduler = true;
inline int createError = 0, logonError = 0, handles = 0, logons = 0, cancels = 0, allocations = 0;
inline CActive* observer = nullptr;
}
namespace User { inline void Free(void* pointer) { if (pointer) { --NativeTest::allocations; std::free(pointer); } } }
class CActive {
public:
    enum { EPriorityStandard = 0 };
    explicit CActive(int) { }
    virtual ~CActive() = default;
    static void* operator new(std::size_t size) noexcept { auto* p = std::calloc(1, size); if (p) ++NativeTest::allocations; return p; }
    void SetActive() { active = true; }
    void Cancel() { if (active) { DoCancel(); active = false; } }
    void complete(int status) { if (!active) std::abort(); active = false; iStatus.value = status; RunL(); }
protected:
    TRequestStatus iStatus;
private:
    virtual void RunL() = 0;
    virtual void DoCancel() = 0;
    bool active = false;
};
struct CActiveScheduler {
    static void* Current() { return NativeTest::scheduler ? reinterpret_cast<void*>(1) : nullptr; }
    static void Add(CActive* observer) { NativeTest::observer = observer; }
};
class RChangeNotifier {
public:
    int Create() { if (NativeTest::createError) return NativeTest::createError; open = true; ++NativeTest::handles; return 0; }
    int Logon(TRequestStatus&) const { ++NativeTest::logons; return NativeTest::logonError; }
    int LogonCancel() const { ++NativeTest::cancels; return 0; }
    void Close() { if (open) { open = false; --NativeTest::handles; } }
private:
    bool open = false;
};
