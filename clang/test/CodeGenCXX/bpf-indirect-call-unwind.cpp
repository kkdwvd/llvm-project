// RUN: %clang_cc1 -triple bpfel -fcxx-exceptions -fexceptions -O1 \
// RUN:   -disable-llvm-passes -emit-llvm -o - %s | FileCheck --check-prefixes=CHECK,BPF %s
// RUN: %clang_cc1 -triple x86_64-linux-gnu -fcxx-exceptions -fexceptions -O1 \
// RUN:   -disable-llvm-passes -emit-llvm -o - %s | FileCheck --check-prefixes=CHECK,OTHER %s

// On BPF the verifier checks a call through a pointer, a virtual call
// included, against every function the pointer could hold, and whether a
// function throws is not in its BTF. A call through a pointer keeps its
// unwind edge whatever the pointee's exception specification says: a noexcept
// pointee called from a noexcept function is an invoke to the terminate pad,
// and one called in a try block an invoke to its landing pad. A call of a
// function by name, or of a helper by its number, is not through a pointer.

struct Base {
  virtual void f() noexcept;
  virtual ~Base();
};

void (*fp)() noexcept;
void by_name() noexcept;
static void (*const helper)(void) noexcept = (void (*)(void) noexcept)7;

// CHECK-LABEL: define {{.*}} @_Z11through_ptrv()
// BPF:   invoke void %{{.*}}()
// BPF:   terminate.lpad:
// OTHER: call void %{{.*}}() #[[NOUNWIND:[0-9]+]]
// OTHER-NOT: terminate.lpad
void through_ptr() noexcept { fp(); }

// CHECK-LABEL: define {{.*}} @_Z12virtual_callR4Base(
// BPF:   invoke void %{{.*}}(ptr {{.*}} %{{.*}})
// OTHER: call void %{{.*}}(ptr {{.*}} %{{.*}}) #[[NOUNWIND]]
void virtual_call(Base& b) noexcept { b.f(); }

// A virtual destructor through the vtable.
// CHECK-LABEL: define {{.*}} @_Z9delete_itP4Base(
// BPF:   invoke void %{{.*}}(ptr {{.*}} %{{.*}})
// OTHER: call void %{{.*}}(ptr {{.*}} %{{.*}}) #[[NOUNWIND]]
void delete_it(Base* b) noexcept { delete b; }

// In a try block the unwind edge goes to the landing pad.
// CHECK-LABEL: define {{.*}} @_Z6in_tryv()
// BPF:   invoke void %{{.*}}()
// BPF:   landingpad
// OTHER: call void %{{.*}}() #[[NOUNWIND]]
int in_try() {
  try {
    fp();
  } catch (...) {
    return 1;
  }
  return 0;
}

// A function by name and a helper by number keep their noexcept.
// CHECK-LABEL: define {{.*}} @_Z18by_name_and_helperv()
// CHECK:   call void @_Z7by_namev() #[[NOUNWIND:[0-9]+]]
// CHECK:   call void inttoptr (i64 7 to ptr)() #[[NOUNWIND]]
// CHECK-NOT: invoke
void by_name_and_helper() noexcept {
  by_name();
  helper();
}

// CHECK: attributes #[[NOUNWIND]] = { nounwind }
