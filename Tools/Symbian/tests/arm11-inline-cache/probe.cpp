#include "config.h"
#include "CCallHelpers.h"
#include <JavaScriptCore/InitializeThreading.h>
#include <cstdio>
#include <cstdint>
using namespace JSC;

// The method under test is extracted verbatim from the production source.
// These fields record label offsets without changing MacroAssembler emission.
struct RecordedLabel {
    CCallHelpers& jit;
    unsigned offset { 0 };
    void operator=(const MacroAssembler::Label&) { offset = jit.m_assembler.buffer().codeSize(); }
};
struct ProbeGenerator {
    struct Stub { bool useDataIC { false }; } stub;
    Stub* m_stubInfo { &stub };
    RecordedLabel m_start, m_done;
    MacroAssembler::Jump m_slowPathJump;
    explicit ProbeGenerator(CCallHelpers& jit) : m_start { jit }, m_done { jit } { }
    void generateFastCommon(CCallHelpers&, size_t);
};
#include "method.inc"

int main()
{
    JSC::initialize();
    unsigned corruptions = 0, wrongPadding = 0, enlargedRegions = 0, cases = 0;
    // Cover every instruction position around multiple 2 KiB pool boundaries,
    // including different numbers of outstanding unique literals.
    for (unsigned literals : { 1u, 2u, 8u }) {
        for (unsigned padding = 0; padding < 1100; ++padding) {
            CCallHelpers jit;
            auto& assembler = jit.m_assembler;
            unsigned offsets[8];
            for (unsigned i = 0; i < literals; ++i) {
                offsets[i] = assembler.codeSize();
                assembler.ldrUniqueImmediate(ARMRegisters::r6, 0x12345678 + i);
            }
            for (unsigned i = 0; i < padding; ++i)
                assembler.nop();
            ProbeGenerator generator(jit);
            generator.generateFastCommon(jit, 64);
            assembler.buffer().flushConstantPool();
            auto* code = static_cast<uint32_t*>(assembler.buffer().data());
            uintptr_t before[8];
            for (unsigned i = 0; i < literals; ++i)
                before[i] = reinterpret_cast<uintptr_t>(ARMAssembler::readPointer(code + offsets[i] / 4));
            // StructureStubInfo::inlineCodeSize() uses done minus start, so
            // a pool emitted by the final label is ALSO in the replaced range.
            unsigned actualSize = generator.m_done.offset - generator.m_start.offset;
            if (actualSize != 64)
                ++enlargedRegions;
            for (unsigned i = 1; i < 16; ++i) {
                if (code[generator.m_start.offset / 4 + i] != 0xe1a00000) {
                    ++wrongPadding;
                    break;
                }
            }
            // Match the real replacement range, including any erroneous pool.
            for (unsigned i = 0; i < actualSize / 4; ++i)
                code[generator.m_start.offset / 4 + i] = 0xe1a00000;
            for (unsigned i = 0; i < literals; ++i) {
                auto after = reinterpret_cast<uintptr_t>(ARMAssembler::readPointer(code + offsets[i] / 4));
                if (after != before[i]) {
                    if (!corruptions)
                        std::printf("CORRUPTION literals=%u padding=%u start=%u expectedEnd=%u actualEnd=%u before=%08x after=%08x\n",
                            literals, padding, generator.m_start.offset, generator.m_start.offset + 64,
                            generator.m_done.offset, unsigned(before[i]), unsigned(after));
                    ++corruptions;
                }
            }
            ++cases;
        }
    }
    std::printf("RESULT cases=%u corruptions=%u wrongPadding=%u enlargedRegions=%u\n", cases, corruptions, wrongPadding, enlargedRegions);
    std::puts(corruptions || wrongPadding || enlargedRegions ? "FAIL: arm11-inline-cache" : "PASS: arm11-inline-cache");
    return corruptions || wrongPadding || enlargedRegions ? 1 : 0;
}
