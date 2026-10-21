//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// libc++abi's catch_class_01..04, inherited_exception, catch_in_noexcept,
// catch_array_01/02, catch_function_01..03, unwind_01, uncaught_exception
// and uncaught_exceptions, as BPF programs: each upstream test is one
// BPF_TEST, its assert() an EXPECT_TRUE, its main() the body. The class
// counters are globals, in the arena as every global of the object is.
#include <cstdint>
#include <cxxabi.h>
#include <exception>

#include <std/bpf/testing/BpfTest.h>

namespace catch_class_01 {
struct A {
  static int count;
  int id_;
  explicit A(int id) : id_(id) { count++; }
  A(const A& a) : id_(a.id_) { count++; }
  ~A() { count--; }
};
int A::count = 0;

static void f1() { throw A(3); }

static void f2() {
  try {
    EXPECT_TRUE(A::count == 0);
    f1();
  } catch (A a) {
    EXPECT_TRUE(A::count != 0);
    EXPECT_TRUE(a.id_ == 3);
    throw;
  }
}
} // namespace catch_class_01

BPF_TEST(CatchClass, Test01) {
  using namespace catch_class_01;
  A::count = 0;
  try {
    f2();
    EXPECT_TRUE(false);
  } catch (const A& a) {
    EXPECT_TRUE(A::count != 0);
    EXPECT_TRUE(a.id_ == 3);
  }
  EXPECT_TRUE(A::count == 0);
}

namespace catch_class_02 {
struct B {
  static int count;
  int id_;
  explicit B(int id) : id_(id) { count++; }
  B(const B& a) : id_(a.id_) { count++; }
  ~B() { count--; }
};
int B::count = 0;

struct A : B {
  static int count;
  int id_;
  explicit A(int id) : B(id - 1), id_(id) { count++; }
  A(const A& a) : B(a.id_ - 1), id_(a.id_) { count++; }
  ~A() { count--; }
};
int A::count = 0;

static void f1() {
  EXPECT_TRUE(A::count == 0);
  EXPECT_TRUE(B::count == 0);
  A a(3);
  EXPECT_TRUE(A::count == 1);
  EXPECT_TRUE(B::count == 1);
  throw a;
}

static void f2() {
  try {
    EXPECT_TRUE(A::count == 0);
    f1();
    EXPECT_TRUE(false);
  } catch (A a) {
    EXPECT_TRUE(A::count != 0);
    EXPECT_TRUE(B::count != 0);
    EXPECT_TRUE(a.id_ == 3);
    throw;
  } catch (B b) {
    EXPECT_TRUE(false);
  }
}
} // namespace catch_class_02

BPF_TEST(CatchClass, Test02) {
  using namespace catch_class_02;
  A::count = 0;
  B::count = 0;
  try {
    f2();
    EXPECT_TRUE(false);
  } catch (const B& b) {
    EXPECT_TRUE(B::count != 0);
    EXPECT_TRUE(b.id_ == 2);
  }
  EXPECT_TRUE(A::count == 0);
  EXPECT_TRUE(B::count == 0);
}

