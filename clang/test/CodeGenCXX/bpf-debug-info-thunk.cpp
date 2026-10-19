// RUN: %clang_cc1 -triple bpfel -emit-llvm -debug-info-kind=constructor %s -o - | FileCheck --check-prefixes=CHECK,BPF %s
// RUN: %clang_cc1 -triple x86_64-linux-gnu -emit-llvm -debug-info-kind=constructor %s -o - | FileCheck --check-prefixes=CHECK,OTHER %s

// A thunk has the type of the vtable slot it fills: it takes a pointer to
// the class of that slot, and returns and takes otherwise what the function
// it adjusts this for does. On BPF, where BTF describes a function by its
// type and names its parameters after their variables, a thunk also has
// variables for its parameters, without a location.

struct A {
  virtual int a() const;
  int x = 0;
};

// M is a non-primary base of Both: its slot for m() holds a thunk that
// adjusts this from the M in Both to Both.
struct M {
  virtual int m(int v) const;
};

struct Both : A, M {
  int m(int v) const override;
};
int Both::m(int v) const { return v + 3; }

// V is a virtual base of Diamond: its slot for f() holds a virtual thunk.
struct V {
  virtual long f(long n);
};
struct Diamond : virtual V {
  long f(long n) override;
};
long Diamond::f(long n) { return n; }

// CHECK-DAG: define {{.*}}i32 @_ZThn16_NK4Both1mEi(ptr {{.*}}%this, i32 {{.*}}%v) {{.*}}!dbg ![[THN:[0-9]+]]
// CHECK-DAG: define {{.*}}i64 @_ZTv0_n24_N7Diamond1fEl(ptr {{.*}}%this, i64 {{.*}}%n) {{.*}}!dbg ![[THV:[0-9]+]]

// BPF-DAG:   ![[THN]] = distinct !DISubprogram(linkageName: "_ZThn16_NK4Both1mEi", {{.*}}type: ![[THN_TYPE:[0-9]+]], flags: DIFlagArtificial | DIFlagThunk, {{.*}}retainedNodes: ![[THN_PARAMS:[0-9]+]])
// OTHER-DAG: ![[THN]] = distinct !DISubprogram(linkageName: "_ZThn16_NK4Both1mEi", {{.*}}type: ![[THN_TYPE:[0-9]+]], flags: DIFlagArtificial | DIFlagThunk, {{.*}}unit: !{{[0-9]+}})
// CHECK-DAG: ![[THN_TYPE]] = !DISubroutineType(types: ![[THN_TYPES:[0-9]+]])
// CHECK-DAG: ![[THN_TYPES]] = !{![[INT:[0-9]+]], ![[CONST_M_PTR:[0-9]+]], ![[INT]]}
// CHECK-DAG: ![[INT]] = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
// CHECK-DAG: ![[CONST_M_PTR]] = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: ![[CONST_M:[0-9]+]], size: 64)
// CHECK-DAG: ![[CONST_M]] = !DIDerivedType(tag: DW_TAG_const_type, baseType: ![[M:[0-9]+]])
// CHECK-DAG: ![[M]] = !DICompositeType(tag: DW_TAG_structure_type, name: "M",
// BPF-DAG:   ![[THN_PARAMS]] = !{![[THN_THIS:[0-9]+]], ![[THN_V:[0-9]+]]}
// BPF-DAG:   ![[THN_THIS]] = !DILocalVariable(name: "this", arg: 1, scope: ![[THN]], {{.*}}type: ![[CONST_M_PTR]], flags: DIFlagArtificial | DIFlagObjectPointer)
// BPF-DAG:   ![[THN_V]] = !DILocalVariable(name: "v", arg: 2, scope: ![[THN]], {{.*}}type: ![[INT]])

// CHECK-DAG: ![[THV]] = distinct !DISubprogram(linkageName: "_ZTv0_n24_N7Diamond1fEl", {{.*}}type: ![[THV_TYPE:[0-9]+]], flags: DIFlagArtificial | DIFlagThunk,
// CHECK-DAG: ![[THV_TYPE]] = !DISubroutineType(types: ![[THV_TYPES:[0-9]+]])
// CHECK-DAG: ![[THV_TYPES]] = !{![[LONG:[0-9]+]], ![[V_PTR:[0-9]+]], ![[LONG]]}
// CHECK-DAG: ![[V_PTR]] = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: ![[V:[0-9]+]], size: 64)
// CHECK-DAG: ![[V]] = !DICompositeType(tag: DW_TAG_structure_type, name: "V",

