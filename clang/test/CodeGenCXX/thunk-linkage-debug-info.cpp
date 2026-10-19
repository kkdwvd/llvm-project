// RUN: %clang_cc1 -triple x86_64-linux-gnu -debug-info-kind=limited -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple bpfel -debug-info-kind=limited -emit-llvm -o - %s | FileCheck %s

// A thunk gets its linkage before its debug info is emitted, so that the
// thunk of a class with internal linkage, whose symbol is local, has a
// subprogram local to its unit as well. The BPF backend takes a function's
// BTF linkage from the function itself; DWARF's DW_AT_external comes from the
// subprogram and used to say external for such a thunk.

namespace {
struct A {
  virtual ~A() {}
  int x = 1;
};
struct B {
  virtual ~B() {}
  virtual int b() { return 2; }
  int y = 2;
};
struct C : A, B {
  int b() override { return 3; }
};
} // namespace

struct D {
  virtual ~D() {}
  int z = 4;
};
struct E : D, B {
  int b() override { return 5; }
};

void *make() { return new C; }
void *make2() { return new E; }

// CHECK: define internal {{.*}} @_ZThn16_N12_GLOBAL__N_11C1bEv({{.*}} !dbg ![[LOCAL:[0-9]+]]
// CHECK: define linkonce_odr {{.*}} @_ZThn16_N1E1bEv({{.*}} !dbg ![[GLOBAL:[0-9]+]]
// CHECK: ![[LOCAL]] = distinct !DISubprogram(linkageName: "_ZThn16_N12_GLOBAL__N_11C1bEv", {{.*}}spFlags: DISPFlagLocalToUnit | DISPFlagDefinition
// CHECK: ![[GLOBAL]] = distinct !DISubprogram(linkageName: "_ZThn16_N1E1bEv", {{.*}}spFlags: DISPFlagDefinition