namespace catch_class_03 {
struct B {
  static int count;
  int id_;
  explicit B(int id) : id_(id) { count++; }
  B(const B& a) : id_(a.id_) { count++; }
  ~B() { count--; }
};
int B::count = 0;

struct C1 : B {
  static int count;
  int id_;
  explicit C1(int id) : B(id - 2), id_(id) { count++; }
  C1(const C1& a) : B(a.id_ - 2), id_(a.id_) { count++; }
  ~C1() { count--; }
};
int C1::count = 0;

struct C2 : B {
  static int count;
  int id_;
  explicit C2(int id) : B(id - 2), id_(id) { count++; }
  C2(const C2& a) : B(a.id_ - 2), id_(a.id_) { count++; }
  ~C2() { count--; }
};
int C2::count = 0;

struct A : C1, C2 {
  static int count;
  int id_;
  explicit A(int id) : C1(id - 1), C2(id - 2), id_(id) { count++; }
  A(const A& a) : C1(a.id_ - 1), C2(a.id_ - 2), id_(a.id_) { count++; }
  ~A() { count--; }
};
int A::count = 0;

static void f1() {
  EXPECT_TRUE(A::count == 0);
  EXPECT_TRUE(C1::count == 0);
  EXPECT_TRUE(C2::count == 0);
  EXPECT_TRUE(B::count == 0);
  A a(5);
  EXPECT_TRUE(A::count == 1);
  EXPECT_TRUE(C1::count == 1);
  EXPECT_TRUE(C2::count == 1);
  EXPECT_TRUE(B::count == 2);

  EXPECT_TRUE(a.id_ == 5);
  EXPECT_TRUE(static_cast<C1&>(a).id_ == 4);
  EXPECT_TRUE(static_cast<C2&>(a).id_ == 3);
  EXPECT_TRUE(static_cast<B&>(static_cast<C1&>(a)).id_ == 2);
  EXPECT_TRUE(static_cast<B&>(static_cast<C2&>(a)).id_ == 1);
  throw a;
}

static void f2() {
  try {
    f1();
    EXPECT_TRUE(false);
  } catch (const A& a) { // can catch A
    EXPECT_TRUE(a.id_ == 5);
    EXPECT_TRUE(static_cast<const C1&>(a).id_ == 4);
    EXPECT_TRUE(static_cast<const C2&>(a).id_ == 3);
    EXPECT_TRUE(static_cast<const B&>(static_cast<const C1&>(a)).id_ == 2);
    EXPECT_TRUE(static_cast<const B&>(static_cast<const C2&>(a)).id_ == 1);
    throw;
  } catch (const C1&) {
    EXPECT_TRUE(false);
  } catch (const C2&) {
    EXPECT_TRUE(false);
  } catch (const B&) {
    EXPECT_TRUE(false);
  }
}

static void f3() {
  try {
    f2();
    EXPECT_TRUE(false);
  } catch (const B& a) { // can not catch B (ambiguous base)
    EXPECT_TRUE(false);
  } catch (const C1& c1) { // can catch C1
    EXPECT_TRUE(c1.id_ == 4);
    EXPECT_TRUE(static_cast<const B&>(c1).id_ == 2);
    throw;
  } catch (const C2&) {
    EXPECT_TRUE(false);
  }
}

static void f4() {
  try {
    f3();
    EXPECT_TRUE(false);
  } catch (const B& a) { // can not catch B (ambiguous base)
    EXPECT_TRUE(false);
  } catch (const C2& c2) { // can catch C2
    EXPECT_TRUE(c2.id_ == 3);
    EXPECT_TRUE(static_cast<const B&>(c2).id_ == 1);
    throw;
  } catch (const C1&) {
    EXPECT_TRUE(false);
  }
}
} // namespace catch_class_03

BPF_TEST(CatchClass, Test03) {
  using namespace catch_class_03;
  A::count = C1::count = C2::count = B::count = 0;
  try {
    f4();
    EXPECT_TRUE(false);
  } catch (...) {
  }
  EXPECT_TRUE(A::count == 0);
  EXPECT_TRUE(C1::count == 0);
  EXPECT_TRUE(C2::count == 0);
  EXPECT_TRUE(B::count == 0);
}

namespace catch_class_04 {
struct B {
  static int count;
  int id_;
  explicit B(int id) : id_(id) { count++; }
  B(const B& a) : id_(a.id_) { count++; }
  ~B() { count--; }
};
int B::count = 0;

struct C1 : virtual B {
  static int count;
  int id_;
  explicit C1(int id) : B(id - 2), id_(id) { count++; }
  C1(const C1& a) : B(a.id_ - 2), id_(a.id_) { count++; }
  ~C1() { count--; }
};
int C1::count = 0;

struct C2 : virtual private B {
  static int count;
  int id_;
  explicit C2(int id) : B(id - 2), id_(id) { count++; }
  C2(const C2& a) : B(a.id_ - 2), id_(a.id_) { count++; }
  ~C2() { count--; }
};
int C2::count = 0;

struct A : C1, C2 {
  static int count;
  int id_;
  explicit A(int id) : B(id + 3), C1(id - 1), C2(id - 2), id_(id) { count++; }
  A(const A& a) : B(a.id_ + 3), C1(a.id_ - 1), C2(a.id_ - 2), id_(a.id_) { count++; }
  ~A() { count--; }
};
int A::count = 0;

// The object with virtual bases lives in the arena: a constructor reaches a
// virtual base at an offset it reads from the vtable, which the verifier
// cannot bound on the stack.
static void f1() {
  EXPECT_TRUE(A::count == 0);
  EXPECT_TRUE(C1::count == 0);
  EXPECT_TRUE(C2::count == 0);
  EXPECT_TRUE(B::count == 0);
  A* a = new A(5);
  EXPECT_TRUE(A::count == 1);
  EXPECT_TRUE(C1::count == 1);
  EXPECT_TRUE(C2::count == 1);
  EXPECT_TRUE(B::count == 1);

  EXPECT_TRUE(a->id_ == 5);
  EXPECT_TRUE(static_cast<C1&>(*a).id_ == 4);
  EXPECT_TRUE(static_cast<C2&>(*a).id_ == 3);
  EXPECT_TRUE(static_cast<B&>(*a).id_ == 8);
  struct Deleter {
    A* a;
    ~Deleter() { delete a; }
  } deleter{a};
  throw *a;
}

static void f2() {
  try {
    f1();
    EXPECT_TRUE(false);
  } catch (const A& a) { // can catch A
    EXPECT_TRUE(a.id_ == 5);
    EXPECT_TRUE(static_cast<const C1&>(a).id_ == 4);
    EXPECT_TRUE(static_cast<const C2&>(a).id_ == 3);
    EXPECT_TRUE(static_cast<const B&>(a).id_ == 8);
    throw;
  } catch (const C1&) {
    EXPECT_TRUE(false);
  } catch (const C2&) {
    EXPECT_TRUE(false);
  } catch (const B&) {
    EXPECT_TRUE(false);
  }
}

static void f3() {
  try {
    f2();
    EXPECT_TRUE(false);
  } catch (const B& a) { // can catch B
    EXPECT_TRUE(static_cast<const B&>(a).id_ == 8);
    throw;
  } catch (const C1& c1) {
    EXPECT_TRUE(false);
  } catch (const C2&) {
    EXPECT_TRUE(false);
  }
}

static void f4() {
  try {
    f3();
    EXPECT_TRUE(false);
  } catch (const C2& c2) { // can catch C2
    EXPECT_TRUE(c2.id_ == 3);
    throw;
  } catch (const B& a) {
    EXPECT_TRUE(false);
  } catch (const C1&) {
    EXPECT_TRUE(false);
  }
}

static void f5() {
  try {
    f4();
    EXPECT_TRUE(false);
  } catch (const C1& c1) { // can catch C1
    EXPECT_TRUE(c1.id_ == 4);
    EXPECT_TRUE(static_cast<const B&>(c1).id_ == 8);
    throw;
  } catch (const B& a) {
    EXPECT_TRUE(false);
  } catch (const C2&) {
    EXPECT_TRUE(false);
  }
}
} // namespace catch_class_04

