#include <e32std.h>
#include <pthread.h>
#include <cstdio>
#include <thread>
#include <atomic>
#include <limits>
#include "SymbianSystem.h"
#include "MemoryDiagnostics.h"

extern "C" int mbedtls_hardware_poll(void*, unsigned char*, size_t, size_t*) { return -1; }
namespace SymbianSystem = WTF::SymbianSystem;
constexpr size_t notFound = static_cast<size_t>(-1);
#define RELEASE_ASSERT(value) assert(value)
static bool failMetadata;
static std::atomic<unsigned> liveMetadata;
void* allocateMetadata(size_t count, size_t bytes) {
    if (failMetadata) return nullptr;
    void* result = std::calloc(count, bytes);
    if (result) ++liveMetadata;
    return result;
}
void freeMetadata(void* pointer) { assert(liveMetadata); --liveMetadata; std::free(pointer); }
#define calloc allocateMetadata
#define free freeMetadata
#include "AlignedPool.inc"
#undef calloc
#undef free

int main() {
    constexpr size_t blockSize = 16384;
    constexpr size_t page = 4096;
    // Physical native limits are mocked; constrain a whole region plus its
    // alignment slack to 8MiB so tests cross several reservations quickly.
    FakeKernel::maximumReservation = 8 * 1024 * 1024 + blockSize;
    AlignedPool pool;
    FakeKernel::reservationsBeforeFailure = 0;
    assert(!pool.allocate(blockSize, blockSize));
    assert(FakeKernel::chunks.empty() && !liveMetadata);
    unsigned calls = FakeKernel::reservationCalls;
    assert(!pool.allocate(blockSize, blockSize));
    assert(FakeKernel::reservationCalls == calls + 1); // No repeated 64/32/16MiB attempts.
    FakeKernel::reservationsBeforeFailure = -1;
    FakeKernel::commitsBeforeFailure = 1;
    assert(!pool.allocate(blockSize, blockSize));
    assert(FakeKernel::chunks.empty() && !liveMetadata);
    FakeKernel::commitsBeforeFailure = -1;
    std::vector<void*> blocks;
    for (unsigned i = 0; i < 512; ++i) {
        void* block = pool.allocate(blockSize, blockSize);
        assert(block && !(reinterpret_cast<uintptr_t>(block) % blockSize));
        std::memset(block, 0x5a, blockSize);
        blocks.push_back(block);
    }
    void* firstBase = blocks.front();
    assert(FakeKernel::chunks.size() == 1);
    calls = FakeKernel::reservationCalls;
    failMetadata = true;
    assert(!pool.allocate(blockSize, blockSize));
    assert(FakeKernel::reservationCalls == calls && !liveMetadata);
    failMetadata = false;
    FakeKernel::reservationsBeforeFailure = 0;
    assert(!pool.allocate(blockSize, blockSize));
    assert(FakeKernel::chunks.size() == 1 && !liveMetadata);
    FakeKernel::reservationsBeforeFailure = -1;
    FakeKernel::commitsBeforeFailure = 1;
    assert(!pool.allocate(blockSize, blockSize));
    assert(FakeKernel::chunks.size() == 1 && !liveMetadata);
    FakeKernel::commitsBeforeFailure = -1;
    for (unsigned i = 0; i < 512 + 10; ++i) {
        void* block = pool.allocate(blockSize, blockSize);
        assert(block && !(reinterpret_cast<uintptr_t>(block) % blockSize));
        std::memset(block, 0xa5, blockSize);
        blocks.push_back(block);
    }
    assert(FakeKernel::chunks.size() == 3 && liveMetadata == 2);
    assert(std::all_of(static_cast<unsigned char*>(blocks.front()), static_cast<unsigned char*>(blocks.front()) + blockSize, [](auto c){ return c == 0x5a; }));
    // Delete a middle region while the tail still owns live blocks.
    for (unsigned i = 512; i < 1024; ++i)
        assert(pool.freeMetadata(blocks[i]));
    assert(FakeKernel::chunks.size() == 2 && liveMetadata == 1);
    for (unsigned i = 1024; i < blocks.size(); ++i)
        assert(pool.freeMetadata(blocks[i]));
    assert(FakeKernel::chunks.size() == 1 && !liveMetadata);
    // Commit failure in an existing free run must not trigger growth.
    assert(pool.freeMetadata(blocks[1]));
    calls = FakeKernel::reservationCalls;
    FakeKernel::commitsBeforeFailure = 1;
    assert(!pool.allocate(blockSize, blockSize));
    assert(FakeKernel::reservationCalls == calls);
    FakeKernel::commitsBeforeFailure = -1;
    auto* reused = static_cast<unsigned char*>(pool.allocate(blockSize, blockSize));
    assert(reused == blocks[1]);
    assert(std::all_of(reused, reused + blockSize, [](auto c){ return !c; }));
    for (unsigned i = 0; i < 512; ++i)
        assert(pool.freeMetadata(blocks[i]));
    std::puts("PASS multi-region growth, middle/tail reclaim, metadata/reserve/commit failure rollback, zero-on-reuse");

    assert(!pool.allocate(page, std::numeric_limits<size_t>::max()));
    assert(!pool.allocate(3 * page, page));
    assert(!pool.allocate(page, 64 * 1024 * 1024 + page));
    int foreign;
    assert(!pool.freeMetadata(&foreign));
    std::vector<void*> pages;
    for (unsigned i = 0; i < 2048; ++i) {
        void* p = pool.allocate(page, page);
        assert(p);
        pages.push_back(p);
    }
    for (unsigned i = 0; i < 2048; i += 2)
        assert(pool.freeMetadata(pages[i]));
    void* fragmented = pool.allocate(blockSize, blockSize);
    assert(fragmented && FakeKernel::chunks.size() == 2);
    assert(pool.freeMetadata(fragmented));
    assert(FakeKernel::chunks.size() == 1);
    for (unsigned i = 1; i < 2048; i += 2)
        assert(pool.freeMetadata(pages[i]));
    std::puts("PASS overflow/alignment/foreign-owner rejection and fragmentation-driven growth");

    std::atomic<unsigned> ready { 0 };
    std::vector<std::thread> threads;
    for (unsigned t = 0; t < 4; ++t) {
        threads.emplace_back([&] {
            std::vector<void*> owned;
            for (unsigned i = 0; i < 300; ++i) {
                auto* p = static_cast<unsigned char*>(pool.allocate(blockSize, blockSize));
                assert(p);
                std::memset(p, 0x3c, blockSize);
                owned.push_back(p);
            }
            ++ready;
            while (ready < 4) std::this_thread::yield();
            for (auto* p : owned) {
                assert(std::all_of(static_cast<unsigned char*>(p), static_cast<unsigned char*>(p) + blockSize, [](auto c){ return c == 0x3c; }));
                assert(pool.freeMetadata(p));
            }
        });
    }
    for (auto& thread : threads) thread.join();
    assert(FakeKernel::chunks.size() == 1 && !liveMetadata);
    blocks.clear();
    for (unsigned i = 0; i < 4160; ++i) {
        auto* block = static_cast<unsigned char*>(pool.allocate(blockSize, blockSize));
        assert(block);
        std::memset(block, i % 256, blockSize);
        blocks.push_back(block);
    }
    assert(FakeKernel::chunks.size() == 9 && liveMetadata == 8);
    for (unsigned i = 0; i < blocks.size(); ++i)
        assert(std::all_of(static_cast<unsigned char*>(blocks[i]), static_cast<unsigned char*>(blocks[i]) + blockSize, [i](auto value) { return value == i % 256; }));
    for (auto it = blocks.rbegin(); it != blocks.rend(); ++it)
        assert(pool.freeMetadata(*it));
    assert(FakeKernel::chunks.size() == 1 && !liveMetadata);
    std::puts("PASS 65MiB live allocations across nine 8MiB regions after native-size downgrade, followed by reclaim");
    SymbianSystem::release(firstBase);
    assert(FakeKernel::chunks.empty());
    std::puts("PASS concurrent region ownership, patterns and final extra-region reclaim");
}
