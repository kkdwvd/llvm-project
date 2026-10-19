// RUN: %clang_cc1 -triple bpfel -debug-info-kind=standalone -O2 -S -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple bpfel -debug-info-kind=standalone -fcxx-exceptions -fexceptions -funwind-tables=1 -O2 -S -o - %s | FileCheck %s

// A pointer to a member or std::nullptr_t in a prototype, a member or a
// global: BTF, which the BPF backend derives from the debug info, has no kind
// for either and used to assert on them. A pointer to a data member is the
// member's offset and travels as a 'long', one to a member function as two
// words, an '__int128'; std::nullptr_t as a scalar of the pointer's size,
// named as the debug info names it.

struct M {
  int i;
  int f();
};

int M::* data(int M::* p) { return p; }
int (M::* func(int (M::* q)()))() { return q; }
decltype(nullptr) null(decltype(nullptr) n) { return n; }

struct Holder {
  int M::* member;
  decltype(nullptr) nothing;
};
Holder holder;

#ifdef __EXCEPTIONS
int thrower(int x, int M::* const& p) {
  if (x)
    throw p;
  return x;
}
#endif

// CHECK: .section .BTF
// CHECK: .ascii "long"
// CHECK: .ascii "__int128"
// CHECK: .ascii "decltype(nullptr)"
// CHECK: .ascii "Holder"
