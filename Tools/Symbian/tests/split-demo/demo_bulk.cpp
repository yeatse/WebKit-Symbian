// 9000 small functions, so the image is big enough to cut into several
// DLLs. Each calls both ends of the image directly and the others through a
// table in writable data (patched by the EXE at start) and a constant table
// (an import between DLLs).
#include "demo.h"

const int kCount = 9000;
// Writable, with external linkage, so it stays in .data.
extern DemoFn g_demoTable[kCount];
extern const DemoFn g_demoConstTable[kCount];
#define g_table g_demoTable
#define g_ctable g_demoConstTable

namespace {

#define DEMO_F(n) \
    __attribute__((noinline)) int fn##n(int d) \
    { \
        int v = static_cast<int>((n##u * 2654435761u) >> 7); \
        for (int i = 0; i < 3; ++i) \
            v = v * 33 + ((n) ^ i); \
        switch ((v >> 3) & 7) { \
        case 0: v += (n) * 3; break; \
        case 1: v ^= (n) << 5; break; \
        case 2: v -= (n) * 7 + d; break; \
        case 3: v = (v << 3) | ((v >> 29) & 7); break; \
        case 4: v += d * (n); break; \
        case 5: v ^= 0x5bd1e995 + (n); break; \
        case 6: v = v * 9 + (n); break; \
        default: v -= d; break; \
        } \
        v = demo_first_mix(v, d); \
        if (d > 0) \
            v ^= ((d & 1) ? g_table : g_ctable)[v % kCount](d - 1); \
        return demo_last_mix(v, n); \
    }
#define DEMO_F10(a) DEMO_F(a##0) DEMO_F(a##1) DEMO_F(a##2) DEMO_F(a##3) DEMO_F(a##4) \
    DEMO_F(a##5) DEMO_F(a##6) DEMO_F(a##7) DEMO_F(a##8) DEMO_F(a##9)
#define DEMO_F100(b) DEMO_F10(b##0) DEMO_F10(b##1) DEMO_F10(b##2) DEMO_F10(b##3) DEMO_F10(b##4) \
    DEMO_F10(b##5) DEMO_F10(b##6) DEMO_F10(b##7) DEMO_F10(b##8) DEMO_F10(b##9)
#define DEMO_F1000(c) DEMO_F100(c##0) DEMO_F100(c##1) DEMO_F100(c##2) DEMO_F100(c##3) DEMO_F100(c##4) \
    DEMO_F100(c##5) DEMO_F100(c##6) DEMO_F100(c##7) DEMO_F100(c##8) DEMO_F100(c##9)
DEMO_F1000(1) DEMO_F1000(2) DEMO_F1000(3) DEMO_F1000(4) DEMO_F1000(5)
DEMO_F1000(6) DEMO_F1000(7) DEMO_F1000(8) DEMO_F1000(9)

#define DEMO_T10(a) fn##a##0, fn##a##1, fn##a##2, fn##a##3, fn##a##4, \
    fn##a##5, fn##a##6, fn##a##7, fn##a##8, fn##a##9,
#define DEMO_T100(b) DEMO_T10(b##0) DEMO_T10(b##1) DEMO_T10(b##2) DEMO_T10(b##3) DEMO_T10(b##4) \
    DEMO_T10(b##5) DEMO_T10(b##6) DEMO_T10(b##7) DEMO_T10(b##8) DEMO_T10(b##9)
#define DEMO_T1000(c) DEMO_T100(c##0) DEMO_T100(c##1) DEMO_T100(c##2) DEMO_T100(c##3) DEMO_T100(c##4) \
    DEMO_T100(c##5) DEMO_T100(c##6) DEMO_T100(c##7) DEMO_T100(c##8) DEMO_T100(c##9)
}

DemoFn g_table[kCount] = { DEMO_T1000(1) DEMO_T1000(2) DEMO_T1000(3) DEMO_T1000(4) DEMO_T1000(5)
    DEMO_T1000(6) DEMO_T1000(7) DEMO_T1000(8) DEMO_T1000(9) };
const DemoFn g_ctable[kCount] = { DEMO_T1000(9) DEMO_T1000(8) DEMO_T1000(7) DEMO_T1000(6) DEMO_T1000(5)
    DEMO_T1000(4) DEMO_T1000(3) DEMO_T1000(2) DEMO_T1000(1) };

namespace {

// A static constructor: the EXE runs it from .init_array.
int g_initValue;
struct Init {
    Init() { g_initValue = fn1234(4); }
} g_init;

}

int demo_bulk_chain(int start, int depth)
{
    return g_table[start % kCount](depth);
}

int demo_bulk_const_chain(int start, int depth)
{
    return g_ctable[start % kCount](depth);
}

int demo_bulk_virtual(int seed)
{
    int total = 0;
    for (int kind = 0; kind < 2; ++kind) {
        DemoShape* shape = demo_last_make_shape(kind);
        total += shape->Area(seed) * 10 + shape->Sides();
        delete shape;
    }
    return total;
}

int demo_bulk_init_value()
{
    return g_initValue;
}
