#pragma once
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <vector>

using TInt = int;
using TUint = unsigned;
using TAny = void;
constexpr TInt KErrNone = 0;
constexpr TInt KErrCancel = -3;
constexpr TInt KErrNotFound = -1;
constexpr TInt KRequestPending = -1000;
enum TLeave { ELeave };
#define TRAPD(error, body) TInt error = KErrNone; try { body; } catch (TInt caughtError) { error = caughtError; }
namespace User {
inline unsigned frees = 0;
inline void LeaveIfError(TInt error) { if (error < 0) throw error; }
inline void Free(TAny* pointer) { ++frees; std::free(pointer); }
}
struct TRequestStatus {
    TInt value = KRequestPending;
    TInt Int() const { return value; }
};
struct TTimeIntervalMicroSeconds32 { explicit TTimeIntervalMicroSeconds32(TInt value) : value(value) { } TInt value; };
class CActive;
namespace FakeKernel {
inline std::vector<CActive*> activeObjects;
inline bool scheduler = true;
}
class CActive {
public:
    // Match the actual CBase heap boundary, including its absence of delete.
    static void* operator new(std::size_t size, TLeave) {
        void* pointer = std::calloc(1, size);
        if (!pointer) throw TInt(-4);
        return pointer;
    }
    enum { EPriorityStandard = 0 };
    explicit CActive(TInt) { }
    virtual ~CActive() { std::erase(FakeKernel::activeObjects, this); }
    bool IsActive() const { return active; }
    void SetActive() { active = true; }
    void Cancel() { if (active) { DoCancel(); active = false; } }
    TRequestStatus* status() { return &iStatus; }
    void complete(TInt value) { assert(active); active = false; iStatus.value = value; try { RunL(); } catch (TInt error) { assert(RunError(error) == KErrNone); } }
protected:
    virtual void RunL() = 0;
    virtual void DoCancel() = 0;
    virtual TInt RunError(TInt error) { return error; }
    TRequestStatus iStatus;
private:
    bool active = false;
};
class CActiveScheduler {
public:
    static void Add(CActive* active) { FakeKernel::activeObjects.push_back(active); }
    static void* Current() { return FakeKernel::scheduler ? &FakeKernel::scheduler : nullptr; }
};
class CTimer : public CActive {
public:
    explicit CTimer(TInt priority) : CActive(priority) { }
    ~CTimer() override { Cancel(); }
    void ConstructL() { }
    void After(TTimeIntervalMicroSeconds32 interval) { delay = interval.value; SetActive(); }
    TInt delay = 0;
protected:
    void DoCancel() override { }
};
namespace FakeKernel {
inline void complete(TRequestStatus* status, TInt result) {
    for (auto* active : activeObjects) {
        if (active->status() == status) { active->complete(result); return; }
    }
    assert(false && "completion after destruction");
}
inline TInt fireTimer() {
    for (auto* active : activeObjects) {
        if (auto* timer = dynamic_cast<CTimer*>(active); timer && timer->IsActive()) {
            TInt delay = timer->delay;
            timer->complete(KErrNone);
            return delay;
        }
    }
    assert(false && "no timer");
    return 0;
}
}
