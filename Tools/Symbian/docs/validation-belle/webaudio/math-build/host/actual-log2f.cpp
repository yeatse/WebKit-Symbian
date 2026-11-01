#include <cmath>
extern "C" float owned_log2f(float x)
{
    // Calling log2 here can be folded back to log2f by the compiler.
    return static_cast<float>(log(static_cast<double>(x)) * 1.44269504088896340736);
}
