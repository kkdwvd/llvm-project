//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// libc++abi's catch_ptr, catch_ptr_02, catch_pointer_nullptr,
// catch_const_pointer_nullptr, catch_member_pointer_nullptr,
// catch_reference_nullptr, catch_member_data_pointer_01,
// catch_member_function_pointer_01/02 and
// catch_null_pointer_to_object_pr64953, as BPF programs. Thrown pointers
// point into the arena: a pointer the matcher follows, to a vtable for a
// virtual base's offset, has to be one a BPF program can follow, and a
// thrown pointer's value passes through the exception object as a number.
#include <cstdint>
#include <cstddef>

#include <std/bpf/testing/BpfTest.h>

namespace catch_ptr {
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

// Constructed by the test, in the arena: a BPF object runs no constructor
// of a global.
static A* global_a;

static void f1() { throw global_a; }

static void f2() {
  try {
    f1();
    EXPECT_TRUE(false);
  } catch (const A* a) { // can catch A
    EXPECT_TRUE(a->id_ == 5);
    EXPECT_TRUE(static_cast<const C1*>(a)->id_ == 4);
    EXPECT_TRUE(static_cast<const C2*>(a)->id_ == 3);
    EXPECT_TRUE(static_cast<const B*>(a)->id_ == 8);
    throw;
  } catch (const C1*) {
    EXPECT_TRUE(false);
  } catch (const C2*) {
    EXPECT_TRUE(false);
  } catch (const B*) {
    EXPECT_TRUE(false);
  }
}

static void f3() {
  try {
    f2();
    EXPECT_TRUE(false);
  } catch (const B* a) { // can catch B
    EXPECT_TRUE(static_cast<const B*>(a)->id_ == 8);
    throw;
  } catch (const C1* c1) {
    EXPECT_TRUE(false);
  } catch (const C2*) {
    EXPECT_TRUE(false);
  }
}

static void f4() {
  try {
    f3();
    EXPECT_TRUE(false);
  } catch (const C2* c2) { // can catch C2
    EXPECT_TRUE(c2->id_ == 3);
    throw;
  } catch (const B* a) {
    EXPECT_TRUE(false);
  } catch (const C1*) {
    EXPECT_TRUE(false);
  }
}

static void f5() {
  try {
    f4();
    EXPECT_TRUE(false);
  } catch (const C1* c1) { // can catch C1
    EXPECT_TRUE(c1->id_ == 4);
    EXPECT_TRUE(static_cast<const B*>(c1)->id_ == 8);
    throw;
  } catch (const B* a) {
    EXPECT_TRUE(false);
  } catch (const C2*) {
    EXPECT_TRUE(false);
  }
}
} // namespace catch_ptr

BPF_TEST(CatchPointer, Ptr) {
  using namespace catch_ptr;
  global_a = new A(5);
  try {
    f5();
    EXPECT_TRUE(false);
  } catch (...) {
  }
  delete global_a;
  global_a = nullptr;
}

namespace catch_ptr_02 {
struct A {};
static A a;
static const A ca = A();

struct base1 {
  int x;
};
struct base2 {
  int x;
};
struct derived : base1, base2 {};

struct vBase {};
struct vDerived : virtual public vBase {};
} // namespace catch_ptr_02

