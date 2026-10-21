//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// What the BPF platform gives the library and what its verifier asks of it.
//
// A BPF program runs in the kernel under the verifier, which walks every path
// of the program. Three of its rules shape the code of the library on BPF:
//
// - Every loop must be one the verifier can bound. A loop over a count it
//   cannot see ends only through a may_goto instruction, which the kernel
//   takes once a budget of iterations is spent; _LIBCXXABI_CAN_LOOP is that
//   instruction as a loop condition, the kernel's own can_loop. A counter that
//   starts at a literal is a precise value the verifier follows one iteration
//   per state up to its limit, where a counter loaded from memory is a number
//   with bounds that one pass covers: _LIBCXXABI_OPAQUE_ZERO is that zero.
//   llvm-libc's LIBC_CAN_LOOP and LIBC_OPAQUE_ZERO are the same two.
//
// - A function with hidden visibility is a static subprogram to libbpf, which
//   the verifier checks again at every call with what the caller passes; one
//   with default visibility is a global subprogram, checked once on its own
//   against its BTF prototype, whose parameters and result must then be
//   integers or pointers to kernel types. The library is built hidden, and
//   the walks of a type_info graph and of the fallback heap and the
//   personality routine's scan of an exception table, which cost the
//   verifier the most, are global subprograms that take and return the
//   numbers their pointers are, so that each is checked once per program:
//   _LIBCXXABI_BPF_GLOBAL_SUBPROGRAM declares one.
//
// - A call through a pointer loaded from memory is verified by checking every
//   function whose address the program stores as a possible callee, and such a
//   callee may not make such a call itself. Which calls the library makes
//   through a stored pointer is therefore restricted; cxa_exception.cpp and
//   cxa_handlers.cpp say where.
//
// The platform's side: the helper that names the CPU, which the guard of a
// function-local static records (cxa_guard_impl.h), and the kfunc that ends
// the program (abort_message.cpp). A helper is called by its number, as every
// BPF program calls it, and a kfunc through a weak extern in the .ksyms
// section, which the loader binds to the kernel's function; neither needs a
// kernel header. The memory functions are libc's, left to the platform, and
// what is per thread elsewhere is thread_local here as well, which emulated
// TLS keeps per CPU.
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCXXABI_BPF_PLATFORM_H
#define _LIBCXXABI_BPF_PLATFORM_H

#if defined(__BPF__)

#  define _LIBCXXABI_CAN_LOOP                                                                                          \
    (__extension__({                                                                                                   \
      __label__ __l_break, __l_continue;                                                                               \
      int __ret = 1;                                                                                                   \
      asm volatile goto("may_goto %l[__l_break]" : : : : __l_break);                                                   \
      goto __l_continue;                                                                                               \
    __l_break:                                                                                                         \
      __ret = 0;                                                                                                       \
    __l_continue:;                                                                                                     \
      __ret;                                                                                                           \
    }))

namespace __cxxabiv1 {
// Always zero, in memory: a load the verifier cannot see through.
extern "C" __attribute__((__visibility__("hidden"))) unsigned __libcxxabi_bpf_zero;
} // namespace __cxxabiv1

#  define _LIBCXXABI_OPAQUE_ZERO (__cxxabiv1::__libcxxabi_bpf_zero)

// A global subprogram: C linkage for a plain name in BTF, default visibility
// against the library's -fvisibility=hidden, and never inlined into its
// callers, where its loops would be checked at every call site.
#  define _LIBCXXABI_BPF_GLOBAL_SUBPROGRAM extern "C" __attribute__((__noinline__, __visibility__("default")))

// A value loaded from memory, pinned in a register before a range check: the
// compiler would otherwise reload it or fold the check into a comparison the
// verifier cannot follow.
template <class _Tp>
inline _Tp __libcxxabi_bpf_checked(_Tp __value) {
  asm volatile("" : "+r"(__value));
  return __value;
}

// The helper that names the CPU the program runs on: helper 8 of the kernel's
// stable set, called by its number as bpf_helper_defs.h calls it.
static unsigned (*const __libcxxabi_bpf_cpu)(void) noexcept = reinterpret_cast<unsigned (*)(void) noexcept>(8);

// The kfunc that ends the program: every frame is discarded and the program
// returns the cookie, 0, a value every program type accepts when it has no
// exception callback of its own. Weak, as the kernel's own headers declare
// every kfunc, so that an object that never calls it loads on a kernel that
// lacks it.
extern "C" [[__noreturn__]] void bpf_throw(unsigned long long __cookie) noexcept
    __attribute__((__weak__, __section__(".ksyms")));

#else // __BPF__

#  define _LIBCXXABI_CAN_LOOP true
#  define _LIBCXXABI_OPAQUE_ZERO 0

#endif // __BPF__

#endif // _LIBCXXABI_BPF_PLATFORM_H
