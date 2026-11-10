#include <mimalloc.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern "C" unsigned SymbianRuntimeMimallocChunkCount();
extern "C" unsigned SymbianRuntimeMimallocChunkBytes();
extern "C" unsigned SymbianRuntimeMimallocReservedBytes();
extern "C" unsigned SymbianRuntimeMimallocAddressBudget();

namespace {
void* blocks[4096];
unsigned errors;
int last_error;
unsigned failures;

void RecordError(int error, void*)
{
    ++errors;
    last_error = error;
}

void Snapshot(const char* phase, unsigned count, unsigned size)
{
    printf("ALLOCATOR phase=%s blocks=%u requested=%u chunks=%u reserved=%u committed=%u budget=%u errors=%u last_error=%d\n",
        phase, count, count * size, SymbianRuntimeMimallocChunkCount(),
        SymbianRuntimeMimallocReservedBytes(), SymbianRuntimeMimallocChunkBytes(),
        SymbianRuntimeMimallocAddressBudget(), errors, last_error);
    if (SymbianRuntimeMimallocReservedBytes() > 64u * 1024 * 1024)
        ++failures;
}

unsigned Burst(const char* phase, unsigned count, unsigned size, unsigned alignment)
{
    unsigned allocated = 0;
    for (; allocated < count; ++allocated) {
        void* memory = alignment ? mi_malloc_aligned(size, alignment) : mi_malloc(size);
        if (!memory)
            break;
        blocks[allocated] = memory;
        if (alignment && reinterpret_cast<uintptr_t>(memory) % alignment)
            ++failures;
        auto* bytes = static_cast<unsigned char*>(memory);
        for (unsigned offset = 0; offset < size; offset += 4096)
            bytes[offset] = static_cast<unsigned char>(allocated);
        bytes[size - 1] = static_cast<unsigned char>(allocated);
        if ((allocated + 1) % 16 == 0 && size >= 256 * 1024)
            Snapshot(phase, allocated + 1, size);
    }
    Snapshot(phase, allocated, size);
    for (unsigned index = 0; index < allocated; ++index) {
        auto* bytes = static_cast<unsigned char*>(blocks[index]);
        for (unsigned offset = 0; offset < size; offset += 4096) {
            if (bytes[offset] != static_cast<unsigned char>(index))
                ++failures;
        }
        if (bytes[size - 1] != static_cast<unsigned char>(index))
            ++failures;
        mi_free(blocks[index]);
        blocks[index] = nullptr;
    }
    mi_collect(true);
    Snapshot("collected", 0, size);
    return allocated;
}
}

int main()
{
    if (!freopen("C:/Data/allocator-" PROBE_MODE ".log", "w", stdout))
        return 2;
    setvbuf(stdout, nullptr, _IOLBF, 0);
    mi_register_error(RecordError, nullptr);
    printf("ALLOCATOR BEGIN mode=%s pointer_bytes=%u version=%d\n", PROBE_MODE,
        static_cast<unsigned>(sizeof(void*)), mi_version());
    if (SymbianRuntimeMimallocAddressBudget() != 64u * 1024 * 1024)
        ++failures;
    Snapshot("baseline", 0, 0);
    unsigned small = Burst("small", 4096, 64, 0);
    // This directly tests mimalloc, not WebKit's separate aligned RChunk pool.
    unsigned aligned = Burst("mi-aligned-16k", 128, 16384, 16384);
    unsigned first = Burst("large-first", 224, 256 * 1024, 0);
    unsigned second = Burst("large-reuse", 224, 256 * 1024, 0);
    if (small != 4096 || aligned != 128)
        ++failures;
    unsigned before = SymbianRuntimeMimallocReservedBytes();
    void* impossible = mi_malloc(72u * 1024 * 1024);
    if (impossible) {
        ++failures;
        mi_free(impossible);
    }
    Snapshot("over-budget-rejected", 0, 0);
    if (SymbianRuntimeMimallocReservedBytes() != before)
        ++failures;
    printf("ALLOCATOR RESULT {\"mode\":\"%s\",\"failures\":%u,\"small\":%u,\"aligned\":%u,\"first_blocks\":%u,\"reuse_blocks\":%u,\"block_bytes\":262144,\"budget\":67108864}\n",
        PROBE_MODE, failures, small, aligned, first, second);
    printf("ALLOCATOR exiting %u\n", failures ? 1u : 0u);
    fflush(stdout);
    return failures ? 1 : 0;
}