BPF_TEST(CatchClass, Test04) {
  using namespace catch_class_04;
  A::count = C1::count = C2::count = B::count = 0;
  try {
    f5();
    EXPECT_TRUE(false);
  } catch (...) {
  }
  EXPECT_TRUE(A::count == 0);
  EXPECT_TRUE(C1::count == 0);
  EXPECT_TRUE(C2::count == 0);
  EXPECT_TRUE(B::count == 0);
}

namespace inherited_exception {
struct Base {
  int b1;
};
struct Base2 {
  int b2;
};
struct Child : public Base, public Base2 {
  int c;
};

static void f1() {
  Child child;
  child.b1 = 10;
  child.b2 = 11;
  child.c = 12;
  throw child;
}

static void f2() {
  Child child;
  child.b1 = 10;
  child.b2 = 11;
  child.c = 12;
  throw static_cast<Base2&>(child);
}

static Child child3;

static void f3() {
  child3.b1 = 10;
  child3.b2 = 11;
  child3.c = 12;
  throw static_cast<Base2*>(&child3);
}
} // namespace inherited_exception

BPF_TEST(CatchClass, InheritedException) {
  using namespace inherited_exception;
  int got = 0;
  try {
    f1();
  } catch (const Child& c) {
    got = 1;
  } catch (const Base& b) {
    got = 2;
  } catch (...) {
    got = 3;
  }
  EXPECT_EQ(got, 1);

  try {
    f1();
  } catch (const Base& c) {
    got = 1;
  } catch (const Child& b) {
    got = 2;
  } catch (...) {
    got = 3;
  }
  EXPECT_EQ(got, 1);

  try {
    f1();
  } catch (const Base2& c) {
    got = 1;
  } catch (const Child& b) {
    got = 2;
  } catch (...) {
    got = 3;
  }
  EXPECT_EQ(got, 1);

  try {
    f2();
  } catch (const Child& c) {
    got = 1;
  } catch (const Base& b) {
    got = 2;
  } catch (const Base2& b) {
    got = 3;
  } catch (...) {
    got = 4;
  }
  EXPECT_EQ(got, 3);

  try {
    f3();
  } catch (const Base* c) {
    got = 1;
  } catch (const Child* b) {
    got = 2;
  } catch (const Base2* c) {
    got = 3;
  } catch (...) {
    got = 4;
  }
  EXPECT_EQ(got, 3);
}

namespace catch_in_noexcept {
struct A {};
static int result;

// Despite being marked as noexcept, this function must be unwound through
// up to its own handler.
static void f1() noexcept {
  try {
    A a;
    throw a;
  } catch (...) {
    result = 1;
    return;
  }
  result = 2;
}
} // namespace catch_in_noexcept

BPF_TEST(CatchClass, CatchInNoexcept) {
  catch_in_noexcept::result = 0;
  catch_in_noexcept::f1();
  EXPECT_EQ(catch_in_noexcept::result, 1);
}

BPF_TEST(CatchClass, Array01) {
  // Can you have a catch clause of array type that catches anything?
  typedef char Array[4];
  Array a = {'H', 'i', '!', 0};
  int got = 0;
  try {
    throw a; // converts to char*
  } catch (Array& b) { // can't catch char*
    got = 1;
  } catch (...) {
    got = 2;
  }
  EXPECT_EQ(got, 2);
}

