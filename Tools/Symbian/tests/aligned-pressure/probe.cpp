// The pool body and chunk bridge are production sources. This executable has
// no JS VM or JIT reservation, and does not emulate allocation success.
#include <e32debug.h>
#include <e32std.h>
#include <hal.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "SymbianSystem.h"
#include "MemoryDiagnostics.h"
#include "source-hashes.h"

static FILE* output;
static unsigned checks, failures;
static void report(const char* format, ...)
{
    char line[768]; va_list args; va_start(args, format);
    vsnprintf(line, sizeof(line), format, args); va_end(args);
    TUint16 units[768]; unsigned n = 0;
    while (line[n] && n < 767) { units[n] = static_cast<unsigned char>(line[n]); ++n; }
    RDebug::RawPrint(TPtrC16(units, n));
    if (output) { fprintf(output, "%s\n", line); fflush(output); }
}
static bool expect(bool value, const char* what)
{
    ++checks; failures += !value; report("%s: %s", value ? "PASS" : "FAIL", what); return value;
}
static void memory(const char* phase)
{
    TInt bytes = -1; HAL::Get(HALData::EMemoryRAMFree, bytes);
    report("MEMORY phase=%s system-free=%d", phase, bytes);
}
constexpr size_t notFound = static_cast<size_t>(-1);
#define RELEASE_ASSERT(value) do { if (!(value)) { report("FAIL: allocator invariant %s", #value); abort(); } } while (0)
namespace SymbianSystem = WTF::SymbianSystem;
#include "AlignedPool.inc"
static AlignedPool pool;
constexpr size_t blockSize = 16384;
constexpr size_t pageSize = 4096;
constexpr unsigned blockCount = 5120; // 80 MiB live, not merely reserved.
static void* blocks[blockCount];
static void* pages[16384];

static bool zero(void* pointer, size_t bytes)
{
    auto* data = static_cast<unsigned char*>(pointer);
    for (size_t i = 0; i < bytes; ++i) if (data[i]) return false;
    return true;
}
static bool pattern(void* pointer, size_t bytes, unsigned char value)
{
    auto* data = static_cast<unsigned char*>(pointer);
    for (size_t i = 0; i < bytes; ++i) if (data[i] != value) return false;
    return true;
}
static void releaseBlocks()
{
    for (auto& block : blocks) if (block) { pool.free(block); block = nullptr; }
}
static bool retainedPressure()
{
    memory("before-80MiB");
    for (unsigned i = 0; i < blockCount; ++i) {
        blocks[i] = pool.allocate(blockSize, blockSize);
        if (!blocks[i]) {
            report("INFO allocation-failed blocks=%u bytes=%u", i, i * unsigned(blockSize));
            memory("allocation-failed"); expect(false, "80 MiB of live aligned blocks");
            releaseBlocks(); return false;
        }
        if ((reinterpret_cast<uintptr_t>(blocks[i]) % blockSize) || !zero(blocks[i], blockSize)) {
            expect(false, "new aligned block is aligned and zero"); releaseBlocks(); return false;
        }
        memset(blocks[i], 1 + i % 251, blockSize);
        if (!(i % 1024)) report("PROGRESS retained-blocks=%u", i + 1);
    }
    expect(true, "80 MiB of live aligned blocks"); memory("retained-80MiB");
    bool intact = true;
    for (unsigned i = 0; i < blockCount; ++i) intact &= pattern(blocks[i], blockSize, 1 + i % 251);
    if (!expect(intact, "all retained block bytes preserve distinct patterns")) { releaseBlocks(); return false; }
    for (unsigned i = 0; i < blockCount; i += 2) { pool.free(blocks[i]); blocks[i] = nullptr; }
    for (unsigned i = 0; i < blockCount; i += 2) {
        blocks[i] = pool.allocate(blockSize, blockSize);
        if (!blocks[i] || !zero(blocks[i], blockSize)) { expect(false, "freed holes recommit as zero"); releaseBlocks(); return false; }
    }
    intact = true;
    for (unsigned i = 1; i < blockCount; i += 2) intact &= pattern(blocks[i], blockSize, 1 + i % 251);
    expect(intact, "hole reuse preserves neighboring live allocations");
    expect(true, "freed holes recommit as zero"); releaseBlocks(); memory("after-release");
    return true;
}
static bool fragmentation()
{
    unsigned allocated = 0;
    for (; allocated < 16384; ++allocated) {
        pages[allocated] = pool.allocate(pageSize, pageSize);
        if (!pages[allocated]) break;
        memset(pages[allocated], 0x3c, pageSize);
    }
    bool ok = allocated == 16384;
    for (unsigned i = 0; i < allocated; i += 2) { pool.free(pages[i]); pages[i] = nullptr; }
    void* block = ok ? pool.allocate(blockSize, blockSize) : nullptr;
    ok = ok && block && zero(block, blockSize);
    for (unsigned i = 1; i < allocated; i += 2) ok &= pattern(pages[i], pageSize, 0x3c);
    if (block) pool.free(block);
    for (auto& page : pages) if (page) { pool.free(page); page = nullptr; }
    return expect(ok, "fragmented 4 KiB pages permit new aligned region without corrupting neighbors");
}
struct Worker { unsigned id; bool ok; };
static void* worker(void* argument)
{
    auto& state = *static_cast<Worker*>(argument); state.ok = true;
    void* held[8] = {};
    for (unsigned round = 0; round < 128 && state.ok; ++round) {
        unsigned char value = 1 + (state.id * 31 + round) % 251;
        for (auto& block : held) {
            block = pool.allocate(blockSize, blockSize);
            if (!block || !zero(block, blockSize)) { state.ok = false; break; }
            memset(block, value, blockSize);
        }
        User::After(0);
        for (auto& block : held) if (block) { state.ok &= pattern(block, blockSize, value); pool.free(block); block = nullptr; }
    }
    return nullptr;
}
static bool concurrent()
{
    Worker workers[4] = {}; pthread_t threads[4]; unsigned started = 0;
    for (; started < 4; ++started) {
        workers[started].id = started;
        if (pthread_create(&threads[started], nullptr, worker, &workers[started])) break;
    }
    bool ok = started == 4;
    for (unsigned i = 0; i < started; ++i) { ok &= !pthread_join(threads[i], nullptr); ok &= workers[i].ok; }
    return expect(ok, "four pthreads preserve alignment zeroing ownership and data");
}
int main()
{
    output = fopen("C:/Data/WebKit/aligned-pressure.log", "w"); if (!output) return 2;
    report("BEGIN: aligned-pressure version=1");
    report("SOURCE pool=%s bridge=%s", ALIGNED_POOL_SOURCE_SHA256, CHUNK_BRIDGE_SOURCE_SHA256);
    bool completed = retainedPressure() && fragmentation() && concurrent();
    expect(completed, "all pressure stages completed");
    report("RESULT {\"probe\":\"aligned-pressure\",\"passed\":%u,\"failed\":%u,\"retainedBytes\":%u}", checks-failures, failures, completed ? blockCount * unsigned(blockSize) : 0);
    report("%s: aligned-pressure %u/%u", failures ? "FAIL" : "PASS", checks-failures, checks);
    report("aligned-pressure exiting %d", failures ? 1 : 0); fclose(output); return failures ? 1 : 0;
}
