//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// The guards of function-local statics on BPF (cxa_guard_impl.h's
// InitByteBpf), as BPF programs: what a program's statics do through the
// compiler's guard calls, and the ABI's functions called directly on a guard
// of the program's own, as guard_test_basic does on the other targets. A
// program is one CPU; what two CPUs do to one guard at once is tested by the
// platform's suite (std::bpf's ConcurrencyTest), which runs programs on two
// CPUs. The recursion test is a program that must end through
// std::terminate(), as the *Terminates suites are.
#include <cstdint>
#include <cxxabi.h>

#include <std/bpf/testing/BpfTest.h>
#include <std/bpf/verifier/Loop.h>

using std::bpf::opaqueZero;

static std::uint64_t initializerRuns;
static std::uint64_t throwingRuns;

namespace {

struct Counted {
  std::uint64_t value;
  explicit Counted(std::uint64_t v) : value(v) { initializerRuns += 1; }
};

struct Failing {
  explicit Failing(std::uint64_t v) {
    throwingRuns += 1;
    if (v == 0)
      throw 7;
  }
};

/// A static initialized once, however often it is reached.
[[gnu::noinline]] std::uint64_t lazy(std::uint64_t v) {
  static const Counted counted(v);
  return counted.value;
}

/// A static whose initializer throws until it is given a non-zero value:
/// each failure aborts the guard, and the next call tries again.
[[gnu::noinline]] bool failing(std::uint64_t v) {
  static const Failing object(v);
  return true;
}

} // namespace

BPF_TEST(Guard, AStaticIsInitializedOnce) {
  initializerRuns = 0;
  EXPECT_TRUE(lazy(41 + opaqueZero()) == 41);
  EXPECT_TRUE(lazy(99 + opaqueZero()) == 41);
  EXPECT_TRUE(lazy(opaqueZero()) == 41);
  EXPECT_TRUE(initializerRuns == 1);
}

/// One attempt at the static whose initializer throws for a zero.
[[gnu::always_inline]] static inline std::uint64_t attempt(std::uint64_t v) {
  try {
    failing(v);
  } catch (int) {
    return 1;
  }
  return 0;
}

BPF_TEST(Guard, AnInitializerThatThrowsIsRetried) {
  throwingRuns = 0;
  // Three attempts as three sites: a handler inside a loop costs the
  // verifier a landing pad per state of the loop, past its limit.
  EXPECT_TRUE(attempt(opaqueZero()) == 1);
  EXPECT_TRUE(attempt(opaqueZero()) == 1);
  EXPECT_TRUE(attempt(opaqueZero()) == 1);
  EXPECT_TRUE(throwingRuns == 3);
  EXPECT_TRUE(attempt(1 + opaqueZero()) == 0);
  EXPECT_TRUE(throwingRuns == 4);
  EXPECT_TRUE(attempt(opaqueZero()) == 0);
  EXPECT_TRUE(throwingRuns == 4);
}

/// A guard of the program's own, driven through the ABI: the 64-bit object
/// clang emits for a static, zero until initialized, whose first byte says
/// it is.
static std::uint64_t guard;

BPF_TEST(Guard, TheAbiOnAGuardOfOurOwn) {
  guard = 0;
  EXPECT_TRUE(__cxxabiv1::__cxa_guard_acquire(&guard) == 1);
  // Pending: the first byte, which the compiler tests, stays unset.
  EXPECT_TRUE((guard & 0xff) == 0);
  EXPECT_TRUE(guard != 0);
  __cxxabiv1::__cxa_guard_abort(&guard);
  EXPECT_TRUE(guard == 0);
  EXPECT_TRUE(__cxxabiv1::__cxa_guard_acquire(&guard) == 1);
  __cxxabiv1::__cxa_guard_release(&guard);
  EXPECT_TRUE((guard & 0xff) == 1);
  EXPECT_TRUE(__cxxabiv1::__cxa_guard_acquire(&guard) == 0);
  EXPECT_TRUE(__cxxabiv1::__cxa_guard_acquire(&guard) == 0);
}

BPF_TEST(GuardTerminates, ARecursiveInitialization) {
  // A guard acquired again on the CPU that holds it, which is what an
  // initializer that reaches its own static does (the verifier refuses
  // the recursive call itself, so the ABI is called directly): the program
  // ends through std::terminate(), as __cxa_guard_acquire ends a recursion
  // on every target.
  guard = 0;
  EXPECT_TRUE(__cxxabiv1::__cxa_guard_acquire(&guard) == 1);
  EXPECT_TRUE(__cxxabiv1::__cxa_guard_acquire(&guard) == 1);
  EXPECT_TRUE(false);
}