BPF_TEST(CatchPointer, Ptr02) {
  using namespace catch_ptr_02;
  int got = 0;
  try {
    throw &a;
  } catch (const A*) {
    got = 1;
  } catch (A*) {
    got = 2;
  }
  EXPECT_EQ(got, 1);

  try {
    throw &a;
  } catch (A*) {
    got = 1;
  } catch (const A*) {
    got = 2;
  }
  EXPECT_EQ(got, 1);

  try {
    throw &ca;
  } catch (const A*) {
    got = 1;
  } catch (A*) {
    got = 2;
  }
  EXPECT_EQ(got, 1);

  try {
    throw &ca;
  } catch (A*) {
    got = 1;
  } catch (const A*) {
    got = 2;
  }
  EXPECT_EQ(got, 2);

  // test5
  try {
    throw (derived*)0;
  } catch (base2* p) {
    got = p == 0 ? 1 : -1;
  } catch (...) {
    got = 2;
  }
  EXPECT_EQ(got, 1);

  // test6
  try {
    throw nullptr;
  } catch (base2* p) {
    got = p == nullptr ? 1 : -1;
  } catch (...) {
    got = 2;
  }
  EXPECT_EQ(got, 1);

  // test7
  try {
    throw (derived*)12;
  } catch (base2* p) {
    got = (uintptr_t)p == 12 + sizeof(base1) ? 1 : -1;
  } catch (...) {
    got = 2;
  }
  EXPECT_EQ(got, 1);

  // test8: the object with a virtual base is in the arena.
  vDerived* derived = new vDerived;
  try {
    throw derived;
  } catch (vBase* p) {
    got = p != 0 ? 1 : -1;
  } catch (...) {
    got = 2;
  }
  EXPECT_EQ(got, 1);
  delete derived;

  // test9
  try {
    throw nullptr;
  } catch (vBase* p) {
    got = p == 0 ? 1 : -1;
  } catch (...) {
    got = 2;
  }
  EXPECT_EQ(got, 1);

  // test10
  try {
    throw (vDerived*)0;
  } catch (vBase* p) {
    got = p == 0 ? 1 : -1;
  } catch (...) {
    got = 2;
  }
  EXPECT_EQ(got, 1);
}

namespace catch_pointer_nullptr {
struct A {};

template <class Catch>
static int catch_nullptr_test() {
  try {
    throw nullptr;
  } catch (Catch c) {
    return !c ? 1 : -1;
  } catch (...) {
    return 2;
  }
  return 0;
}
} // namespace catch_pointer_nullptr

BPF_TEST(CatchPointer, PointerNullptr) {
  using namespace catch_pointer_nullptr;
  int got = 0;
  try {
    throw nullptr;
  } catch (int* p) {
    got = !p ? 1 : -1;
  } catch (long*) {
    got = 2;
  }
  EXPECT_EQ(got, 1);

  try {
    throw nullptr;
  } catch (A* p) {
    got = !p ? 1 : -1;
  } catch (int*) {
    got = 2;
  }
  EXPECT_EQ(got, 1);

  EXPECT_EQ(catch_nullptr_test<int*>(), 1);
  EXPECT_EQ(catch_nullptr_test<int**>(), 1);
  EXPECT_EQ(catch_nullptr_test<int A::*>(), 1);
  EXPECT_EQ(catch_nullptr_test<const int A::*>(), 1);
  EXPECT_EQ(catch_nullptr_test<int A::**>(), 1);
}

namespace catch_const_pointer_nullptr {
struct A {};
}

BPF_TEST(CatchPointer, ConstPointerNullptr) {
  using namespace catch_const_pointer_nullptr;
  int got = 0;
  try {
    throw nullptr;
  } catch (A* p) {
    got = !p ? 1 : -1;
  } catch (const A*) {
    got = 2;
  }
  EXPECT_EQ(got, 1);

  try {
    throw nullptr;
  } catch (const A* p) {
    got = !p ? 1 : -1;
  } catch (A*) {
    got = 2;
  }
  EXPECT_EQ(got, 1);

  try {
    throw nullptr;
  } catch (const A* const p) {
    got = !p ? 1 : -1;
  } catch (A*) {
    got = 2;
  }
  EXPECT_EQ(got, 1);

  try {
    throw nullptr;
  } catch (A* p) {
    got = !p ? 1 : -1;
  } catch (const A* const) {
    got = 2;
  }
  EXPECT_EQ(got, 1);

  try {
    throw nullptr;
  } catch (A const* p) {
    got = !p ? 1 : -1;
  } catch (A*) {
    got = 2;
  }
  EXPECT_EQ(got, 1);

  try {
    throw nullptr;
  } catch (A* p) {
    got = !p ? 1 : -1;
  } catch (A const*) {
    got = 2;
  }
  EXPECT_EQ(got, 1);
}

