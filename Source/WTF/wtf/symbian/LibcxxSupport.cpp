/*
 * Copyright (C) 2026 The symbian-webkit authors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Pieces of the C++ and C runtimes that WTF and JavaScriptCore reference
// but the symbian-platform SDK runtime does not provide.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <exception>

// Open C's libm predates C99 log2.
extern "C" double log2(double x)
{
    return log(x) * 1.44269504088896340736; // log2(e)
}

extern "C" float log2f(float x)
{
    return static_cast<float>(log2(x));
}

// The SDK runtime has the other EABI float/int64 conversions, but not this one.
extern "C" long long __aeabi_f2lz(float value)
{
    return static_cast<long long>(static_cast<double>(value));
}

_LIBCPP_BEGIN_NAMESPACE_STD
_LIBCPP_BEGIN_EXPLICIT_ABI_ANNOTATIONS

// libc++ declares these std::sort helpers as extern templates that live in
// the shared library (src/algorithm.cpp), which the SDK runtime does not
// include.
template <class Comp, class RandomAccessIterator>
void __sort(RandomAccessIterator first, RandomAccessIterator last, Comp comp)
{
    // As libc++'s src/algorithm.cpp: go straight to the introsort, because
    // __sort_dispatch routes arithmetic types back to this function.
    (void)comp;
    if (first == last)
        return;
    __less<> less;
    auto depthLimit = 2 * std::__bit_log2(std::__to_unsigned_like(last - first));
    std::__introsort<_ClassicAlgPolicy, __less<>&, RandomAccessIterator,
        __use_branchless_sort<__less<>, RandomAccessIterator>>(first, last, less, depthLimit);
}

template void __sort<__less<char>&, char*>(char*, char*, __less<char>&);
template void __sort<__less<wchar_t>&, wchar_t*>(wchar_t*, wchar_t*, __less<wchar_t>&);
template void __sort<__less<signed char>&, signed char*>(signed char*, signed char*, __less<signed char>&);
template void __sort<__less<unsigned char>&, unsigned char*>(unsigned char*, unsigned char*, __less<unsigned char>&);
template void __sort<__less<short>&, short*>(short*, short*, __less<short>&);
template void __sort<__less<unsigned short>&, unsigned short*>(unsigned short*, unsigned short*, __less<unsigned short>&);
template void __sort<__less<int>&, int*>(int*, int*, __less<int>&);
template void __sort<__less<unsigned>&, unsigned*>(unsigned*, unsigned*, __less<unsigned>&);
template void __sort<__less<long>&, long*>(long*, long*, __less<long>&);
template void __sort<__less<unsigned long>&, unsigned long*>(unsigned long*, unsigned long*, __less<unsigned long>&);
template void __sort<__less<long long>&, long long*>(long long*, long long*, __less<long long>&);
template void __sort<__less<unsigned long long>&, unsigned long long*>(unsigned long long*, unsigned long long*, __less<unsigned long long>&);
template void __sort<__less<float>&, float*>(float*, float*, __less<float>&);
template void __sort<__less<double>&, double*>(double*, double*, __less<double>&);
template void __sort<__less<long double>&, long double*>(long double*, long double*, __less<long double>&);

_LIBCPP_END_EXPLICIT_ABI_ANNOTATIONS
_LIBCPP_END_NAMESPACE_STD

_LIBCPP_BEGIN_UNVERSIONED_NAMESPACE_STD
_LIBCPP_BEGIN_EXPLICIT_ABI_ANNOTATIONS

// The runtime's <future> support refers to exception_ptr. Code here is built
// without exceptions, so an exception_ptr is never non-null.
exception_ptr::~exception_ptr() noexcept { }
exception_ptr::exception_ptr(const exception_ptr& other) noexcept
    : __ptr_(other.__ptr_)
{
}
exception_ptr& exception_ptr::operator=(const exception_ptr& other) noexcept
{
    __ptr_ = other.__ptr_;
    return *this;
}
[[noreturn]] void rethrow_exception(exception_ptr)
{
    abort();
}

_LIBCPP_END_EXPLICIT_ABI_ANNOTATIONS
_LIBCPP_END_UNVERSIONED_NAMESPACE_STD
