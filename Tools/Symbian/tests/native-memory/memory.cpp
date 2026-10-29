// Host fault injection for the actual bridge implementation; no Symbian kernel is emulated here.
#include <e32std.h>
#include <cstdio>
#include <limits>
#include "SymbianSystem.h"

extern "C" int mbedtls_hardware_poll(void*, unsigned char* output, size_t size, size_t* written)
{
    assert(size <= 1024);
    ++FakeKernel::entropyCalls;
    std::memset(output, 0xa5, size);
    *written = size;
    if (FakeKernel::entropyMode && FakeKernel::entropyCalls == 2) {
        if (FakeKernel::entropyMode == 1)
            --*written;
        else
            return -1;
    }
    return 0;
}

int main()
{
    using namespace WTF::SymbianSystem;
    constexpr size_t page = FakeKernel::pageSize;
    assert(!reserve(0, false));
    assert(!reserve(page - 1, false));
    assert(!reserve(page, true));
    assert(!reserve(page, false, 3 * page));
    assert(!reserve(std::numeric_limits<size_t>::max() - page + 1, false));
    assert(!reserve(page, false, page, std::numeric_limits<unsigned>::max()));

    auto* memory = static_cast<unsigned char*>(reserve(4 * page, false, 4 * page, 2));
    assert(memory && !(reinterpret_cast<uintptr_t>(memory) % (4 * page)));
    assert(!commit(memory - page, page));
    assert(!commit(memory + 4 * page, page));
    assert(!commit(memory + 1, page));
    assert(!commit(memory, page - 1));
    assert(commit(memory, page));
    assert(std::all_of(memory, memory + page, [](auto value) { return !value; }));
    std::memset(memory, 0x5a, page);
    unsigned calls = FakeKernel::commitCalls;
    assert(commit(memory, page) && FakeKernel::commitCalls == calls);

    FakeKernel::commitsBeforeFailure = 1;
    unsigned decommits = FakeKernel::decommitCalls;
    assert(!commit(memory, 4 * page));
    assert(FakeKernel::decommitCalls == decommits + 1);
    assert(std::all_of(memory, memory + page, [](auto value) { return value == 0x5a; }));
    FakeKernel::commitsBeforeFailure = -1;
    assert(commit(memory, 4 * page));
    assert(std::all_of(memory, memory + page, [](auto value) { return value == 0x5a; }));
    assert(std::all_of(memory + page, memory + 4 * page, [](auto value) { return !value; }));
    assert(decommit(memory, page));
    decommits = FakeKernel::decommitCalls;
    assert(decommit(memory, page) && FakeKernel::decommitCalls == decommits);
    assert(commit(memory, page));
    assert(std::all_of(memory, memory + page, [](auto value) { return !value; }));
    release(memory);
    assert(FakeKernel::chunks.empty());
    std::puts("PASS alignment, guards, overlapping commit, partial failure rollback, zero-on-recommit");

    std::vector<void*> reservations;
    for (unsigned i = 0; i < 256; ++i) {
        auto* reservation = reserve(page, false);
        assert(reservation);
        reservations.push_back(reservation);
    }
    assert(!reserve(page, false) && FakeKernel::chunks.size() == 256);
    release(reservations.back());
    reservations.back() = reserve(page, false);
    assert(reservations.back());
    for (auto* reservation : reservations)
        release(reservation);
    assert(FakeKernel::chunks.empty());
    std::puts("PASS reservation limit, failed reservation cleanup, slot reuse");

    std::vector<unsigned char> entropy(2500, 0);
    assert(randomBytes(entropy.data(), entropy.size()));
    assert(FakeKernel::entropyCalls == 3);
    assert(std::all_of(entropy.begin(), entropy.end(), [](auto value) { return value == 0xa5; }));
    for (int mode : { 1, 2 }) {
        std::fill(entropy.begin(), entropy.end(), 0xff);
        FakeKernel::entropyMode = mode;
        FakeKernel::entropyCalls = 0;
        assert(!randomBytes(entropy.data(), entropy.size()));
        assert(std::all_of(entropy.begin(), entropy.end(), [](auto value) { return !value; }));
    }
    assert(randomBytes(nullptr, 0));
    std::puts("PASS entropy chunking, short-fill and error wipe, empty request");
}
