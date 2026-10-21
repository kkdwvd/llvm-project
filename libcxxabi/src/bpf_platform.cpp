//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// What bpf_platform.h declares and defines nowhere else.
//
//===----------------------------------------------------------------------===//

#include "bpf_platform.h"

namespace __cxxabiv1 {

// Always zero: what _LIBCXXABI_OPAQUE_ZERO loads.
extern "C" unsigned __libcxxabi_bpf_zero = 0;

} // namespace __cxxabiv1
