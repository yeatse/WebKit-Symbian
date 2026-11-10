#include <e32std.h>
#include <pthread.h>
#include <cstdio>
#include "SymbianSystem.h"
#include "MemoryDiagnostics.h"

extern "C" int mbedtls_hardware_poll(void*, unsigned char*, size_t, size_t*) { return -1; }
namespace SymbianSystem = WTF::SymbianSystem;
constexpr size_t notFound = static_cast<size_t>(-1);
#define RELEASE_ASSERT(value) assert(value)
#include "AlignedPool.inc"

static AlignedPool pool;
AlignedPool& alignedPool() { return pool; }
static void* sdkAllocation;
static unsigned heapQueries;
static unsigned sdkFrees;

extern "C" bool mi_is_in_heap_region(const void* pointer) noexcept
{
    ++heapQueries;
    return pointer == sdkAllocation;
}

extern "C" void mi_free(void* pointer)
{
    assert(pointer == sdkAllocation);
    ++sdkFrees;
    std::free(pointer);
    sdkAllocation = nullptr;
}

#include "FastFree.inc"

int main()
{
    void* ordinary = std::malloc(128);
    assert(ordinary);
    fastFree(ordinary);
    assert(heapQueries == 1 && !sdkFrees);

    sdkAllocation = std::aligned_alloc(64, 128);
    assert(sdkAllocation);
    fastFree(sdkAllocation);
    assert(heapQueries == 2 && sdkFrees == 1 && !sdkAllocation);

    constexpr size_t blockSize = 16384;
    void* native = pool.allocate(blockSize, blockSize);
    assert(native);
    std::memset(native, 0x5a, blockSize);
    unsigned decommits = FakeKernel::decommitCalls;
    fastFree(native);
    assert(heapQueries == 2 && sdkFrees == 1);
    assert(FakeKernel::decommitCalls == decommits + blockSize / FakeKernel::pageSize);
    for (auto& [handle, chunk] : FakeKernel::chunks)
        assert(std::none_of(chunk.committed.begin(), chunk.committed.end(), [](bool value) { return value; }));
    SymbianSystem::release(native);
    assert(FakeKernel::chunks.empty());
    std::puts("PASS fastFree routes libc, SDK aligned and native GC allocations");
}
