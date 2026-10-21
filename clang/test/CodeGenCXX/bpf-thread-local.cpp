// RUN: %clang_cc1 -triple bpf -std=c++20 -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple bpf -std=c++20 -femulated-tls -emit-llvm -o - %s \
// RUN:   | FileCheck %s
// RUN: %clang_cc1 -triple bpf -x c -std=c11 -fsyntax-only -verify %s

// BPF has thread-local storage, emulated as on the targets that default to
// emulated TLS: the driver passes -femulated-tls, the backend lowers each
// access to a call of __emutls_get_address(), and the runtime decides what
// a thread is. The frontend accepts thread_local and _Thread_local and
// emits the variables as thread-local; nothing distinguishes the target.

// expected-no-diagnostics

#ifdef __cplusplus
#if !__has_feature(cxx_thread_local) || !__has_feature(tls)
#error thread-local storage is supported on BPF
#endif

thread_local int counter = 5;
static thread_local long hits;
constinit thread_local int zeroed;

int bump() {
  hits++;
  return counter++ + zeroed;
}
#else
#if !__has_feature(c_thread_local) || !__has_feature(tls)
#error thread-local storage is supported on BPF
#endif

_Thread_local int counter = 5;
#endif

// CHECK-DAG: @counter = {{.*}}thread_local global i32 5
// CHECK-DAG: @zeroed = {{.*}}thread_local global i32 0
// CHECK-DAG: @_ZL4hits = internal thread_local global i64 0
// CHECK: call {{.*}}ptr @llvm.threadlocal.address.p0(ptr {{.*}}@counter)
