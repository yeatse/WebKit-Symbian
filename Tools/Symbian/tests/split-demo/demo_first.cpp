// Linked first: the split puts these at the start of the image, so the bulk
// and the last file reach them across cuts.
#include "demo.h"

#include <e32base.h>

extern "C" int demo_first_mix(int a, int b)
{
    unsigned v = static_cast<unsigned>(a) * 2654435761u;
    v ^= static_cast<unsigned>(b) + 0x9e3779b9u + (v << 6) + (v >> 2);
    return static_cast<int>(v & 0x7fffffff);
}

// A C++ exception thrown in the last piece, caught here.
int demo_first_catch(int code)
{
    try {
        demo_last_throw(code);
    } catch (const DemoError& error) {
        return error.code;
    } catch (...) {
        return -1000;
    }
    return -2000;
}

// A leave from the last piece, trapped here.
int demo_first_trap(int code)
{
    TRAPD(error, demo_last_leave_l(code));
    return error;
}
