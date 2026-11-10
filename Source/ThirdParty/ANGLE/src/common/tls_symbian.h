// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMMON_TLS_SYMBIAN_H_
#define COMMON_TLS_SYMBIAN_H_

#include "common/tls.h"

#if defined(ANGLE_PLATFORM_SYMBIAN)
#include <cstdlib>
#include <new>

namespace angle
{
// Symbian E32 images have no ELF TLS segment or __aeabi_read_tp. Keep each
// logical variable in its own pthread key and release its storage at thread exit.
// Tag distinguishes variables that happen to have the same value type.
template <typename Tag, typename T>
T &SymbianThreadLocal()
{
    static const TLSIndex index = [] {
        TLSIndex key = CreateTLSIndex([](void *value) { delete static_cast<T *>(value); });
        if (key == TLS_INVALID_INDEX)
            std::abort();
        return key;
    }();
    T *value = static_cast<T *>(GetTLSValue(index));
    if (!value)
    {
        value = new (std::nothrow) T();
        if (!value)
            std::abort();
        if (!SetTLSValue(index, value))
        {
            delete value;
            std::abort();
        }
    }
    return *value;
}
}  // namespace angle
#endif

#endif  // COMMON_TLS_SYMBIAN_H_