namespace catch_member_pointer_nullptr {
struct A {
  const int i;
  int j;
};
typedef const int A::*md1;
typedef int A::*md2;
} // namespace catch_member_pointer_nullptr

BPF_TEST(CatchPointer, MemberPointerNullptr) {
  using namespace catch_member_pointer_nullptr;
  int got = 0;
  try {
    throw nullptr;
  } catch (md2 p) {
    got = !p ? 1 : -1;
  } catch (md1) {
    got = 2;
  }
  EXPECT_EQ(got, 1);

  try {
    throw nullptr;
  } catch (md1 p) {
    got = !p ? 1 : -1;
  } catch (md2) {
    got = 2;
  }
  EXPECT_EQ(got, 1);
}

namespace catch_reference_nullptr {
template <typename T, bool CanCatchNullptr>
static int catch_nullptr_test() {
  try {
    throw nullptr;
  } catch (T& p) {
    return CanCatchNullptr && !static_cast<bool>(p) ? 1 : -1;
  } catch (...) {
    return !CanCatchNullptr ? 2 : -2;
  }
  return 0;
}
} // namespace catch_reference_nullptr

BPF_TEST(CatchPointer, ReferenceNullptr) {
  using namespace catch_reference_nullptr;
  // A reference to nullptr_t can catch nullptr.
  EXPECT_EQ((catch_nullptr_test<std::nullptr_t, true>()), 1);
  EXPECT_EQ((catch_nullptr_test<const std::nullptr_t, true>()), 1);
  EXPECT_EQ((catch_nullptr_test<volatile std::nullptr_t, true>()), 1);
  EXPECT_EQ((catch_nullptr_test<const volatile std::nullptr_t, true>()), 1);
}

namespace catch_member_data_pointer {
struct A {
  A() : i(0), j(0) {}
  const int i;
  int j;
};
typedef const int A::*md1;
typedef int A::*md2;

struct B : public A {
  B() : k(0), l(0) {}
  const int k;
  int l;
};
typedef const int B::*der1;
typedef int B::*der2;
} // namespace catch_member_data_pointer

BPF_TEST(CatchPointer, MemberDataPointer01) {
  using namespace catch_member_data_pointer;
  int got = 0;
  // test1
  try {
    throw &A::i;
  } catch (md2) {
    got = 1;
  } catch (md1) {
    got = 2;
  }
  EXPECT_EQ(got, 2);

  // test2: cv qualified conversions are allowed.
  try {
    throw &A::j;
  } catch (md2) {
    got = 1;
  } catch (...) {
    got = 2;
  }
  EXPECT_EQ(got, 1);
  try {
    throw &A::j;
  } catch (md1) {
    got = 1;
  } catch (...) {
    got = 2;
  }
  EXPECT_EQ(got, 1);

  // test3: Base -> Derived conversions are NOT allowed.
  try {
    throw &A::i;
  } catch (md2) {
    got = 1;
  } catch (der2) {
    got = 2;
  } catch (der1) {
    got = 3;
  } catch (md1) {
    got = 4;
  }
  EXPECT_EQ(got, 4);

  // test4: not with different cv qualifiers either.
  try {
    throw &A::j;
  } catch (der2) {
    got = 1;
  } catch (der1) {
    got = 2;
  } catch (md2) {
    got = 3;
  } catch (...) {
    got = 4;
  }
  EXPECT_EQ(got, 3);

  // test5: no Derived -> Base conversions.
  try {
    throw &B::k;
  } catch (md1) {
    got = 1;
  } catch (md2) {
    got = 2;
  } catch (der1) {
    got = 3;
  }
  EXPECT_EQ(got, 3);
  try {
    throw &B::l;
  } catch (md1) {
    got = 1;
  } catch (md2) {
    got = 2;
  } catch (der2) {
    got = 3;
  }
  EXPECT_EQ(got, 3);
}

