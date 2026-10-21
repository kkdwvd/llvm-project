//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// libc++abi's dynamic_cast3.pass.cpp as BPF programs, one per namespace,
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

/*

A1   A2   A3

*/

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
{
    char _[93481];
    virtual ~A3() {}

    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA3()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t1

/*

A1   A2
|
A3

*/

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
    : public A1
{
    char _[93481];
    virtual ~A3() {}

    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA3()) == a3.getA1());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == 0);

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
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
    : public virtual A1
{
    char _[93481];
    virtual ~A3() {}

    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA3()) == a3.getA1());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == 0);

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
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
    : private A1
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == 0);

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
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
    : private virtual A1
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == 0);

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t5

/*

A1   A2
 \  /
  A3

*/

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
      public A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA3()) == a3.getA1());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
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
    : public virtual A1,
      public A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA3()) == a3.getA1());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
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
    : private A1,
      public A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == 0);

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
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
    : private virtual A1,
      public A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == 0);

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t9

namespace t10
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
      public virtual A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA3()) == a3.getA1());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t10

namespace t11
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
    : private A1,
      public virtual A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == 0);

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t11

namespace t12
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
    : private virtual A1,
      public virtual A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == 0);

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t12

namespace t13
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
    : private A1,
      private A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == 0);

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t13

namespace t14
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
    : private virtual A1,
      private A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == 0);

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t14

namespace t15
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
    : private virtual A1,
      private virtual A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == 0);

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t15

/*

A1
|
A2
|
A3

*/

namespace t16
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
    : public A1
{
    char _[34981];
    virtual ~A2() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
};

struct A3
    : public A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA1()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA3()) == a3.getA1());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA1()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t16

namespace t17
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
    : public virtual A1
{
    char _[34981];
    virtual ~A2() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
};

struct A3
    : public A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA1()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA3()) == a3.getA1());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA1()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t17

namespace t18
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
    : private A1
{
    char _[34981];
    virtual ~A2() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
};

struct A3
    : public A2
{
    char _[93481];
    virtual ~A3() {}

    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA1()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t18

namespace t19
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
    : protected virtual A1
{
    char _[34981];
    virtual ~A2() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
};

struct A3
    : public A2
{
    char _[93481];
    virtual ~A3() {}

    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA1()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t19

namespace t20
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
    : public virtual A1
{
    char _[34981];
    virtual ~A2() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
};

struct A3
    : public virtual A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA1()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA3()) == a3.getA1());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA1()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t20

namespace t21
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
    : private A1
{
    char _[34981];
    virtual ~A2() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
};

struct A3
    : public virtual A2
{
    char _[93481];
    virtual ~A3() {}

    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA1()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t21

namespace t22
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
    : protected virtual A1
{
    char _[34981];
    virtual ~A2() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
};

struct A3
    : public virtual A2
{
    char _[93481];
    virtual ~A3() {}

    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA1()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t22

namespace t23
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
    : private A1
{
    char _[34981];
    virtual ~A2() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
};

struct A3
    : private A2
{
    char _[93481];
    virtual ~A3() {}

    t23::A1* getA1() {return A2::getA1();}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA1()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t23

namespace t24
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
    : protected virtual A1
{
    char _[34981];
    virtual ~A2() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
};

struct A3
    : private A2
{
    char _[93481];
    virtual ~A3() {}

    t24::A1* getA1() {return A2::getA1();}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA1()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t24

namespace t25
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
    : protected virtual A1
{
    char _[34981];
    virtual ~A2() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
};

struct A3
    : private virtual A2
{
    char _[93481];
    virtual ~A3() {}

    t25::A1* getA1() {return A2::getA1();}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA1()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t25

/*

A1 A1
|  |
A2 |
 \ |
  A3

*/

namespace t26
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
    : public A1
{
    char _[34981];
    virtual ~A2() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
};

struct A3
    : public A1,
      public A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA12() {return A2::getA1();}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA1()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA12()) == a3.getA12());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == a3.getA12());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA1()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA12()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA12()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t26

namespace t27
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
    : private A1
{
    char _[34981];
    virtual ~A2() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
};

struct A3
    : public A1,
      public A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA12() {return A2::getA1();}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA1()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA12()) == a3.getA12());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA12()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA12()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t27

namespace t28
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
    : public A1
{
    char _[34981];
    virtual ~A2() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
};

struct A3
    : private A1,
      public A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA12() {return A2::getA1();}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA1()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == a3.getA12());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA1()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA12()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA12()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t28

namespace t29
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
    : public A1
{
    char _[34981];
    virtual ~A2() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
};

struct A3
    : public A1,
      private A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA12() {return A2::getA1();}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA1()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == a3.getA12());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA1()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA12()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA12()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t29

namespace t30
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
    : public A1
{
    char _[34981];
    virtual ~A2() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
};

struct A3
    : private A1,
      private A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA12() {return A2::getA1();}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA1()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA12()) == a3.getA12());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == a3.getA12());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA1()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA12()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA12()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t30

namespace t31
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
    : private A1
{
    char _[34981];
    virtual ~A2() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
};

struct A3
    : public A1,
      private A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA12() {return A2::getA1();}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA1()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA12()) == a3.getA12());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA12()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA12()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t31

namespace t32
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
    : private A1
{
    char _[34981];
    virtual ~A2() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
};

struct A3
    : private A1,
      public A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA12() {return A2::getA1();}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA1()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA12()) == a3.getA12());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA12()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA12()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t32

namespace t33
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
    : private A1
{
    char _[34981];
    virtual ~A2() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
};

struct A3
    : private A1,
      private A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA12() {return A2::getA1();}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA1()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA12()) == a3.getA12());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA12()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA12()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t33

/*

A1
| \
A2 \
 \ |
  A3

*/

