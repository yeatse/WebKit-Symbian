// Compiles the current production AlignedPool and SymbianSystem implementation.
// Only native kernel services are replaced; this is not a phone RAM limit test.
#include <e32std.h>
#include <pthread.h>
#include <cstdio>
#include <cstring>
#include "SymbianSystem.h"
#include "MemoryDiagnostics.h"

extern "C" int mbedtls_hardware_poll(void*, unsigned char*, size_t, size_t*) { return -1; }
constexpr size_t notFound = static_cast<size_t>(-1);
#define RELEASE_ASSERT(value) assert(value)
namespace SymbianSystem = WTF::SymbianSystem;
#include "AlignedPool.inc"

int main(int argc, char** argv)
{
    bool reduced = argc == 2 && !std::strcmp(argv[1], "reduced");
    if (reduced)
        FakeKernel::maximumReservation = 8 * 1024 * 1024;
    AlignedPool pool;
    constexpr size_t blockSize = 16 * 1024;
    constexpr size_t pageSize = AlignedPool::pageSize;
    // An actual failed Commit must not consume any pool pages.
    FakeKernel::commitsBeforeFailure = 1;
    assert(!pool.allocate(blockSize, blockSize));
    assert(FakeKernel::chunks.size() == 1);
    const auto& chunk = FakeKernel::chunks.begin()->second;
    assert(std::none_of(chunk.committed.begin(), chunk.committed.end(), [](bool value) { return value; }));
    FakeKernel::commitsBeforeFailure = -1;
    std::vector<void*> blocks;
    while (void* block = pool.allocate(blockSize, blockSize)) {
        assert(!(reinterpret_cast<uintptr_t>(block) % blockSize));
        assert(std::all_of(static_cast<unsigned char*>(block), static_cast<unsigned char*>(block) + blockSize, [](unsigned char value) { return !value; }));
        std::memset(block, 0x5a, blockSize);
        blocks.push_back(block);
    }
    size_t expectedPool = reduced ? 8 * 1024 * 1024 : 64 * 1024 * 1024;
    assert(chunk.size == expectedPool);
    // Kernel chunk bases need only be page-aligned, losing at most one block.
    assert(blocks.size() >= expectedPool / blockSize - 1 && blocks.size() <= expectedPool / blockSize);
    unsigned calls = FakeKernel::commitCalls;
    assert(!pool.allocate(blockSize, blockSize));
    assert(calls == FakeKernel::commitCalls); // Failure occurred before the kernel.
    void* independent = SymbianSystem::reserve(blockSize, false);
    assert(independent && SymbianSystem::commit(independent, blockSize));
    SymbianSystem::release(independent);
    std::printf("PASS %zu-byte pool exhausted after %zu aligned blocks; independent native reservation still succeeds\n", expectedPool, blocks.size());

    // Freeing one block recovers it, without falling back to ordinary malloc.
    void* freed = blocks.back();
    blocks.pop_back();
    assert(pool.free(freed));
    void* replacement = pool.allocate(blockSize, blockSize);
    assert(replacement == freed);
    assert(std::all_of(static_cast<unsigned char*>(replacement), static_cast<unsigned char*>(replacement) + blockSize, [](unsigned char value) { return !value; }));
    blocks.push_back(replacement);
    for (void* block : blocks)
        assert(pool.free(block));

    // Fill every page, then retain alternating pages: total free space can be
    // large while no 16KiB contiguous/aligned run exists.
    std::vector<void*> pages;
    while (void* page = pool.allocate(pageSize, pageSize))
        pages.push_back(page);
    for (size_t index = 0; index < pages.size(); index += 2)
        assert(pool.free(pages[index]));
    assert(!pool.allocate(blockSize, blockSize));
    for (size_t index = 1; index < pages.size(); index += 2)
        assert(pool.free(pages[index]));
    assert(std::none_of(chunk.committed.begin(), chunk.committed.end(), [](bool value) { return value; }));
    std::printf("PASS fragmentation, failed-commit rollback, free/reuse and zero-on-recommit\n");
    SymbianSystem::release(chunk.base);
    assert(FakeKernel::chunks.empty());
}
