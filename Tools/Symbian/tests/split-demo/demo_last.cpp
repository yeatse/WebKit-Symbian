// Linked last, at the end of the image.
#include "demo.h"

#include <e32base.h>
#include <string.h>

extern "C" int demo_last_mix(int a, int b)
{
    unsigned v = static_cast<unsigned>(a) ^ (static_cast<unsigned>(b) * 40503u);
    v = (v << 13) | (v >> 19);
    return static_cast<int>((v * 5u + 0xe6546b64u) & 0x7fffffff);
}

void demo_last_throw(int code)
{
    throw DemoError { code };
}

void demo_last_leave_l(int code)
{
    User::LeaveIfError(code);
}

namespace {

struct Square : DemoShape {
    int Area(int scale) const override { return scale * scale; }
    int Sides() const override { return 4; }
};

struct Triangle : DemoShape {
    int Area(int scale) const override { return scale * scale / 2; }
    int Sides() const override { return 3; }
};

}

DemoShape* demo_last_make_shape(int kind)
{
    if (kind)
        return new Triangle;
    return new Square;
}

int demo_last_virtual(int seed)
{
    return demo_bulk_virtual(seed) + 1;
}

const char* demo_last_string()
{
    return "split demo string in the last piece";
}

unsigned demo_libc_length(const char* text)
{
    return static_cast<unsigned>(strlen(text));
}
