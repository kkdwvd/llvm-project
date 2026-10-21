//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef __ABORT_MESSAGE_H_
#define __ABORT_MESSAGE_H_

#include "cxxabi.h"

#if defined(__BPF__)
// BPF refuses a variadic definition and has no stream to write to: the message
// is dropped and the program ends through the kernel's bpf_throw(), as
// std::terminate() ends it (abort_message.cpp). A variadic template keeps the
// printf-style call sites of the library unchanged.
extern "C" [[noreturn]] _LIBCXXABI_HIDDEN void __bpf_abort(void) noexcept;
template <class... _Args>
[[noreturn]] inline void __abort_message(const char*, _Args...) noexcept {
  __bpf_abort();
}
#else
extern "C" [[noreturn]] _LIBCXXABI_HIDDEN void
__abort_message(const char *format, ...) __attribute__((format(printf, 1, 2)));
#endif

#ifndef _LIBCXXABI_ASSERT
#  define _LIBCXXABI_ASSERT(expr, msg)                                                                                 \
    do {                                                                                                               \
      if (!(expr)) {                                                                                                   \
        char const* __msg = (msg);                                                                                     \
        ::__abort_message("%s:%d: %s", __FILE__, __LINE__, __msg);                                                     \
      }                                                                                                                \
    } while (false)

#endif

#endif // __ABORT_MESSAGE_H_
