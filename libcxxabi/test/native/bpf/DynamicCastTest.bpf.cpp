//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// libc++abi's dynamic_cast.pass (t1..t5), cxa_bad_cast, cxa_bad_typeid,
// test_aux_runtime and test_aux_runtime_op_array_new, as BPF programs. The
// polymorphic objects live in the arena, allocated by the test: a BPF object
// runs no constructor of a global, and the run-time check takes the object's
// address as a number, which a stack address cannot be.
#include <cstdint>
#include <cxxabi.h>
#include <exception>
#include <new>
#include <typeinfo>

#include <std/bpf/testing/BpfTest.h>
#include <std/bpf/verifier/Loop.h>

#pragma clang diagnostic ignored "-Winaccessible-base"

typedef char Pad1[43981];
typedef char Pad2[34981];
typedef char Pad3[93481];
typedef char Pad4[13489];
typedef char Pad5[81349];
typedef char Pad6[34819];
typedef char Pad7[3489];

namespace t1 {
// PR33425
struct C3 {
  virtual ~C3() {}
  Pad1 _;
};
struct C5 : protected virtual C3 {
  Pad2 _;
};
struct C6 : virtual C5 {
  Pad3 _;
};
struct C7 : virtual C3 {
  Pad4 _;
};
struct C9 : C6, C7 {
  Pad5 _;
};
} // namespace t1

BPF_TEST(DynamicCast, T1) {
  using namespace t1;
  C9* c9 = new C9;
  C3* c3 = c9;
  EXPECT_TRUE(dynamic_cast<C3*>(c3) == static_cast<C3*>(c9));
  EXPECT_TRUE(dynamic_cast<C5*>(c3) == static_cast<C5*>(c9));
  EXPECT_TRUE(dynamic_cast<C6*>(c3) == static_cast<C6*>(c9));
  EXPECT_TRUE(dynamic_cast<C7*>(c3) == static_cast<C7*>(c9));
  EXPECT_TRUE(dynamic_cast<C9*>(c3) == static_cast<C9*>(c9));
  delete c9;
}

namespace t2 {
// PR33425
struct Src {
  virtual ~Src() {}
  Pad1 _;
};
struct Mask : protected virtual Src {
  Pad2 _;
};
struct Dest : Mask {
  Pad3 _;
};
struct Root : Dest, virtual Src {
  Pad4 _;
};
} // namespace t2

BPF_TEST(DynamicCast, T2) {
  using namespace t2;
  Root* root = new Root;
  Src* src = root;
  EXPECT_TRUE(dynamic_cast<Src*>(src) == static_cast<Src*>(root));
  EXPECT_TRUE(dynamic_cast<Mask*>(src) == static_cast<Mask*>(root));
  EXPECT_TRUE(dynamic_cast<Dest*>(src) == static_cast<Dest*>(root));
  EXPECT_TRUE(dynamic_cast<Root*>(src) == static_cast<Root*>(root));
  delete root;
}

namespace t3 {
// PR33487
struct Class1 {
  virtual ~Class1() {}
  Pad1 _;
};
struct Shared : virtual Class1 {
  Pad2 _;
};
struct Class6 : virtual Shared {
  Pad3 _;
};
struct Left : Class6 {
  Pad4 _;
};
struct Right : Class6 {
  Pad5 _;
};
struct Main : Left, Right {
  Pad6 _;
};
} // namespace t3

BPF_TEST(DynamicCast, T3) {
  using namespace t3;
  Main* m = new Main;
  Class1* c1 = m;
  EXPECT_TRUE(dynamic_cast<Class1*>(c1) == static_cast<Class1*>(m));
  EXPECT_TRUE(dynamic_cast<Shared*>(c1) == static_cast<Shared*>(m));
  EXPECT_TRUE(dynamic_cast<Class6*>(c1) == 0);
  EXPECT_TRUE(dynamic_cast<Left*>(c1) == static_cast<Left*>(m));
  EXPECT_TRUE(dynamic_cast<Right*>(c1) == static_cast<Right*>(m));
  EXPECT_TRUE(dynamic_cast<Main*>(c1) == static_cast<Main*>(m));
  delete m;
}

namespace t4 {
// PR33439
struct C2 {
  virtual ~C2() {}
  Pad1 _;
};
struct C3 {
  virtual ~C3() {}
  Pad2 _;
};
struct C4 : C3 {
  Pad3 _;
};
struct C8 : C2, virtual C4 {
  Pad4 _;
};
struct C9 : C4, C8 {
  Pad5 _;
};
} // namespace t4