BPF_TEST(CatchClass, Array02) {
  typedef char Array[4];
  Array a = {'H', 'i', '!', 0};
  int got = 0;
  try {
    throw a; // converts to char*
  } catch (Array b) { // equivalent to char*
    got = 1;
  } catch (...) {
    got = 2;
  }
  EXPECT_EQ(got, 1);
}

namespace catch_function {
static void f() {}
template <bool Noexcept>
static void g() noexcept(Noexcept) {}
template <bool Noexcept>
using FnType = void() noexcept(Noexcept);

template <bool ThrowNoexcept, bool CatchNoexcept>
static int check() {
  try {
    auto* p = g<ThrowNoexcept>;
    throw p;
  } catch (FnType<CatchNoexcept>* p) {
    return (ThrowNoexcept || !CatchNoexcept) && p == &g<ThrowNoexcept> ? 1 : -1;
  } catch (...) {
    return (!ThrowNoexcept && CatchNoexcept) ? 2 : -2;
  }
  return 0;
}
} // namespace catch_function

BPF_TEST(CatchClass, Function01) {
  using namespace catch_function;
  typedef void Function();
  int got = 0;
  try {
    throw f; // converts to void (*)()
  } catch (Function& b) { // can't catch void (*)()
    got = 1;
  } catch (void*) { // can't catch as void*
    got = 2;
  } catch (Function*) {
    got = 3;
  } catch (...) {
    got = 4;
  }
  EXPECT_EQ(got, 3);
}

BPF_TEST(CatchClass, Function02) {
  using namespace catch_function;
  typedef void Function();
  int got = 0;
  try {
    throw f; // converts to void (*)()
  } catch (Function b) { // equivalent to void (*)()
    got = 1;
  } catch (...) {
    got = 2;
  }
  EXPECT_EQ(got, 1);
}

BPF_TEST(CatchClass, Function03) {
  // Can a noexcept function pointer be caught by a non-noexcept catch clause?
  using namespace catch_function;
  EXPECT_EQ((check<false, false>()), 1);
  EXPECT_EQ((check<false, true>()), 2);
  EXPECT_EQ((check<true, false>()), 1);
  EXPECT_EQ((check<true, true>()), 1);
  auto* p = g<true>;
  int got = 0;
  try {
    throw &p;
  } catch (FnType<false>** q) {
    got = 1;
  } catch (FnType<true>** q) {
    got = 2;
  } catch (...) {
    got = 3;
  }
  EXPECT_EQ(got, 2);
}

namespace unwind_01 {
static int failures;
struct A {
  static int count;
  int id_;
  A() : id_(++count) {}
  ~A() { failures += id_ == count-- ? 0 : 1; }
};
int A::count = 0;
struct B {
  static int count;
  int id_;
  B() : id_(++count) {}
  ~B() { failures += id_ == count-- ? 0 : 1; }
};
int B::count = 0;
struct C {
  static int count;
  int id_;
  C() : id_(++count) {}
  ~C() { failures += id_ == count-- ? 0 : 1; }
};
int C::count = 0;

static void f2() {
  C c;
  A a;
  throw 55;
  B b;
}

static void f1() {
  A a;
  B b;
  f2();
  C c;
}
} // namespace unwind_01

BPF_TEST(CatchClass, Unwind01) {
  using namespace unwind_01;
  failures = 0;
  A::count = B::count = C::count = 0;
  int got = 0;
  try {
    f1();
  } catch (int* i) {
    got = 1;
  } catch (long i) {
    got = 2;
  } catch (int i) {
    got = i;
  } catch (...) {
    got = 4;
  }
  EXPECT_EQ(got, 55);
  EXPECT_EQ(failures, 0);
  EXPECT_TRUE(A::count == 0);
  EXPECT_TRUE(B::count == 0);
  EXPECT_TRUE(C::count == 0);
}

namespace uncaught {
static unsigned seen;
static unsigned seenBool;
struct A {
  A(unsigned cnt) : data_(cnt) {}
  ~A() {
    seen = __cxxabiv1::__cxa_uncaught_exceptions() == data_ ? 1 : 2;
    seenBool = __cxxabiv1::__cxa_uncaught_exception() ? 1 : 2;
  }
  unsigned data_;
};
} // namespace uncaught

BPF_TEST(CatchClass, UncaughtExceptions) {
  using namespace uncaught;
  seen = seenBool = 0;
  try {
    A a(1);
    throw 3;
  } catch (int) {
  }
  EXPECT_EQ(seen, 1u);
  EXPECT_EQ(seenBool, 1u);
  EXPECT_EQ(__cxxabiv1::__cxa_uncaught_exceptions(), 0u);
}
