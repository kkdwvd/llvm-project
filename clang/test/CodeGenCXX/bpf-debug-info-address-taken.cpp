// RUN: %clang_cc1 -triple bpfel -emit-llvm -debug-info-kind=constructor \
// RUN:   -fcxx-exceptions -fexceptions %s -o - | FileCheck %s
// RUN: %clang_cc1 -triple bpfel -emit-llvm -debug-info-kind=constructor \
// RUN:   -fcxx-exceptions -fexceptions %s -o - \
// RUN:   | FileCheck --check-prefix=UNUSED %s
// RUN: %clang_cc1 -triple x86_64-linux-gnu -emit-llvm \
// RUN:   -debug-info-kind=constructor -fcxx-exceptions -fexceptions %s -o - \
// RUN:   | FileCheck --check-prefix=OTHER %s

// libbpf links a function in another object through BTF, which comes from
// debug info, whether the unit calls the function or takes its address. Every
// function the unit refers to and does not define gets the debug info of its
// declaration: those that the source uses, and those that CodeGen refers to
// on its own, by the variant that it refers to.

// The complete destructor of what a throw throws, passed to __cxa_throw():
// CHECK-DAG: declare !dbg ![[ERROR_D1:[0-9]+]] void @_ZN5ErrorD1Ev(
// CHECK-DAG: ![[ERROR_D1]] = !DISubprogram(name: "~Error", linkageName: "_ZN5ErrorD1Ev", {{.*}}type: ![[ERROR_DTOR_TY:[0-9]+]]
// CHECK-DAG: !DISubprogram(name: "~Error", linkageName: "_ZN5ErrorD4Ev", {{.*}}type: ![[ERROR_DTOR_TY]]

// The functions of another unit in a vtable that this unit emits:
// CHECK-DAG: declare !dbg ![[SIDES:[0-9]+]] {{.*}} @_ZNK5Shape5sidesEv(
// CHECK-DAG: declare !dbg ![[SHAPE_D1:[0-9]+]] void @_ZN5ShapeD1Ev(
// CHECK-DAG: declare !dbg ![[SHAPE_D0:[0-9]+]] void @_ZN5ShapeD0Ev(
// CHECK-DAG: ![[SIDES]] = !DISubprogram(name: "sides", linkageName: "_ZNK5Shape5sidesEv",
// CHECK-DAG: ![[SHAPE_D1]] = !DISubprogram(name: "~Shape", linkageName: "_ZN5ShapeD1Ev",
// CHECK-DAG: ![[SHAPE_D0]] = !DISubprogram(name: "~Shape", linkageName: "_ZN5ShapeD0Ev",

// The source's own: a function in an initializer, and one whose address it
// takes.
// CHECK-DAG: declare !dbg !{{[0-9]+}} i32 @external_fn(
// CHECK-DAG: declare !dbg !{{[0-9]+}} i32 @taken_fn(

// A function that the unit does not refer to gets nothing.
// UNUSED-NOT: unused_fn
// UNUSED: !llvm.dbg.cu

// Another target does not ask for debug info of external references.
// OTHER: declare void @_ZN5ErrorD1Ev(

struct Error {
  Error(int code) noexcept : code(code) {}
  ~Error();
  int code;
};

// The vtable is this unit's, which defines the key function area(). The
// destructor and sides() are another unit's.
struct Shape {
  virtual int area() const;
  virtual int sides() const;
  virtual ~Shape();
  int size;
};

int Shape::area() const { return size; }

extern "C" int external_fn(int);
extern "C" int taken_fn(int);
extern "C" int unused_fn(int);

int (*table[])(int) = {external_fn};

void throws(int v) {
  throw Error(v);
}

int (*take())(int) {
  return &taken_fn;
}
