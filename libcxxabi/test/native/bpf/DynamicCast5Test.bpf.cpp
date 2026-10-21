//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// libc++abi's dynamic_cast5.pass.cpp as BPF programs, one per namespace,
// converted mechanically: assert() is EXPECT_TRUE(), and every object is
// allocated in the arena (`A1& a1 = *new A1;`), since a BPF program cannot
// construct an object with a virtual base on its stack and the run-time
// check takes the object's address as a number. The hierarchies, casts and
// expectations are upstream's.
#include <std/bpf/testing/BpfTest.h>

// This test explicitly tests dynamic cast with types that have inaccessible
// bases.
#if defined(__clang__)
#   pragma clang diagnostic ignored "-Winaccessible-base"
#elif defined(__GNUC__)
#   pragma GCC diagnostic ignored "-Winaccessible-base"
#endif

namespace t1
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
{
    char _[34981];
    virtual ~A2() {}

    A2* getA2() {return this;}
};

struct A3
    : public virtual A1,
      private A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return A1::getA1();}
    A2* getA2() {return A2::getA2();}
    A3* getA3() {return this;}
};

struct A4
    : public A3,
      public A2
{
    char _[13489];
    virtual ~A4() {}

    t1::A1* getA1() {return A3::getA1();}
    A2* getA2() {return A3::getA2();}
    A3* getA3() {return A3::getA3();}
    A4* getA4() {return this;}
};

struct A5
    : public A4,
      public A3
{
    char _[13489];
    virtual ~A5() {}

    t1::A1* getA1() {return A4::getA1();}
    A2* getA2() {return A4::getA2();}
    A3* getA3() {return A4::getA3();}
    A4* getA4() {return A4::getA4();}
    A5* getA5() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    A4& a4 = *new A4;
    A5& a5 = *new A5;

    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA3()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA1()) == a4.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA3()) == a4.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA4()) == a4.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA1()) == a5.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA3()) == a5.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA4()) == a5.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA5()) == a5.getA1());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
//    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == 0);  // cast to private base
    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA2()) == a4.getA2());
//    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA3()) == 0);  // cast to private base
//    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA4()) == 0);  // cast to ambiguous base
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA2()) == a5.getA2());
//    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA3()) == 0);  // cast to private base
//    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA4()) == 0);  // cast to ambiguous base
//    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA5()) == 0);  // cast to ambiguous base

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA1()) == a4.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA3()) == a4.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA4()) == a4.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA3()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA4()) == a5.getA3());
//    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA5()) == 0);  // cast to ambiguous base

    EXPECT_TRUE(dynamic_cast<A4*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a3.getA3()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA1()) == a4.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA3()) == a4.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA4()) == a4.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA1()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA3()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA4()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA5()) == a5.getA4());

    EXPECT_TRUE(dynamic_cast<A5*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a3.getA3()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA3()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA4()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA1()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA3()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA4()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA5()) == a5.getA5());
}

}  // t1

namespace t2
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
{
    char _[34981];
    virtual ~A2() {}

    A2* getA2() {return this;}
};

struct A3
    : public virtual A1,
      public A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return A1::getA1();}
    A2* getA2() {return A2::getA2();}
    A3* getA3() {return this;}
};

struct A4
    : public A3,
      public A2
{
    char _[13489];
    virtual ~A4() {}

    t2::A1* getA1() {return A3::getA1();}
    A2* getA2() {return A3::getA2();}
    A3* getA3() {return A3::getA3();}
    A4* getA4() {return this;}
};

struct A5
    : public A4,
      public A3
{
    char _[13489];
    virtual ~A5() {}

    t2::A1* getA1() {return A4::getA1();}
    A2* getA2() {return A4::getA2();}
    A3* getA3() {return A4::getA3();}
    A4* getA4() {return A4::getA4();}
    A5* getA5() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    A4& a4 = *new A4;
    A5& a5 = *new A5;

    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA3()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA1()) == a4.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA2()) == a4.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA3()) == a4.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA4()) == a4.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA1()) == a5.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA2()) == a5.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA3()) == a5.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA4()) == a5.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA5()) == a5.getA1());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA2()) == a4.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA3()) == a4.getA2());
//    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA4()) == 0);  // cast to ambiguous base
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA2()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA3()) == a5.getA2());
//    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA4()) == 0);  // cast to ambiguous base
//    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA5()) == 0);  // cast to ambiguous base

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA1()) == a4.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA2()) == a4.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA3()) == a4.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA4()) == a4.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA2()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA3()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA4()) == a5.getA3());
//    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA5()) == 0);  // cast to ambiguous base

    EXPECT_TRUE(dynamic_cast<A4*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a3.getA3()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA1()) == a4.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA2()) == a4.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA3()) == a4.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA4()) == a4.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA1()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA2()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA3()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA4()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA5()) == a5.getA4());

    EXPECT_TRUE(dynamic_cast<A5*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a3.getA3()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA3()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA4()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA1()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA2()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA3()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA4()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA5()) == a5.getA5());
}

}  // t2

