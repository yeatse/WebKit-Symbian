// Exercise the port's actual RChunk allocator without linking JavaScriptCore.
#include "SymbianSystem.h"
#include <e32base.h>
#include <e32debug.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace System = WTF::SymbianSystem;
static FILE* output;
static unsigned failures;
static unsigned checks;

static void report(const char* format, ...)
{
    char line[512];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    TUint16 units[512];
    unsigned length = 0;
    while (line[length] && length < 511) {
        units[length] = static_cast<unsigned char>(line[length]);
        ++length;
    }
    RDebug::RawPrint(TPtrC16(units, length));
    if (output) {
        fprintf(output, "%s\n", line);
        fflush(output);
    }
}

static bool expect(bool condition, const char* description)
{
    ++checks;
    if (!condition)
        ++failures;
    report("%s: %s", condition ? "PASS" : "FAIL", description);
    return condition;
}

static void returnConstant(uint32_t* code, unsigned value)
{
    code[0] = 0xe3a00000 | value; // mov r0, #value (values here fit imm8).
    code[1] = 0xe12fff1e; // bx lr
    System::flushInstructionCache(code, 2 * sizeof(uint32_t));
}

static void branch(uint32_t* from, uint32_t* to)
{
    intptr_t displacement = reinterpret_cast<intptr_t>(to) - reinterpret_cast<intptr_t>(from) - 8;
    from[0] = 0xea000000 | (static_cast<uint32_t>(displacement / 4) & 0x00ffffff);
    System::flushInstructionCache(from, sizeof(uint32_t));
}

static void codeProbe()
{
    constexpr size_t poolSize = 16 * 1024 * 1024;
    constexpr size_t pageSize = 4096;
    auto* pool = static_cast<unsigned char*>(System::reserve(poolSize, true));
    if (!expect(pool, "reserve 16 MB executable pool"))
        return;
    auto* first = reinterpret_cast<uint32_t*>(pool);
    auto* last = reinterpret_cast<uint32_t*>(pool + poolSize - pageSize);
    if (expect(System::commit(first, pageSize, true) && System::commit(last, pageSize, true), "commit executable pool endpoints")) {
        using Function = int (*)();
        auto firstFunction = reinterpret_cast<Function>(first);
        auto lastFunction = reinterpret_cast<Function>(last);
        returnConstant(first, 42);
        expect(firstFunction() == 42, "execute generated ARM code");
        returnConstant(first, 99);
        expect(firstFunction() == 99, "execute rewritten ARM code after IMB_Range");
        returnConstant(last, 73);
        branch(first, last);
        expect(firstFunction() == 73, "ARM B forward across 16 MB pool");
        returnConstant(first, 29);
        branch(last, first);
        expect(lastFunction() == 29, "ARM B backward across 16 MB pool");
        expect(!System::commit(first, pageSize), "data commit rejects executable reservation");
        expect(!System::decommit(first, pageSize), "data protection rejects executable reservation");
        expect(System::decommit(first, pageSize, true), "logically decommit executable page");
        expect(System::commit(first, pageSize, true) && !first[0] && !first[1], "recommit executable page clears old code");
        returnConstant(first, 11);
        expect(firstFunction() == 11, "execute after executable page reuse");
    }
    System::release(pool);
    void* guardedCode = System::reserve(pageSize, true, pageSize, 1);
    expect(!guardedCode, "executable guard pages rejected");
    if (guardedCode)
        System::release(guardedCode);
}

static void dataProbe()
{
    constexpr size_t pageSize = 4096;
    auto* data = static_cast<unsigned char*>(System::reserve(2 * pageSize, false, 65536, 1));
    if (!expect(data && !(reinterpret_cast<uintptr_t>(data) & 65535), "aligned data reservation with guard pages")) {
        if (data)
            System::release(data);
        return;
    }
    expect(!System::commit(data, pageSize, true), "data reservation cannot become executable");
    if (expect(System::commit(data, pageSize), "commit data page")) {
        memset(data, 0xa5, pageSize);
        expect(System::commit(data, pageSize) && data[0] == 0xa5 && data[pageSize - 1] == 0xa5, "repeat data commit preserves bytes");
        expect(!System::commit(data, 3 * pageSize) && data[0] == 0xa5, "out-of-range commit leaves existing data intact");
        expect(System::decommit(data, pageSize), "decommit data page");
        if (expect(System::commit(data, pageSize), "recommit data page")) {
            bool zero = true;
            for (size_t i = 0; i < pageSize; ++i)
                zero &= !data[i];
            expect(zero, "recommitted data page is zero-filled");
        }
    }
    System::release(data);
}

int main()
{
    output = fopen("C:/Data/WebKit/arm11-exec-probe.log", "w");
    report("BEGIN: arm11-exec-probe");
    codeProbe();
    dataProbe();
    report("%s: arm11-exec-probe %u/%u", failures ? "FAIL" : "PASS", checks - failures, checks);
    report("arm11-exec-probe exiting %d", failures ? 1 : 0);
    if (output)
        fclose(output);
    return failures ? 1 : 0;
}
