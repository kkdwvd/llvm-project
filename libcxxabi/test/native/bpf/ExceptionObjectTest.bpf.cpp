//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// libc++abi's exception_object_alignment, exception_object_alignment.2 and
// test_exception_address_alignment as BPF programs: what
// __cxa_allocate_exception returns is aligned to the target's default
// alignment (8 on BPF, as __attribute__((aligned)) gives), and so is the
// address of a thrown object of minimal alignment.
#include <cstdint>
#include <cxxabi.h>
#include <type_traits>
#include <unwind.h>

#include <std/bpf/testing/BpfTest.h>
#include <std/bpf/verifier/Loop.h>

using std::bpf::opaqueZero;

namespace {
struct S {
  int a[4];
} __attribute__((aligned));

struct __attribute__((aligned)) AlignedType {};
static_assert(alignof(_Unwind_Exception) == alignof(AlignedType), "_Unwind_Exception is incorrectly aligned");

struct MinAligned {};
static_assert(alignof(MinAligned) == 1 && sizeof(MinAligned) == 1, "");

struct exception {
  exception() : x(0) {}
  exception(const exception&) = default;
  exception& operator=(const exception&) = default;
  virtual ~exception() {}
  int x;
};
struct foo : exception {};
} // namespace

BPF_TEST(ExceptionObject, AllocateExceptionAlignment) {
  void* p = __cxxabiv1::__cxa_allocate_exception(16);
  auto i = reinterpret_cast<uintptr_t>(p);
  auto a = std::alignment_of<S>::value;
  EXPECT_EQ(i % a, 0u);
  __cxxabiv1::__cxa_free_exception(p);
}

// Upstream throws ten times in a loop; a throw inside a may_goto loop is a
// landing pad the verifier walks for every state of the loop, past its
// limit (291,740 instructions and -E2BIG for ten), so the throw is made
// twice, unrolled.
BPF_TEST(ExceptionObject, ThrownObjectAlignment) {
  std::uint64_t misaligned = 0;
  try {
    throw MinAligned{};
  } catch (MinAligned const& ref) {
    misaligned += reinterpret_cast<uintptr_t>(&ref) % alignof(AlignedType) != 0;
  }
  try {
    throw MinAligned{};
  } catch (MinAligned const& ref) {
    misaligned += reinterpret_cast<uintptr_t>(&ref) % alignof(AlignedType) != 0;
  }
  EXPECT_EQ(misaligned, 0u);
}

BPF_TEST(ExceptionObject, PolymorphicObjectThrownAndCaught) {
  // exception_object_alignment.2: the compiler makes no assumption about the
  // exception's alignment beyond the ABI's.
  int got = 0;
  try {
    throw foo();
  } catch (...) {
    got = 1;
  }
  EXPECT_EQ(got, 1);
}