namespace t3
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
{
    char _[34981];
    virtual ~A2() {}

    A2* getA2() {return this;}
};

struct A3
    : public A1,
      public virtual A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return A1::getA1();}
    A2* getA2() {return A2::getA2();}
    A3* getA3() {return this;}
};

struct A4
    : public A1,
      public virtual A2
{
    char _[13489];
    virtual ~A4() {}

    A1* getA1() {return A1::getA1();}
    A2* getA2() {return A2::getA2();}
    A4* getA4() {return this;}
};

struct A5
    : public A3,
      public A4
{
    char _[41389];
    virtual ~A5() {}

    A1* getA14() {return A4::getA1();}
    A1* getA13() {return A3::getA1();}
    A2* getA2() {return A4::getA2();}
    A3* getA3() {return A3::getA3();}
    A4* getA4() {return A4::getA4();}
    A5* getA5() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    A4& a4 = *new A4;
    A5& a5 = *new A5;

    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA3()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA1()) == a4.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA2()) == a4.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA4()) == a4.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA14()) == a5.getA14());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA13()) == a5.getA13());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA3()) == a5.getA13());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA4()) == a5.getA14());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA1()) == a4.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA2()) == a4.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA4()) == a4.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA14()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA13()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA2()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA3()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA4()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA5()) == a5.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA4()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA14()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA13()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA2()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA3()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA4()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA5()) == a5.getA3());

    EXPECT_TRUE(dynamic_cast<A4*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a3.getA3()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA1()) == a4.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA2()) == a4.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA4()) == a4.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA14()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA13()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA2()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA3()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA4()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA5()) == a5.getA4());

    EXPECT_TRUE(dynamic_cast<A5*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a3.getA3()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA4()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA14()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA13()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA2()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA3()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA4()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA5()) == a5.getA5());
}

}  // t3

namespace t4
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
{
    char _[34981];
    virtual ~A2() {}

    A2* getA2() {return this;}
};

struct A3
    : protected A1,
      public virtual A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return A1::getA1();}
    A2* getA2() {return A2::getA2();}
    A3* getA3() {return this;}
};

struct A4
    : public A1,
      public virtual A2
{
    char _[13489];
    virtual ~A4() {}

    A1* getA1() {return A1::getA1();}
    A2* getA2() {return A2::getA2();}
    A4* getA4() {return this;}
};

struct A5
    : public A3,
      public A4
{
    char _[41389];
    virtual ~A5() {}

    A1* getA14() {return A4::getA1();}
    A1* getA13() {return A3::getA1();}
    A2* getA2() {return A4::getA2();}
    A3* getA3() {return A3::getA3();}
    A4* getA4() {return A4::getA4();}
    A5* getA5() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    A4& a4 = *new A4;
    A5& a5 = *new A5;

    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == 0);
//    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA3()) == a3.getA1());  // cast to protected base
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA1()) == a4.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA2()) == a4.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA4()) == a4.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA14()) == a5.getA14());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA13()) == a5.getA13());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA2()) == 0);
//    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA3()) == a5.getA13());  // cast to protected base
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA4()) == a5.getA14());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA1()) == a4.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA2()) == a4.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA4()) == a4.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA14()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA13()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA2()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA3()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA4()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA5()) == a5.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA4()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA14()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA13()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA2()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA3()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA4()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA5()) == a5.getA3());

    EXPECT_TRUE(dynamic_cast<A4*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a3.getA3()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA1()) == a4.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA2()) == a4.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA4()) == a4.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA14()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA13()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA2()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA3()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA4()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA5()) == a5.getA4());

    EXPECT_TRUE(dynamic_cast<A5*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a3.getA3()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA4()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA14()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA13()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA2()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA3()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA4()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA5()) == a5.getA5());
}

}  // t4

namespace t5
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
{
    char _[34981];
    virtual ~A2() {}

    A2* getA2() {return this;}
};

struct A3
    : public A1,
      protected virtual A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return A1::getA1();}
    A2* getA2() {return A2::getA2();}
    A3* getA3() {return this;}
};

struct A4
    : public A1,
      public virtual A2
{
    char _[13489];
    virtual ~A4() {}

    A1* getA1() {return A1::getA1();}
    A2* getA2() {return A2::getA2();}
    A4* getA4() {return this;}
};

