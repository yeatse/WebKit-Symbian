#pragma once
#include <cstdint>
namespace eka2l1 {
template<class T> class ptr {
    T* value = nullptr;
public:
    ptr() = default;
    ptr(std::uintptr_t v) : value(reinterpret_cast<T*>(v)) { }
    explicit ptr(T* p) : value(p) { }
    explicit operator bool() const { return value; }
    T* get() const { return value; }
};
}