BPF_TEST(DynamicCast, T4) {
  using namespace t4;
  C9* c9 = new C9;
  C2* c2 = c9;
  EXPECT_TRUE(dynamic_cast<C2*>(c2) == static_cast<C2*>(c9));
  EXPECT_TRUE(dynamic_cast<C3*>(c2) == 0);
  EXPECT_TRUE(dynamic_cast<C4*>(c2) == 0);
  EXPECT_TRUE(dynamic_cast<C8*>(c2) == static_cast<C8*>(c9));
  EXPECT_TRUE(dynamic_cast<C9*>(c2) == static_cast<C9*>(c9));
  delete c9;
}

namespace t5 {
// PR33439
struct Dummy {
  virtual ~Dummy() {}
  Pad1 _;
};
struct Src {
  virtual ~Src() {}
  Pad2 _;
};
struct Dest : Dummy {
  Pad3 _;
};
struct A1 : Dest {
  Pad4 _;
};
struct A2 : Dest {
  Pad5 _;
};
struct Root : Src, A1, A2 {
  Pad6 _;
};
} // namespace t5

BPF_TEST(DynamicCast, T5) {
  using namespace t5;
  Root* root = new Root;
  Src* src = root;
  EXPECT_TRUE(dynamic_cast<Dummy*>(src) == 0);
  EXPECT_TRUE(dynamic_cast<Src*>(src) == static_cast<Src*>(root));
  EXPECT_TRUE(dynamic_cast<Dest*>(src) == 0);
  EXPECT_TRUE(dynamic_cast<A1*>(src) == static_cast<A1*>(root));
  EXPECT_TRUE(dynamic_cast<A2*>(src) == static_cast<A2*>(root));
  delete root;
}

namespace bad_cast {
class Base {
  virtual void foo() {}
};
class Derived : public Base {};

[[gnu::noinline]] static Derived& test_bad_cast(Base& b) { return dynamic_cast<Derived&>(b); }
} // namespace bad_cast

BPF_TEST(DynamicCast, CxaBadCast) {
  using namespace bad_cast;
  Base* gB = new Base;
  int got = 0;
  try {
    Derived& d = test_bad_cast(*gB);
    got = 1;
    ((void)d);
  } catch (std::bad_cast const&) {
    got = 2;
  } catch (...) {
    got = 3;
  }
  EXPECT_EQ(got, 2);
  delete gB;
}

namespace bad_typeid {
class Base {
  virtual void foo() {}
};
class Derived : public Base {};

[[gnu::noinline]] static const char* test_bad_typeid(Derived* p) { return typeid(*p).name(); }
} // namespace bad_typeid

BPF_TEST(DynamicCast, CxaBadTypeid) {
  using namespace bad_typeid;
  int got = 0;
  try {
    test_bad_typeid(nullptr);
    got = 1;
  } catch (std::bad_typeid const&) {
    got = 2;
  } catch (...) {
    got = 3;
  }
  EXPECT_EQ(got, 2);
}

namespace aux_runtime {
class A {
  virtual void f() {}
};
class B {
  virtual void g() {}
};
class D : public virtual A, private B {};

//  Test taken from 5.2.8.2: typeid of a null polymorphic pointer throws
//  std::bad_typeid.
static bool bad_typeid_test() {
  B* bp = nullptr;
  try {
    bool b = typeid(*bp) == typeid(A);
    ((void)b);
  } catch (const std::bad_typeid&) {
    return true;
  }
  return false;
}

//  A failed cast to reference type throws std::bad_cast (18.7.2).
static bool bad_cast_test() {
  D* d = new D;
  B* bp = (B*)d; // cast needed to break protection
  bool caught = false;
  try {
    D& dr = dynamic_cast<D&>(*bp);
    ((void)dr);
  } catch (const std::bad_cast&) {
    caught = true;
  }
  delete d;
  return caught;
}
} // namespace aux_runtime

BPF_TEST(DynamicCast, AuxRuntime) {
  EXPECT_TRUE(aux_runtime::bad_typeid_test());
  EXPECT_TRUE(aux_runtime::bad_cast_test());
}

BPF_TEST(DynamicCast, AuxRuntimeOpArrayNew) {
  //  If the expression passed to operator new[] would result in an overflow,
  //  a std::bad_array_new_length exception is thrown instead (5.3.4p7).
  bool caught = false;
  try {
    __cxxabiv1::__cxa_throw_bad_array_new_length();
  } catch (const std::bad_array_new_length& banl) {
    caught = true;
  }
  EXPECT_TRUE(caught);
}