struct A5
    : public A3,
      public A4
{
    char _[41389];
    virtual ~A5() {}

    A1* getA14() {return A4::getA1();}
    A1* getA13() {return A3::getA1();}
    A2* getA2() {return A4::getA2();}
    A3* getA3() {return A3::getA3();}
    A4* getA4() {return A4::getA4();}
    A5* getA5() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    A4& a4 = *new A4;
    A5& a5 = *new A5;

    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA3()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA1()) == a4.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA2()) == a4.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA4()) == a4.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA14()) == a5.getA14());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA13()) == a5.getA13());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA3()) == a5.getA13());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA4()) == a5.getA14());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
//    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());  // cast to protected base
    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA1()) == a4.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA2()) == a4.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA4()) == a4.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA14()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA13()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA2()) == a5.getA2());
//    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA3()) == a5.getA2());  // cast to protected base
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA4()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA5()) == a5.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA4()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA14()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA13()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA2()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA3()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA4()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA5()) == a5.getA3());

    EXPECT_TRUE(dynamic_cast<A4*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a3.getA3()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA1()) == a4.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA2()) == a4.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA4()) == a4.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA14()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA13()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA2()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA3()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA4()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA5()) == a5.getA4());

    EXPECT_TRUE(dynamic_cast<A5*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a3.getA3()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA4()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA14()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA13()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA2()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA3()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA4()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA5()) == a5.getA5());
}

}  // t5

namespace t6
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
{
    char _[34981];
    virtual ~A2() {}

    A2* getA2() {return this;}
};

struct A3
    : public A1,
      public virtual A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return A1::getA1();}
    A2* getA2() {return A2::getA2();}
    A3* getA3() {return this;}
};

struct A4
    : protected A1,
      public virtual A2
{
    char _[13489];
    virtual ~A4() {}

    A1* getA1() {return A1::getA1();}
    A2* getA2() {return A2::getA2();}
    A4* getA4() {return this;}
};

struct A5
    : public A3,
      public A4
{
    char _[41389];
    virtual ~A5() {}

    A1* getA14() {return A4::getA1();}
    A1* getA13() {return A3::getA1();}
    A2* getA2() {return A4::getA2();}
    A3* getA3() {return A3::getA3();}
    A4* getA4() {return A4::getA4();}
    A5* getA5() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    A4& a4 = *new A4;
    A5& a5 = *new A5;

    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA3()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA1()) == a4.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA2()) == 0);
//    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA4()) == a4.getA1());  // cast to protected base
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA14()) == a5.getA14());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA13()) == a5.getA13());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA3()) == a5.getA13());
//    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA4()) == a5.getA14());  // cast to protected base

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA2()) == a4.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA4()) == a4.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA14()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA13()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA2()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA3()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA4()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA5()) == a5.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA4()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA14()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA13()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA2()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA3()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA4()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA5()) == a5.getA3());

    EXPECT_TRUE(dynamic_cast<A4*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a3.getA3()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA2()) == a4.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA4()) == a4.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA14()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA13()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA2()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA3()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA4()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA5()) == a5.getA4());

    EXPECT_TRUE(dynamic_cast<A5*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a3.getA3()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA4()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA14()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA13()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA2()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA3()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA4()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA5()) == a5.getA5());
}

}  // t6

namespace t7
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
{
    char _[34981];
    virtual ~A2() {}

    A2* getA2() {return this;}
};

struct A3
    : public A1,
      public virtual A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return A1::getA1();}
    A2* getA2() {return A2::getA2();}
    A3* getA3() {return this;}
};

struct A4
    : public A1,
      protected virtual A2
{
    char _[13489];
    virtual ~A4() {}

    A1* getA1() {return A1::getA1();}
    A2* getA2() {return A2::getA2();}
    A4* getA4() {return this;}
};

struct A5
    : public A3,
      public A4
{
    char _[41389];
    virtual ~A5() {}

    A1* getA14() {return A4::getA1();}
    A1* getA13() {return A3::getA1();}
    A2* getA2() {return A4::getA2();}
    A3* getA3() {return A3::getA3();}
    A4* getA4() {return A4::getA4();}
    A5* getA5() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    A4& a4 = *new A4;
    A5& a5 = *new A5;

    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA3()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA1()) == a4.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA4()) == a4.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA14()) == a5.getA14());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA13()) == a5.getA13());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA3()) == a5.getA13());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA4()) == a5.getA14());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA2()) == a4.getA2());
//    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA4()) == a4.getA2());  // cast to protected base
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA14()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA13()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA2()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA3()) == a5.getA2());
//    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA4()) == a5.getA2());  // cast to protected base
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA5()) == a5.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA4()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA14()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA13()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA2()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA3()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA4()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA5()) == a5.getA3());

    EXPECT_TRUE(dynamic_cast<A4*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a3.getA3()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA1()) == a4.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA4()) == a4.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA14()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA13()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA2()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA3()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA4()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA5()) == a5.getA4());

    EXPECT_TRUE(dynamic_cast<A5*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a3.getA3()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA4()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA14()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA13()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA2()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA3()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA4()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA5()) == a5.getA5());
}

}  // t7

