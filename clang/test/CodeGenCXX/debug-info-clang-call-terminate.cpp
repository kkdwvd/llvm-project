// RUN: %clang_cc1 -triple x86_64-linux-gnu -fcxx-exceptions -fexceptions \
// RUN:   -debug-info-kind=standalone -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple bpf -fcxx-exceptions -fexceptions \
// RUN:   -debug-info-kind=standalone -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple x86_64-linux-gnu -fcxx-exceptions -fexceptions \
// RUN:   -debug-info-kind=line-tables-only -emit-llvm -o - %s \
// RUN:   | FileCheck --check-prefix=LINES %s
// RUN: %clang_cc1 -triple x86_64-linux-gnu -fcxx-exceptions -fexceptions \
// RUN:   -emit-llvm -o - %s | FileCheck --check-prefix=NODEBUG %s

// __clang_call_terminate(), which CodeGen defines for an exception that
// would leave a noexcept function, is described like the other functions
// CodeGen makes up: an artificial subprogram, named by the symbol, with the
// prototype it was defined with, void (char *), and its instructions at the
// artificial location. With line tables only the prototype is left out, as
// for every function; without debug info there is none.

void may_throw();
void no_throw() noexcept { may_throw(); }

// CHECK: define linkonce_odr hidden void @__clang_call_terminate(ptr noundef %0) #{{[0-9]+}} comdat !dbg ![[SP:[0-9]+]] {
// CHECK-NEXT: call ptr @__cxa_begin_catch(ptr %0) #{{[0-9]+}}, !dbg ![[LOC:[0-9]+]]
// CHECK-NEXT: call void @_ZSt9terminatev() #{{[0-9]+}}, !dbg ![[LOC]]
// CHECK-NEXT: unreachable, !dbg ![[LOC]]

// CHECK-DAG: ![[SP]] = distinct !DISubprogram(linkageName: "__clang_call_terminate", scope: ![[FILE:[0-9]+]], file: ![[FILE]], type: ![[TYPE:[0-9]+]], flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: !{{[0-9]+}})
// CHECK-DAG: ![[TYPE]] = !DISubroutineType(types: ![[TYPES:[0-9]+]])
// CHECK-DAG: ![[TYPES]] = !{null, ![[CHARP:[0-9]+]]}
// CHECK-DAG: ![[CHARP]] = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: ![[CHAR:[0-9]+]], size: 64)
// CHECK-DAG: ![[CHAR]] = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
// CHECK-DAG: ![[LOC]] = !DILocation(line: 0, scope: ![[SP]])

// LINES: define linkonce_odr hidden void @__clang_call_terminate(ptr noundef %0) #{{[0-9]+}} comdat !dbg ![[SP:[0-9]+]] {
// LINES-DAG: ![[SP]] = distinct !DISubprogram(linkageName: "__clang_call_terminate", scope: ![[FILE:[0-9]+]], file: ![[FILE]], type: ![[TYPE:[0-9]+]], flags: DIFlagArtificial, spFlags: DISPFlagDefinition, unit: !{{[0-9]+}})
// LINES-DAG: ![[TYPE]] = !DISubroutineType(types: ![[TYPES:[0-9]+]])
// LINES-DAG: ![[TYPES]] = !{}

// NODEBUG: define linkonce_odr hidden void @__clang_call_terminate(ptr noundef %0) #{{[0-9]+}} comdat {
// NODEBUG-NOT: !dbg
