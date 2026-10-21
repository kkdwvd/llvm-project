//===-- Memcpy implementation for BPF ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// A BPF program runs under the kernel's verifier, which walks every path of
// a loop and admits a loop over a count it cannot bound only when an
// iteration can leave it through a may_goto instruction, which LIBC_CAN_LOOP
// is. The copy is a byte loop so bounded; the backend expands a copy of a
// known size inline before it would call this, so the loop is the general
// case and nothing else.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC_STRING_MEMORY_UTILS_BPF_INLINE_MEMCPY_H
#define LLVM_LIBC_SRC_STRING_MEMORY_UTILS_BPF_INLINE_MEMCPY_H

#include "src/__support/macros/attributes.h"   // LIBC_INLINE
#include "src/__support/macros/config.h"       // LIBC_NAMESPACE_DECL
#include "src/__support/macros/optimization.h" // LIBC_CAN_LOOP
#include "src/string/memory_utils/utils.h"     // Ptr, CPtr

#include <stddef.h> // size_t

namespace LIBC_NAMESPACE_DECL {

[[maybe_unused]] LIBC_INLINE void
inline_memcpy_bpf(Ptr __restrict dst, CPtr __restrict src, size_t count) {
  for (size_t offset = LIBC_OPAQUE_ZERO; offset < count && LIBC_CAN_LOOP; ++offset)
    dst[offset] = src[offset];
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_BPF_INLINE_MEMCPY_H