namespace t8
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
{
    char _[34981];
    virtual ~A2() {}

    A2* getA2() {return this;}
};

struct A3
    : public A1,
      public virtual A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return A1::getA1();}
    A2* getA2() {return A2::getA2();}
    A3* getA3() {return this;}
};

struct A4
    : public A1,
      public virtual A2
{
    char _[13489];
    virtual ~A4() {}

    A1* getA1() {return A1::getA1();}
    A2* getA2() {return A2::getA2();}
    A4* getA4() {return this;}
};

struct A5
    : protected A3,
      public A4
{
    char _[41389];
    virtual ~A5() {}

    A1* getA14() {return A4::getA1();}
    A1* getA13() {return A3::getA1();}
    A2* getA2() {return A4::getA2();}
    A3* getA3() {return A3::getA3();}
    A4* getA4() {return A4::getA4();}
    A5* getA5() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    A4& a4 = *new A4;
    A5& a5 = *new A5;

    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA3()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA1()) == a4.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA2()) == a4.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA4()) == a4.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA14()) == a5.getA14());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA13()) == a5.getA13());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA3()) == a5.getA13());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA4()) == a5.getA14());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA1()) == a4.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA2()) == a4.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA4()) == a4.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA14()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA13()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA2()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA3()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA4()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA5()) == a5.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA4()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA14()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA13()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA2()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA3()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA4()) == 0);
//    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA5()) == a5.getA3());  // cast to protected base

    EXPECT_TRUE(dynamic_cast<A4*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a3.getA3()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA1()) == a4.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA2()) == a4.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA4()) == a4.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA14()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA13()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA2()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA3()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA4()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA5()) == a5.getA4());

    EXPECT_TRUE(dynamic_cast<A5*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a3.getA3()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA4()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA14()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA13()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA2()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA3()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA4()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA5()) == a5.getA5());
}

}  // t8

namespace t9
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
{
    char _[34981];
    virtual ~A2() {}

    A2* getA2() {return this;}
};

struct A3
    : public A1,
      public virtual A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return A1::getA1();}
    A2* getA2() {return A2::getA2();}
    A3* getA3() {return this;}
};

struct A4
    : public A1,
      public virtual A2
{
    char _[13489];
    virtual ~A4() {}

    A1* getA1() {return A1::getA1();}
    A2* getA2() {return A2::getA2();}
    A4* getA4() {return this;}
};

struct A5
    : public A3,
      protected A4
{
    char _[41389];
    virtual ~A5() {}

    A1* getA14() {return A4::getA1();}
    A1* getA13() {return A3::getA1();}
    A2* getA2() {return A4::getA2();}
    A3* getA3() {return A3::getA3();}
    A4* getA4() {return A4::getA4();}
    A5* getA5() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    A4& a4 = *new A4;
    A5& a5 = *new A5;

    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA3()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA1()) == a4.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA2()) == a4.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a4.getA4()) == a4.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA14()) == a5.getA14());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA13()) == a5.getA13());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA3()) == a5.getA13());
    EXPECT_TRUE(dynamic_cast<A1*>(a5.getA4()) == a5.getA14());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA1()) == a4.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA2()) == a4.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a4.getA4()) == a4.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA14()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA13()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA2()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA3()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA4()) == a5.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a5.getA5()) == a5.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a4.getA4()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA14()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA13()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA2()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA3()) == a5.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA4()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a5.getA5()) == a5.getA3());

    EXPECT_TRUE(dynamic_cast<A4*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a3.getA3()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA1()) == a4.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA2()) == a4.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a4.getA4()) == a4.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA14()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA13()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA2()) == a5.getA4());
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA3()) == 0);
    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA4()) == a5.getA4());
//    EXPECT_TRUE(dynamic_cast<A4*>(a5.getA5()) == a5.getA4());  // cast to protected base

    EXPECT_TRUE(dynamic_cast<A5*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a3.getA3()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a4.getA4()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA14()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA13()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA2()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA3()) == a5.getA5());
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA4()) == 0);
    EXPECT_TRUE(dynamic_cast<A5*>(a5.getA5()) == a5.getA5());
}

}  // t9

BPF_TEST(DynamicCast5, T1) { t1::test(); }

BPF_TEST(DynamicCast5, T2) { t2::test(); }

BPF_TEST(DynamicCast5, T3) { t3::test(); }

BPF_TEST(DynamicCast5, T4) { t4::test(); }

BPF_TEST(DynamicCast5, T5) { t5::test(); }

BPF_TEST(DynamicCast5, T6) { t6::test(); }

BPF_TEST(DynamicCast5, T7) { t7::test(); }

BPF_TEST(DynamicCast5, T8) { t8::test(); }

BPF_TEST(DynamicCast5, T9) { t9::test(); }
