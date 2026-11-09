// Pieces of the split demo: code spread over the image so split-e32.py's cuts
// fall between callers and callees.
#pragma once

typedef int (*DemoFn)(int);

extern "C" int demo_first_mix(int a, int b);
extern "C" int demo_last_mix(int a, int b);
int demo_bulk_chain(int start, int depth);
int demo_bulk_const_chain(int start, int depth);
int demo_bulk_virtual(int seed);
int demo_bulk_init_value();
int demo_first_catch(int code);
int demo_first_trap(int code);
void demo_last_throw(int code);
void demo_last_leave_l(int code);
int demo_last_virtual(int seed);
const char* demo_last_string();
unsigned demo_libc_length(const char* text);

// Thrown by value: its type information is the demo's own (a firmware
// type such as int's has no import proxy).
struct DemoError {
    int code;
};

struct DemoShape {
    virtual ~DemoShape() { }
    virtual int Area(int scale) const = 0;
    virtual int Sides() const = 0;
};
DemoShape* demo_last_make_shape(int kind);