namespace catch_member_function_pointer {
struct A {
  void foo() {}
  void bar() const {}
};
typedef void (A::*mf1)();
typedef void (A::*mf2)() const;

struct B : public A {};
typedef void (B::*dmf1)();
typedef void (B::*dmf2)() const;

struct X {
  template <bool Noexcept>
  void f() noexcept(Noexcept) {}
};
template <bool Noexcept>
using FnType = void (X::*)() noexcept(Noexcept);

template <bool ThrowNoexcept, bool CatchNoexcept>
static int check() {
  try {
    auto p = &X::f<ThrowNoexcept>;
    throw p;
  } catch (FnType<CatchNoexcept> p) {
    return (ThrowNoexcept || !CatchNoexcept) && p == &X::f<ThrowNoexcept> ? 1 : -1;
  } catch (...) {
    return (!ThrowNoexcept && CatchNoexcept) ? 2 : -2;
  }
  return 0;
}
} // namespace catch_member_function_pointer

BPF_TEST(CatchPointer, MemberFunctionPointer01) {
  using namespace catch_member_function_pointer;
  int got = 0;
  try {
    throw &A::foo;
  } catch (mf2) {
    got = 1;
  } catch (mf1) {
    got = 2;
  }
  EXPECT_EQ(got, 2);

  try {
    throw &A::bar;
  } catch (mf1) {
    got = 1;
  } catch (mf2) {
    got = 2;
  }
  EXPECT_EQ(got, 2);

  // test_derived
  try {
    throw (mf1)0;
  } catch (dmf2) {
    got = 1;
  } catch (dmf1) {
    got = 2;
  } catch (mf1) {
    got = 3;
  }
  EXPECT_EQ(got, 3);
  try {
    throw (mf2)0;
  } catch (dmf1) {
    got = 1;
  } catch (dmf2) {
    got = 2;
  } catch (mf2) {
    got = 3;
  }
  EXPECT_EQ(got, 3);
  try {
    throw (dmf1)0;
  } catch (mf2) {
    got = 1;
  } catch (mf1) {
    got = 2;
  } catch (...) {
    got = 3;
  }
  EXPECT_EQ(got, 3);
  try {
    throw (dmf2)0;
  } catch (mf2) {
    got = 1;
  } catch (mf1) {
    got = 2;
  } catch (...) {
    got = 3;
  }
  EXPECT_EQ(got, 3);

  // test_void
  try {
    throw &A::foo;
  } catch (void*) {
    got = 1;
  } catch (...) {
    got = 2;
  }
  EXPECT_EQ(got, 2);
}

BPF_TEST(CatchPointer, MemberFunctionPointer02) {
  // Can a noexcept member function pointer be caught by a non-noexcept
  // catch clause?
  using namespace catch_member_function_pointer;
  EXPECT_EQ((check<false, false>()), 1);
  EXPECT_EQ((check<false, true>()), 2);
  EXPECT_EQ((check<true, false>()), 1);
  EXPECT_EQ((check<true, true>()), 1);
  FnType<true> p = &X::f<true>;
  int got = 0;
  try {
    throw &p;
  } catch (FnType<false>* q) {
    got = 1;
  } catch (FnType<true>* q) {
    got = 2;
  } catch (...) {
    got = 3;
  }
  EXPECT_EQ(got, 2);
}

