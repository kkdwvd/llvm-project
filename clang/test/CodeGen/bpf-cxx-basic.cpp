// REQUIRES: bpf-registered-target
// RUN: %clang_cc1 -triple bpf -x c++ -std=c++17 -emit-llvm -debug-info-kind=limited -disable-llvm-passes %s -o - | FileCheck %s

// Verify that basic C++ constructs compile to BPF IR with debug info.

// --- Classes ---

class Foo {
  int x;
public:
  int get() const { return x; }
  void set(int v) { x = v; }
};

// CHECK: %class.Foo = type { i32 }

int test_class(Foo *f) {
  return f->get();
}
// CHECK: define dso_local noundef i32 @_Z10test_classP3Foo

// --- Templates ---

template <typename T>
struct Pair {
  T first;
  T second;
};

int test_template(Pair<int> *p) {
  return p->first + p->second;
}
// CHECK: define dso_local noundef i32 @_Z13test_templateP4PairIiE

// --- Namespaces ---

namespace ns {
  struct Bar {
    int val;
  };

  int get_val(Bar *b) { return b->val; }
}
// CHECK: define dso_local noundef i32 @_ZN2ns7get_valEPNS_3BarE

// --- constexpr ---

constexpr int add(int a, int b) { return a + b; }
int test_constexpr() { return add(1, 2); }
// CHECK: define dso_local noundef i32 @_Z14test_constexprv

// --- Lambdas ---

int test_lambda(int x) {
  auto fn = [](int a) { return a * 2; };
  return fn(x);
}
// CHECK: define dso_local noundef i32 @_Z11test_lambdai

// --- Operator overloading ---

struct Vec {
  int x, y;
  Vec operator+(const Vec &o) const { return {x + o.x, y + o.y}; }
};

Vec test_operator(Vec a, Vec b) {
  return a + b;
}
// CHECK: define dso_local {{.*}}@_Z13test_operator3VecS_

// Verify DW_TAG_class_type in debug info for Foo.
// CHECK: !DICompositeType(tag: DW_TAG_class_type, name: "Foo"
