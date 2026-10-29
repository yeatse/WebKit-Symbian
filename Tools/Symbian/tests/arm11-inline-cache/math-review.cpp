#include "config.h"
#include "JITMathIC.h"
#include <JavaScriptCore/InitializeThreading.h>
#include <cstring>
#include <cstdio>
using namespace JSC;
static unsigned cases, repatched, corruptions;
static unsigned offset(const MacroAssembler::Label& label)
{
    static_assert(sizeof(label) == sizeof(uint32_t));
    uint32_t value; std::memcpy(&value, &label, sizeof(value)); return value;
}
template<typename Generator> static void binary(const char* name)
{
    for (unsigned profileKind = 0; profileKind < 3; ++profileKind) {
        for (unsigned literals : { 1u, 2u, 8u }) {
            for (unsigned padding = 0; padding < 1100; ++padding) {
                CCallHelpers jit;
                auto& a = jit.m_assembler;
                unsigned offsets[8];
                for (unsigned i = 0; i < literals; ++i) {
                    offsets[i] = a.codeSize();
                    a.ldrUniqueImmediate(ARMRegisters::r6, 0x12345678 + i);
                }
                for (unsigned i = 0; i < padding; ++i) a.nop();
                BinaryArithProfile profile;
                if (profileKind == 1) { profile.lhsSawInt32(); profile.rhsSawInt32(); }
                if (profileKind == 2) { profile.lhsSawNumber(); profile.rhsSawNumber(); }
                JITBinaryMathIC<Generator> ic(&profile);
                ic.m_generator = Generator(SnippetOperand(), SnippetOperand(),
                    JSValueRegs(ARMRegisters::r4, ARMRegisters::r5),
                    JSValueRegs(ARMRegisters::r0, ARMRegisters::r1),
                    JSValueRegs(ARMRegisters::r2, ARMRegisters::r3),
                    ARMRegisters::d0, ARMRegisters::d1, ARMRegisters::r8);
                MathICGenerationState state;
                if (!ic.generateInline(jit, state, false)) continue;
                a.buffer().flushConstantPool();
                ++cases;
                if (!state.shouldSlowPathRepatch) continue;
                ++repatched;
                auto* code=static_cast<uint32_t*>(a.buffer().data());
                uintptr_t before[8];
                for (unsigned i=0;i<literals;++i) before[i]=reinterpret_cast<uintptr_t>(ARMAssembler::readPointer(code+offsets[i]/4));
                CCallHelpers replacement;
                replacement.jumpThunk(CodeLocationLabel<JITStubRoutinePtrTag>(reinterpret_cast<void*>(0x10000000)));
                replacement.label();
                replacement.m_assembler.buffer().flushConstantPool();
                unsigned patchSize=replacement.m_assembler.codeSize();
                unsigned start=offset(state.fastPathStart), end=offset(state.fastPathEnd);
                if (patchSize != 12 || end-start < patchSize) { std::puts("FAIL wrong replacement size"); std::exit(2); }
                for (unsigned i=0;i<patchSize/4;++i) code[start/4+i]=0xe1a00000;
                for (unsigned i=0;i<literals;++i) {
                    auto after=reinterpret_cast<uintptr_t>(ARMAssembler::readPointer(code+offsets[i]/4));
                    if (after!=before[i]) {
                        if(!corruptions) std::printf("CORRUPTION %s profile=%u padding=%u literals=%u start=%u end=%u patchSize=%u\n",name,profileKind,padding,literals,start,end,patchSize);
                        ++corruptions;
                    }
                }
            }
        }
    }
}
int main() { JSC::initialize(); binary<JITAddGenerator>("add"); binary<JITSubGenerator>("sub"); binary<JITMulGenerator>("mul"); std::printf("RESULT cases=%u repatched=%u corruptions=%u\n",cases,repatched,corruptions); return !!corruptions; }
