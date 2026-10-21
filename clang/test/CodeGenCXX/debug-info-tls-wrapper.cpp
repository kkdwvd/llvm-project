// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++20 \
// RUN:   -debug-info-kind=standalone -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple bpf -std=c++20 -femulated-tls \
// RUN:   -debug-info-kind=standalone -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++20 -emit-llvm -o - %s \
// RUN:   | FileCheck --check-prefix=NODEBUG %s

// The wrapper of a thread_local variable, which CodeGen defines for every
// access of one with external linkage, is described like the other
// functions CodeGen makes up: an artificial subprogram, named by the
// symbol, with the prototype it was defined with, a pointer to the variable
// taking nothing, and its instructions at the artificial location. Without
// debug info there is none.

thread_local int counter = 5;
extern thread_local long other;
struct Pair { int first, second; };
constinit thread_local Pair pair{1, 2};

int bump() { return counter++ + other + pair.second; }

// CHECK: define linkonce_odr hidden {{.*}}ptr @_ZTW5other() #{{[0-9]+}} comdat !dbg ![[OTHER:[0-9]+]] {
// CHECK: define weak_odr hidden {{.*}}ptr @_ZTW7counter() #{{[0-9]+}} comdat !dbg ![[COUNTER:[0-9]+]] {
// CHECK: call {{.*}}ptr @llvm.threadlocal.address.p0(ptr {{.*}}@counter), !dbg ![[LOC:[0-9]+]]
// CHECK: ret ptr %{{.*}}, !dbg ![[LOC]]
// CHECK: define weak_odr hidden {{.*}}ptr @_ZTW4pair() #{{[0-9]+}} comdat !dbg ![[PAIR:[0-9]+]] {

// CHECK-DAG: ![[COUNTER]] = distinct !DISubprogram(linkageName: "_ZTW7counter", scope: ![[FILE:[0-9]+]], file: ![[FILE]], type: ![[INTP_FN:[0-9]+]], flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: !{{[0-9]+}})
// CHECK-DAG: ![[INTP_FN]] = !DISubroutineType(types: ![[INTP_TYS:[0-9]+]])
// CHECK-DAG: ![[INTP_TYS]] = !{![[INTP:[0-9]+]]}
// CHECK-DAG: ![[INTP]] = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: ![[INT:[0-9]+]], size: 64)
// CHECK-DAG: ![[INT]] = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
// CHECK-DAG: ![[LOC]] = !DILocation(line: 0, scope: ![[COUNTER]])
// CHECK-DAG: ![[OTHER]] = distinct !DISubprogram(linkageName: "_ZTW5other", {{.*}}type: ![[LONGP_FN:[0-9]+]], flags: DIFlagArtificial, spFlags: DISPFlagDefinition
// CHECK-DAG: ![[LONGP_FN]] = !DISubroutineType(types: ![[LONGP_TYS:[0-9]+]])
// CHECK-DAG: ![[LONGP_TYS]] = !{![[LONGP:[0-9]+]]}
// CHECK-DAG: ![[LONGP]] = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: ![[LONG:[0-9]+]], size: 64)
// CHECK-DAG: ![[LONG]] = !DIBasicType(name: "long", size: 64, encoding: DW_ATE_signed)
// CHECK-DAG: ![[PAIR]] = distinct !DISubprogram(linkageName: "_ZTW4pair", {{.*}}type: ![[PAIRP_FN:[0-9]+]], flags: DIFlagArtificial, spFlags: DISPFlagDefinition
// CHECK-DAG: ![[PAIRP_FN]] = !DISubroutineType(types: ![[PAIRP_TYS:[0-9]+]])
// CHECK-DAG: ![[PAIRP_TYS]] = !{![[PAIRP:[0-9]+]]}
// CHECK-DAG: ![[PAIRP]] = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: ![[PAIRTY:[0-9]+]], size: 64)
// CHECK-DAG: ![[PAIRTY]] = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "Pair"

// NODEBUG: define weak_odr hidden {{.*}}ptr @_ZTW7counter() #{{[0-9]+}} comdat {
// NODEBUG-NOT: !dbg
