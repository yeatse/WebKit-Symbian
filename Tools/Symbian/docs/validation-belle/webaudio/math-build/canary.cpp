#include <cmath>
#include <type_traits>
static_assert(std::is_same_v<decltype(std::log2(1.f)), float>);
extern "C" float _start(float value) { return std::log2(value); }
