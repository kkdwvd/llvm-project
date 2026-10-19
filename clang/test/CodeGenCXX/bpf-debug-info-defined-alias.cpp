// RUN: %clang_cc1 -triple bpfel -emit-llvm -debug-info-kind=constructor \
// RUN:   -O1 -disable-llvm-passes -mconstructor-aliases %s -o - | FileCheck %s

// A function that the translation unit defines is no external reference,
// though its first declaration is the one in the class: B's base destructor,
// an alias of A's, does not get the debug info of one.

struct A {
  virtual ~A();
};

struct B : A {
  ~B() override;
};

A::~A() {}
B::~B() {}

// CHECK: @_ZN1BD2Ev = {{.*}}alias void (ptr), ptr @_ZN1AD2Ev
// CHECK-NOT: DIImportedEntity
