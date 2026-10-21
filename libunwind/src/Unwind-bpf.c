//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//
//  Implements the unwinding interface of BPF, where the kernel unwinds: the
//  compiler lowers _Unwind_RaiseException() and the resumption of an unwind to
//  the unwind_exit instruction, and a call that may unwind is followed by
//  unwind_goto to its landing pad, which is entered with the exception object
//  and calls the personality routine itself, as a WebAssembly catch does. What
//  remains for the library is the landing pad context the pad fills in and
//  the accessors the personality routine reads it through, as Unwind-wasm.c
//  implements them for WebAssembly.
//
//===----------------------------------------------------------------------===//

#include <stdbool.h>

#include "config.h"

#if defined(__BPF__)

#include "unwind.h"

// Thread-local through emulated TLS, which compiler-rt's emutls.c keeps per
// CPU on BPF.
_Thread_local struct _Unwind_LandingPadContext __bpf_lpad_context;

_LIBUNWIND_EXPORT struct _Unwind_LandingPadContext *
_Unwind_GetBPFLPadContext(void) {
  return &__bpf_lpad_context;
}

/// Called by __cxa_end_catch.
_LIBUNWIND_EXPORT void
_Unwind_DeleteException(_Unwind_Exception *exception_object) {
  if (exception_object->exception_cleanup != NULL)
    (*exception_object->exception_cleanup)(_URC_FOREIGN_EXCEPTION_CAUGHT,
                                           exception_object);
}

/// Called by personality handler to alter register values.
_LIBUNWIND_EXPORT void _Unwind_SetGR(struct _Unwind_Context *context, int index,
                                     uintptr_t value) {
  // We only use this function to set __bpf_lpad_context.selector field, which
  // is index 1 in the personality function.
  if (index == 1)
    ((struct _Unwind_LandingPadContext *)context)->selector = value;
}

/// Called by personality handler to get instruction pointer.
_LIBUNWIND_EXPORT uintptr_t _Unwind_GetIP(struct _Unwind_Context *context) {
  // The result will be used as a 1-based index after decrementing 1, so we
  // increment 2 here
  return ((struct _Unwind_LandingPadContext *)context)->lpad_index + 2;
}

/// Not used in BPF.
_LIBUNWIND_EXPORT void _Unwind_SetIP(struct _Unwind_Context *context,
                                     uintptr_t value) {}

/// Called by personality handler to get LSDA for current frame.
_LIBUNWIND_EXPORT uintptr_t
_Unwind_GetLanguageSpecificData(struct _Unwind_Context *context) {
  return ((struct _Unwind_LandingPadContext *)context)->lsda;
}

/// Not used in BPF.
_LIBUNWIND_EXPORT uintptr_t
_Unwind_GetRegionStart(struct _Unwind_Context *context) {
  return 0;
}

#endif // defined(__BPF__)
