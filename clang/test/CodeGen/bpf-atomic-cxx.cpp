// REQUIRES: bpf-registered-target
// RUN: %clang_cc1 -triple bpf -target-cpu v4 -std=c++26 -O2 -S %s -o - \
// RUN:   | FileCheck %s --check-prefixes=CHECK,V4
// RUN: %clang_cc1 -triple bpf -target-cpu v3 -std=c++26 -O2 -S %s -o - \
// RUN:   | FileCheck %s --check-prefixes=CHECK,V3

// std::atomic with its default order, seq_cst, its fences, atomic<bool> and
// atomic_flag, as libc++ builds them from the __c11_atomic builtins, through
// the BPF backend. A seq_cst load is a load-acquire, a seq_cst store of 32 or
// 64 bits an exchange, one of 8 bits a store-release and a full barrier, a
// fence a fetch-add of zero on the stack. Before -mcpu=v4, which brings
// load-acquire and store-release, a plain access with full barriers.

extern "C" {

// CHECK-LABEL: load:
// V4:          w0 = load_acquire((u32 *)(r1 + 0))
// V3:          w0 = *(u32 *)(r1 + 0)
// V3-NEXT:     r1 = 0
// V3-NEXT:     *(u64 *)(r10 - 8) = r1
// V3-NEXT:     r1 = atomic_fetch_add((u64 *)(r10 - 8), r1)
// CHECK:       exit
int load(_Atomic(int) *a) { return __c11_atomic_load(a, __ATOMIC_SEQ_CST); }

// CHECK-LABEL: store:
// CHECK:       w2 = xchg32_32(r1 + 0, w2)
// CHECK-NEXT:  exit
void store(_Atomic(int) *a, int v) {
  __c11_atomic_store(a, v, __ATOMIC_SEQ_CST);
}

// CHECK-LABEL: fetch_add:
// CHECK:       w0 = atomic_fetch_add((u32 *)(r1 + 0), w0)
int fetch_add(_Atomic(int) *a, int v) {
  return __c11_atomic_fetch_add(a, v, __ATOMIC_SEQ_CST);
}

// CHECK-LABEL: compare_exchange:
// CHECK:       w0 = cmpxchg32_32(r1 + 0, w0, w3)
bool compare_exchange(_Atomic(int) *a, int *expected, int desired) {
  return __c11_atomic_compare_exchange_strong(a, expected, desired,
                                              __ATOMIC_SEQ_CST,
                                              __ATOMIC_SEQ_CST);
}

// CHECK-LABEL: load_acquire:
// V4:          r0 = load_acquire((u64 *)(r1 + 0))
// V3:          r0 = *(u64 *)(r1 + 0)
// V3:          r1 = atomic_fetch_add((u64 *)(r10 - 8), r1)
// CHECK:       exit
long load_acquire(_Atomic(long) *a) {
  return __c11_atomic_load(a, __ATOMIC_ACQUIRE);
}

// CHECK-LABEL: store_release:
// V4:          store_release((u64 *)(r1 + 0), r2)
// V3:          r3 = atomic_fetch_add((u64 *)(r10 - 8), r3)
// V3-NEXT:     *(u64 *)(r1 + 0) = r2
// CHECK-NEXT:  exit
void store_release(_Atomic(long) *a, long v) {
  __c11_atomic_store(a, v, __ATOMIC_RELEASE);
}

// CHECK-LABEL: thread_fence:
// V4:          *(u64 *)(r10 - 8) = 0
// V4-NEXT:     r1 = 0
// V3:          r1 = 0
// V3-NEXT:     *(u64 *)(r10 - 8) = r1
// CHECK-NEXT:  r1 = atomic_fetch_add((u64 *)(r10 - 8), r1)
// CHECK-NEXT:  exit
void thread_fence() { __c11_atomic_thread_fence(__ATOMIC_SEQ_CST); }

// CHECK-LABEL: signal_fence:
// CHECK-NOT:   atomic_fetch_add
// CHECK:       exit
void signal_fence() { __c11_atomic_signal_fence(__ATOMIC_SEQ_CST); }

// CHECK-LABEL: load_bool:
// V4:          w0 = load_acquire((u8 *)(r1 + 0))
// V3:          w0 = *(u8 *)(r1 + 0)
// V3:          atomic_fetch_add((u64 *)(r10 - 8)
// CHECK:       exit
bool load_bool(_Atomic(bool) *a) {
  return __c11_atomic_load(a, __ATOMIC_SEQ_CST);
}

// atomic_flag::clear().
// CHECK-LABEL: store_bool:
// V4:          store_release((u8 *)(r1 + 0), w2)
// V4-NEXT:     *(u64 *)(r10 - 8) = 0
// V3:          atomic_fetch_add((u64 *)(r10 - 8)
// V3-NEXT:     *(u8 *)(r1 + 0) = w2
// CHECK:       atomic_fetch_add((u64 *)(r10 - 8)
// CHECK-NEXT:  exit
void store_bool(_Atomic(bool) *a, bool v) {
  __c11_atomic_store(a, v, __ATOMIC_SEQ_CST);
}

// atomic_flag::test_and_set(): a cmpxchg loop on the word around the flag.
// CHECK-LABEL: test_and_set:
// CHECK:       &= -4
// CHECK:       cmpxchg32_32
// CHECK:       goto
bool test_and_set(_Atomic(bool) *a) {
  return __c11_atomic_exchange(a, true, __ATOMIC_SEQ_CST);
}

} // extern "C"
