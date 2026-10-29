#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sys/mman.h>
#include <vector>

using TInt = int;
using TUint8 = unsigned char;
constexpr TInt KErrNone = 0;
constexpr TInt EOwnerProcess = 0;
constexpr TInt EPriorityLess = 0;

namespace FakeKernel {
constexpr size_t pageSize = 4096;
struct Chunk {
    void* base;
    size_t size;
    std::vector<bool> committed;
};
inline std::map<int, Chunk> chunks;
inline int nextHandle = 1;
inline int commitsBeforeFailure = -1;
inline unsigned commitCalls = 0;
inline unsigned decommitCalls = 0;
inline int entropyMode = 0;
inline unsigned entropyCalls = 0;
}

class RChunk {
public:
    TInt CreateDisconnectedLocal(TInt, TInt, TInt size, TInt)
    {
        void* base = mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (base == MAP_FAILED)
            return -4;
        m_handle = FakeKernel::nextHandle++;
        FakeKernel::chunks.emplace(m_handle, FakeKernel::Chunk { base, static_cast<size_t>(size), std::vector<bool>(size / FakeKernel::pageSize) });
        return 0;
    }
    TInt Handle() const { return m_handle; }
    void SetHandle(TInt handle) { m_handle = handle; }
    TUint8* Base() { return static_cast<TUint8*>(FakeKernel::chunks.at(m_handle).base); }
    TInt Commit(TInt offset, TInt size)
    {
        auto& chunk = FakeKernel::chunks.at(m_handle);
        assert(offset >= 0 && size > 0 && static_cast<size_t>(offset + size) <= chunk.size);
        assert(!(offset % FakeKernel::pageSize) && !(size % FakeKernel::pageSize));
        ++FakeKernel::commitCalls;
        if (!FakeKernel::commitsBeforeFailure)
            return -4;
        if (FakeKernel::commitsBeforeFailure > 0)
            --FakeKernel::commitsBeforeFailure;
        size_t page = offset / FakeKernel::pageSize;
        if (chunk.committed[page])
            return -14;
        assert(size == FakeKernel::pageSize);
        if (mprotect(Base() + offset, size, PROT_READ | PROT_WRITE))
            return -4;
        chunk.committed[page] = true;
        return 0;
    }
    TInt Decommit(TInt offset, TInt size)
    {
        auto& chunk = FakeKernel::chunks.at(m_handle);
        assert(size == FakeKernel::pageSize);
        auto result = mmap(Base() + offset, size, PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        assert(result != MAP_FAILED);
        chunk.committed[offset / FakeKernel::pageSize] = false;
        ++FakeKernel::decommitCalls;
        return 0;
    }
    void Close()
    {
        auto& chunk = FakeKernel::chunks.at(m_handle);
        assert(!munmap(chunk.base, chunk.size));
        FakeKernel::chunks.erase(m_handle);
        m_handle = 0;
    }
private:
    int m_handle = 0;
};

class TPtr8 {
public:
    TPtr8(TUint8*, TInt, size_t) { }
    TInt Length() const { return 0; }
};
class TPtr16 {
public:
    TInt Length() const { return 0; }
    uint16_t operator[](TInt) const { return 0; }
};
class HBufC {
public:
    static HBufC* New(TInt) { return nullptr; }
    TPtr16 Des() { return { }; }
};
struct TThreadStackInfo { uintptr_t iBase; uintptr_t iLimit; };
class TTimeIntervalMicroSeconds {
public:
    explicit TTimeIntervalMicroSeconds(TInt) { }
    int64_t Int64() { return 0; }
};
class RThread {
public:
    TInt Id() { return 1; }
    TInt Open(TInt, TInt) { return 0; }
    TInt Handle() { return 1; }
    void SetHandle(TInt) { }
    void SetPriority(TInt) { }
    void Close() { }
    void Suspend() { }
    void Resume() { }
    void Context(TPtr8&) { }
    TInt StackInfo(TThreadStackInfo&) { return -5; }
    TInt GetCpuTime(TTimeIntervalMicroSeconds&) { return -5; }
};
struct Mem { static void FillZ(void* address, TInt size) { memset(address, 0, size); } };
struct User {
    static void IMB_Range(void*, void*) { }
    static TInt CommandLineLength() { return 0; }
    static void CommandLine(TPtr16&) { }
    static void Free(void* p) { free(p); }
};
