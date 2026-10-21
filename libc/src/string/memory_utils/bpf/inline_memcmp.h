//===-- Memcmp implementation for BPF ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC_STRING_MEMORY_UTILS_BPF_INLINE_MEMCMP_H
#define LLVM_LIBC_SRC_STRING_MEMORY_UTILS_BPF_INLINE_MEMCMP_H

#include "src/__support/macros/attributes.h"   // LIBC_INLINE
#include "src/__support/macros/config.h"       // LIBC_NAMESPACE_DECL
#include "src/__support/macros/optimization.h" // LIBC_CAN_LOOP
#include "src/string/memory_utils/utils.h"     // CPtr, MemcmpReturnType

#include <stddef.h> // size_t
#include <stdint.h> // int32_t

namespace LIBC_NAMESPACE_DECL {

// A byte loop bounded for the verifier, as bpf/inline_memcpy.h says.
[[maybe_unused]] LIBC_INLINE MemcmpReturnType inline_memcmp_bpf(CPtr p1,
                                                                 CPtr p2,
                                                                 size_t count) {
  for (size_t offset = LIBC_OPAQUE_ZERO; offset < count && LIBC_CAN_LOOP; ++offset) {
    const int32_t a = static_cast<int32_t>(p1[offset]);
    const int32_t b = static_cast<int32_t>(p2[offset]);
    const int32_t diff = a - b;
    if (diff)
      return diff;
  }
  return MemcmpReturnType::zero();
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_BPF_INLINE_MEMCMP_H