namespace pr64953 {
struct Base {
  int b;
};
struct Base2 {
  int b;
};
struct Derived1 : Base {
  int b;
};
struct Derived2 : Base {
  int b;
};
struct Derived3 : Base2 {
  int b;
};
struct Private : private Base {
  int b;
};
struct Protected : protected Base {
  int b;
};
struct Virtual1 : virtual Base {
  int b;
};
struct Virtual2 : virtual Base {
  int b;
};

struct Ambiguous1 : Derived1, Derived2 {
  int b;
};
struct Ambiguous2 : Derived1, Private {
  int b;
};
struct Ambiguous3 : Derived1, Protected {
  int b;
};

struct NoPublic1 : Private, Base2 {
  int b;
};
struct NoPublic2 : Protected, Base2 {
  int b;
};

struct Catchable1 : Derived3, Derived1 {
  int b;
};
struct Catchable2 : Virtual1, Virtual2 {
  int b;
};
struct Catchable3 : virtual Base, Virtual2 {
  int b;
};

// Check that, when we have a null pointer-to-object that we catch a nullptr.
template <typename T, typename E>
static int assert_catches() {
  try {
    throw static_cast<E>(0);
  } catch (T t) {
    return t == nullptr ? 1 : -1;
  } catch (...) {
    return 2;
  }
  return 0;
}

template <typename T, typename E>
static int assert_cannot_catch() {
  try {
    throw static_cast<E>(0);
  } catch (T t) {
    return 1;
  } catch (...) {
    return 2;
  }
  return 0;
}

// Check that when we have a pointer-to-actual-object we, in fact, get the
// adjusted pointer to the base class.
template <typename T, typename O>
static int assert_catches_bp() {
  O* o = new (O);
  int result = 0;
  try {
    throw o;
  } catch (T t) {
    result = t == static_cast<T>(o) ? 1 : -1;
  } catch (...) {
    result = 2;
  }
  delete o;
  return result;
}
} // namespace pr64953

namespace anchors {
// One base type, X, at offset zero of two paths from D, one through D's
// virtual V and one through D's first base B, which keeps its own X. Without
// an object, as for a thrown null pointer, the walk tells two subobjects at
// the same offset apart by the innermost virtual base on each path alone:
// V on one, none on the other. X is ambiguous; V, A and B are not.
struct X {};
struct V : X {};
struct A : virtual V {};
struct B : X, virtual V {};
struct D : B, A {};
} // namespace anchors

BPF_TEST(CatchPointer, NullPointerToABaseByAVirtualAndADirectPath) {
  using namespace pr64953;
  using namespace anchors;
  EXPECT_EQ((assert_cannot_catch<X*, D*>()), 2);
  EXPECT_EQ((assert_catches<V*, D*>()), 1);
  EXPECT_EQ((assert_catches<A*, D*>()), 1);
  EXPECT_EQ((assert_catches<B*, D*>()), 1);
  EXPECT_EQ((assert_catches<const void*, D*>()), 1);
  // With an object, the two X subobjects are at different addresses and
  // the walk reads V's offset from D's vtable: still ambiguous, and the
  // unambiguous bases adjust to their subobjects.
  EXPECT_EQ((assert_catches_bp<V*, D>()), 1);
  EXPECT_EQ((assert_catches_bp<B*, D>()), 1);
  D* d = new D;
  int result = 0;
  try {
    throw d;
  } catch (X* x) {
    result = x == nullptr ? -1 : 1;
  } catch (...) {
    result = 2;
  }
  delete d;
  EXPECT_EQ(result, 2);
}

BPF_TEST(CatchPointer, NullPointerToObject) {
  using namespace pr64953;
  EXPECT_EQ((assert_catches<Base*, Catchable1*>()), 1);
  EXPECT_EQ((assert_catches<Base*, Catchable2*>()), 1);
  EXPECT_EQ((assert_catches<Base*, Catchable3*>()), 1);

  EXPECT_EQ((assert_cannot_catch<Base*, Ambiguous1*>()), 2);
  EXPECT_EQ((assert_cannot_catch<Base*, Ambiguous2*>()), 2);
  EXPECT_EQ((assert_cannot_catch<Base*, Ambiguous3*>()), 2);
  EXPECT_EQ((assert_cannot_catch<Base*, NoPublic1*>()), 2);
  EXPECT_EQ((assert_cannot_catch<Base*, NoPublic2*>()), 2);

  EXPECT_EQ((assert_catches_bp<Base*, Catchable1>()), 1);
  EXPECT_EQ((assert_catches_bp<Base*, Catchable2>()), 1);
  EXPECT_EQ((assert_catches_bp<Base*, Catchable3>()), 1);
}
