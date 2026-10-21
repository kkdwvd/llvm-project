//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef __BPF_UNWIND_H__
#define __BPF_UNWIND_H__

// The kernel unwinds, and a landing pad calls the personality routine itself,
// as a WebAssembly catch does: this is the channel between the two, as
// unwind_wasm.h declares it for WebAssembly.
struct _Unwind_LandingPadContext {
  // Input information to personality function
  uintptr_t lpad_index; // landing pad index
  uintptr_t lsda;       // LSDA address

  // Output information computed by personality function
  uintptr_t selector; // selector value
};

// Communication channel between compiler-generated user code and personality
// function
#ifdef __cplusplus
extern "C"
#endif
    struct _Unwind_LandingPadContext *_Unwind_GetBPFLPadContext(void);

#endif // __BPF_UNWIND_H__
