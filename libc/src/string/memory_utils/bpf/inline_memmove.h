//===-- Memmove implementation for BPF --------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC_STRING_MEMORY_UTILS_BPF_INLINE_MEMMOVE_H
#define LLVM_LIBC_SRC_STRING_MEMORY_UTILS_BPF_INLINE_MEMMOVE_H

#include "src/__support/macros/attributes.h"   // LIBC_INLINE
#include "src/__support/macros/config.h"       // LIBC_NAMESPACE_DECL
#include "src/__support/macros/optimization.h" // LIBC_CAN_LOOP
#include "src/string/memory_utils/utils.h"     // Ptr, CPtr

#include <stddef.h> // size_t

namespace LIBC_NAMESPACE_DECL {

// Byte loops bounded for the verifier, as bpf/inline_memcpy.h says. The
// backward copy counts down from the end so that both directions index with
// one unsigned offset the verifier can follow.
[[maybe_unused]] LIBC_INLINE void inline_memmove_bpf(Ptr dst, CPtr src,
                                                     size_t count) {
  if (count == 0 || dst == src)
    return;
  if (dst < src) {
    for (size_t offset = LIBC_OPAQUE_ZERO; offset < count && LIBC_CAN_LOOP; ++offset)
      dst[offset] = src[offset];
  } else {
    for (size_t offset = count + LIBC_OPAQUE_ZERO; offset != 0 && LIBC_CAN_LOOP; --offset)
      dst[offset - 1] = src[offset - 1];
  }
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_BPF_INLINE_MEMMOVE_H