namespace t34
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
    : public virtual A1
{
    char _[34981];
    virtual ~A2() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
};

struct A3
    : public virtual A1,
      public A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return A1::getA1();}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA1()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA3()) == a3.getA1());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA1()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t34

namespace t35
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
    : private virtual A1
{
    char _[34981];
    virtual ~A2() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
};

struct A3
    : public virtual A1,
      public A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return A1::getA1();}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA1()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA3()) == a3.getA1());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t35

namespace t36
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
    : public virtual A1
{
    char _[34981];
    virtual ~A2() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
};

struct A3
    : private virtual A1,
      public A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return A1::getA1();}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA1()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA3()) == a3.getA1());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA1()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t36

namespace t37
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
    : public virtual A1
{
    char _[34981];
    virtual ~A2() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
};

struct A3
    : public virtual A1,
      private A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return A1::getA1();}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA1()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA3()) == a3.getA1());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA1()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t37

namespace t38
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
    : public virtual A1
{
    char _[34981];
    virtual ~A2() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
};

struct A3
    : private virtual A1,
      private A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return A1::getA1();}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA1()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA2()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA2()) == a3.getA1());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA1()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t38

namespace t39
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
    : private virtual A1
{
    char _[34981];
    virtual ~A2() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
};

struct A3
    : public virtual A1,
      private A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return A1::getA1();}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA1()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA3()) == a3.getA1());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t39

namespace t40
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
    : private virtual A1
{
    char _[34981];
    virtual ~A2() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
};

struct A3
    : private virtual A1,
      public A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return A1::getA1();}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA1()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA3()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == a3.getA3());
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t40

namespace t41
{

struct A1
{
    char _[43981];
    virtual ~A1() {}

    A1* getA1() {return this;}
};

struct A2
    : private virtual A1
{
    char _[34981];
    virtual ~A2() {}

    A1* getA1() {return this;}
    A2* getA2() {return this;}
};

struct A3
    : private virtual A1,
      private A2
{
    char _[93481];
    virtual ~A3() {}

    A1* getA1() {return A1::getA1();}
    A2* getA2() {return this;}
    A3* getA3() {return this;}
};

static void test()
{
    A1& a1 = *new A1;
    A2& a2 = *new A2;
    A3& a3 = *new A3;
    EXPECT_TRUE(dynamic_cast<A1*>(a1.getA1()) == a1.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a2.getA1()) == a2.getA1());
    EXPECT_TRUE(dynamic_cast<A1*>(a3.getA1()) == a3.getA1());

    EXPECT_TRUE(dynamic_cast<A2*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a2.getA2()) == a2.getA2());
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A2*>(a3.getA2()) == a3.getA2());

    EXPECT_TRUE(dynamic_cast<A3*>(a1.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a2.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA1()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA2()) == 0);
    EXPECT_TRUE(dynamic_cast<A3*>(a3.getA3()) == a3.getA3());
}

}  // t41

BPF_TEST(DynamicCast3, T1) { t1::test(); }

BPF_TEST(DynamicCast3, T2) { t2::test(); }

BPF_TEST(DynamicCast3, T3) { t3::test(); }

BPF_TEST(DynamicCast3, T4) { t4::test(); }

BPF_TEST(DynamicCast3, T5) { t5::test(); }

BPF_TEST(DynamicCast3, T6) { t6::test(); }

BPF_TEST(DynamicCast3, T7) { t7::test(); }

BPF_TEST(DynamicCast3, T8) { t8::test(); }

BPF_TEST(DynamicCast3, T9) { t9::test(); }

BPF_TEST(DynamicCast3, T10) { t10::test(); }

BPF_TEST(DynamicCast3, T11) { t11::test(); }

BPF_TEST(DynamicCast3, T12) { t12::test(); }

BPF_TEST(DynamicCast3, T13) { t13::test(); }

BPF_TEST(DynamicCast3, T14) { t14::test(); }

BPF_TEST(DynamicCast3, T15) { t15::test(); }

BPF_TEST(DynamicCast3, T16) { t16::test(); }

BPF_TEST(DynamicCast3, T17) { t17::test(); }

BPF_TEST(DynamicCast3, T18) { t18::test(); }

BPF_TEST(DynamicCast3, T19) { t19::test(); }

BPF_TEST(DynamicCast3, T20) { t20::test(); }

BPF_TEST(DynamicCast3, T21) { t21::test(); }

BPF_TEST(DynamicCast3, T22) { t22::test(); }

BPF_TEST(DynamicCast3, T23) { t23::test(); }

BPF_TEST(DynamicCast3, T24) { t24::test(); }

BPF_TEST(DynamicCast3, T25) { t25::test(); }

BPF_TEST(DynamicCast3, T26) { t26::test(); }

BPF_TEST(DynamicCast3, T27) { t27::test(); }

BPF_TEST(DynamicCast3, T28) { t28::test(); }

BPF_TEST(DynamicCast3, T29) { t29::test(); }

BPF_TEST(DynamicCast3, T30) { t30::test(); }

BPF_TEST(DynamicCast3, T31) { t31::test(); }

BPF_TEST(DynamicCast3, T32) { t32::test(); }

BPF_TEST(DynamicCast3, T33) { t33::test(); }

BPF_TEST(DynamicCast3, T34) { t34::test(); }

BPF_TEST(DynamicCast3, T35) { t35::test(); }

BPF_TEST(DynamicCast3, T36) { t36::test(); }

BPF_TEST(DynamicCast3, T37) { t37::test(); }

BPF_TEST(DynamicCast3, T38) { t38::test(); }

BPF_TEST(DynamicCast3, T39) { t39::test(); }

BPF_TEST(DynamicCast3, T40) { t40::test(); }

BPF_TEST(DynamicCast3, T41) { t41::test(); }
