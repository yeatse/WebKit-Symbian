#ifndef EKA_SYMBIAN_CLANG_NATIVE_PATCH_COMPAT_H
#define EKA_SYMBIAN_CLANG_NATIVE_PATCH_COMPAT_H

// ARM EABI euser exports descriptor formatters with std::__va_list.
// Clang's ARM builtin supplies this exact ABI and saves variadic GPRs;
// e32def.h's generic TInt8*[1] fallback supplies neither.
#if !defined(__clang__) || !defined(__arm__)
#error This header requires the Symbian SDK ARM Clang toolchain
#endif
typedef __builtin_va_list VA_LIST;
#define __VA_LIST_defined
#define VA_START(ap, last) __builtin_va_start(ap, last)
#define VA_ARG(ap, type) __builtin_va_arg(ap, type)
#define VA_END(ap) __builtin_va_end(ap)

#endif
