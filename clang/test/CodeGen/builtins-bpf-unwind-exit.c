// RUN: %clang_cc1 -triple bpfel -fexceptions -funwind-tables=2 -emit-llvm \
// RUN:   -disable-llvm-passes -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple bpfel -emit-llvm -disable-llvm-passes -o - %s \
// RUN:   | FileCheck --check-prefix=NOEXC %s

// __builtin_bpf_unwind_exit() starts or carries on an unwind of the object it
// is passed: llvm.bpf.unwind.exit, which does not return. Where the call may
// unwind into a landing pad, under a cleanup here, it is an invoke, as
// __builtin_wasm_throw() is.

void raise(void *obj) {
  __builtin_bpf_unwind_exit(obj);
}

// CHECK-LABEL: define dso_local void @raise(
// CHECK:         call void @llvm.bpf.unwind.exit(ptr %{{.*}})
// NOEXC-LABEL: define dso_local void @raise(
// NOEXC:         call void @llvm.bpf.unwind.exit(ptr %{{.*}})

void release(int *p);

void raise_owner(void *obj) {
  int x __attribute__((cleanup(release))) = 0;
  __builtin_bpf_unwind_exit(obj);
}

// CHECK-LABEL: define dso_local void @raise_owner(
// CHECK:         invoke void @llvm.bpf.unwind.exit(ptr %{{.*}})
// CHECK-NEXT:            to label %{{.*}} unwind label %[[PAD:.*]]
// CHECK:       [[PAD]]:
// CHECK-NEXT:    landingpad { ptr, i32 }
// CHECK-NEXT:            cleanup
// CHECK:         invoke void @release(ptr
