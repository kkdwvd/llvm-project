// RUN: %clang_cc1 -triple bpfel -O2 -disable-llvm-passes -emit-llvm -debug-info-kind=standalone %s -o - | FileCheck %s
// RUN: %clang_cc1 -triple bpfel -O2 -disable-llvm-passes -emit-llvm %s -o - | FileCheck --check-prefix=NODEBUG %s
// RUN: %clang_cc1 -triple x86_64-linux-gnu -O2 -disable-llvm-passes -emit-llvm -debug-info-kind=standalone %s -o - | FileCheck --check-prefix=HOST %s

// BPF links a call to a function in another object through BTF, which comes
// from debug info. An inline member of an explicit instantiation declaration,
// which libc++ makes of basic_string<char>, is normally emitted available
// externally so that the optimizer may inline it, and when it does not, it
// drops the body with the subprogram, and the call that remains has no BTF.
// With debug info on, BPF refers to such a function as a declaration, which
// EmitExternalFunctionReferences describes.

template <class T> struct S {
  T get() const { return v; }
  int v;
};
extern template struct S<int>;

int f(const S<int>& s) { return s.get(); }

// CHECK-NOT: define available_externally
// CHECK: declare !dbg ![[SP:[0-9]+]] {{.*}}i32 @_ZNK1SIiE3getEv(
// CHECK: ![[SP]] = !DISubprogram(name: "get", linkageName: "_ZNK1SIiE3getEv"
// CHECK-NOT: ![[SP]] = {{.*}}DISPFlagDefinition

// NODEBUG: define available_externally {{.*}}i32 @_ZNK1SIiE3getEv(
// HOST: define available_externally {{.*}}i32 @_ZNK1SIiE3getEv(
