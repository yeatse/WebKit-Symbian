// Execute real Dynarmic-generated ARM/Thumb loads and stores. The callbacks
// deliberately record a miss; the emulator remains responsible for guest faults.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "dynarmic/interface/A32/a32.h"
#include "dynarmic/interface/tlb.h"

namespace Guest = Dynarmic::A32;
using Address = Guest::VAddr;

class Callbacks final : public Guest::UserCallbacks {
public:
    uint32_t instruction = 0xe5903000; // ldr r3, [r0]
    unsigned reads = 0;
    unsigned writes = 0;
    Address address = 0;
    uint64_t value = 0;
    uint64_t ticks = 1;
    bool thumb = false;

    std::optional<uint32_t> MemoryReadCode(Address addr) override
    {
        return addr == 0x10000 ? instruction : (thumb ? 0xe7fee7fe : 0xeafffffe);
    }
    uint64_t read(Address addr) { ++reads; address = addr; return 0x12345678; }
    void write(Address addr, uint64_t data) { ++writes; address = addr; value = data; }
    uint8_t MemoryRead8(Address addr) override { return read(addr); }
    uint16_t MemoryRead16(Address addr) override { return read(addr); }
    uint32_t MemoryRead32(Address addr) override { return read(addr); }
    uint64_t MemoryRead64(Address addr) override { return read(addr); }
    void MemoryWrite8(Address addr, uint8_t data) override { write(addr, data); }
    void MemoryWrite16(Address addr, uint16_t data) override { write(addr, data); }
    void MemoryWrite32(Address addr, uint32_t data) override { write(addr, data); }
    void MemoryWrite64(Address addr, uint64_t data) override { write(addr, data); }
    void InterpreterFallback(Address, size_t) override { std::abort(); }
    void CallSVC(uint32_t) override { std::abort(); }
    void ExceptionRaised(Address, Guest::Exception) override { std::abort(); }
    void AddTicks(uint64_t count) override { ticks = count >= ticks ? 0 : ticks - count; }
    uint64_t GetTicksRemaining() override { return ticks; }
};

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    const std::string name(argv[1]);
    Callbacks callbacks;
    Dynarmic::TLB<9> tlb(12);
    alignas(4096) std::array<uint32_t, 1024> page;
    page.fill(0x87654321);
    Address address = name.find("highest") != std::string::npos ? (~Address{0} & ~Address{0xfff}) + 4 : 4;
    const bool write = name.find("write") != std::string::npos;
    const bool hit = name.find("valid-") == 0;
    const bool byte = name.find("byte") != std::string::npos;
    const bool half = name.find("half") != std::string::npos;
    callbacks.thumb = name.find("thumb") != std::string::npos;
    callbacks.instruction = write ? 0xe5803000 : 0xe5903000;
    if (byte)
        callbacks.instruction = write ? 0xe5c03000 : 0xe5d03000;
    if (half)
        callbacks.instruction = write ? 0xe1c030b0 : 0xe1d030b0;
    if (callbacks.thumb)
        callbacks.instruction = write ? 0xe7fe6003 : 0xe7fe6803; // str/ldr r3, [r0]; b .
    const uint32_t mask = byte ? 0xff : half ? 0xffff : 0xffffffff;

    auto* backing = reinterpret_cast<uint8_t*>(page.data());
    const auto full = Dynarmic::MemoryPermission::ReadWrite;
    if (hit)
        tlb.Add(address & ~Address{0xfff}, backing, full);
    else if (name.find("denied") != std::string::npos) {
        // Nonzero aliases of page 0 share TLB slot 0. The forbidden tag must
        // never be interpreted as a valid mapping of page 0.
        const auto mapped = name.find("collision") != std::string::npos ? Address{0x200000} : address & ~Address{0xfff};
        tlb.Add(mapped, backing, write ? Dynarmic::MemoryPermission::Read : Dynarmic::MemoryPermission::Write);
    } else if (name.find("flush") != std::string::npos) {
        tlb.Add(0, backing, full);
        tlb.Flush();
    } else if (name.find("dirty") != std::string::npos) {
        tlb.Add(0, backing, full);
        tlb.MakeDirty(4);
    } else if (name.find("null-base") != std::string::npos) {
        // An inconsistent externally supplied entry must also take fallback.
        auto& entry = tlb.entries[(address >> 12) & 511];
        entry.read_addr = entry.write_addr = address & ~uint64_t{0xfff};
        entry.host_base = nullptr;
    }

    Guest::UserConfig config;
    config.callbacks = &callbacks;
    config.tlb_entries = tlb.entries.data();
    config.tlb_index_mask_bits = 9;
    Guest::Jit jit(config);
    jit.Regs()[0] = address;
    jit.Regs()[3] = 0xaabbccdd;
    jit.Regs()[15] = 0x10000;
    jit.SetCpsr(callbacks.thumb ? 0x1f0 : 0x1d0);
    jit.Run();
    auto result = jit.Regs()[3];

    bool good;
    if (hit) {
        good = !callbacks.reads && !callbacks.writes;
        if (write)
            good &= page[1] == ((0x87654321 & ~mask) | (0xaabbccdd & mask));
        else
            good &= result == (0x87654321 & mask);
    } else {
        good = callbacks.address == address && callbacks.reads == !write && callbacks.writes == write;
        if (write)
            good &= callbacks.value == (0xaabbccdd & mask);
        else
            good &= result == (0x12345678 & mask);
        for (auto word : page)
            good &= word == 0x87654321;
    }
    std::printf("%s %s callbacks=%u/%u address=%016llx result=%08llx\n", good ? "PASS" : "FAIL", argv[1], callbacks.reads, callbacks.writes, static_cast<unsigned long long>(callbacks.address), static_cast<unsigned long long>(result));
    return good ? 0 : 1;
}
