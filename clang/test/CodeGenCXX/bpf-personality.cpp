// RUN: %clang_cc1 -triple bpf -fcxx-exceptions -fexceptions -emit-llvm -o - %s \
// RUN:   | FileCheck %s
// RUN: %clang_cc1 -triple bpfel -fcxx-exceptions -fexceptions -emit-llvm -o - %s \
// RUN:   | FileCheck %s
// RUN: %clang_cc1 -triple bpfeb -fcxx-exceptions -fexceptions -emit-llvm -o - %s \
// RUN:   | FileCheck %s

// The C++ personality of BPF is __gxx_bpf_personality_v0, which a landing
// pad with a catch clause for a type calls itself, with the exception object,
// as a Wasm catch calls __gxx_wasm_personality_v0: the kernel unwinds and
// runs no personality routine between the frames. Declared as the Wasm one
// is, taking the exception object and returning int, where the Itanium
// personality the unwinder calls is declared variadic. The landing pads and
// the dispatch on the selector are the Itanium ones.

void may_throw();

// CHECK-LABEL: define {{.*}} @_Z6catchsv() {{.*}} personality ptr @__gxx_bpf_personality_v0
// CHECK: invoke void @_Z9may_throwv()
// CHECK: landingpad { ptr, i32 }
// CHECK-NEXT: catch ptr @_ZTIi
// CHECK: call i32 @llvm.eh.typeid.for.p0(ptr @_ZTIi)
int catchs() {
  try {
    may_throw();
  } catch (int e) {
    return e;
  }
  return 0;
}

// CHECK: declare i32 @__gxx_bpf_personality_v0(ptr)
