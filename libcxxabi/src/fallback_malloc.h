//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _FALLBACK_MALLOC_H
#define _FALLBACK_MALLOC_H

#include "__cxxabi_config.h"
#include <stddef.h> // for size_t

#if defined(__BPF__)
#  include "bpf_platform.h"
#endif

namespace __cxxabiv1 {

#if defined(__BPF__)
// The allocation and release with the fallback as global subprograms taking
// and returning the numbers the addresses are, checked once per program
// (bpf_platform.h); the functions below wrap them.
_LIBCXXABI_BPF_GLOBAL_SUBPROGRAM unsigned long long __libcxxabi_bpf_aligned_malloc(unsigned long long size) noexcept;
_LIBCXXABI_BPF_GLOBAL_SUBPROGRAM void __libcxxabi_bpf_aligned_free(unsigned long long address) noexcept;
#endif

// Allocate some memory from _somewhere_
_LIBCXXABI_HIDDEN void * __aligned_malloc_with_fallback(size_t size) _LIBCXXABI_NOEXCEPT;

// Allocate and zero-initialize memory from _somewhere_
_LIBCXXABI_HIDDEN void * __calloc_with_fallback(size_t count, size_t size) _LIBCXXABI_NOEXCEPT;

_LIBCXXABI_HIDDEN void __aligned_free_with_fallback(void *ptr) _LIBCXXABI_NOEXCEPT;
_LIBCXXABI_HIDDEN void __free_with_fallback(void *ptr) _LIBCXXABI_NOEXCEPT;

} // namespace __cxxabiv1

#endif
