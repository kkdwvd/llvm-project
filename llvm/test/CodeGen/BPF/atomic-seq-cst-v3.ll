; RUN: not llc < %s -mtriple=bpfel -mcpu=v3 -o /dev/null 2>&1 | FileCheck %s
; RUN: not llc < %s -mtriple=bpfel -mcpu=v1 -o /dev/null 2>&1 | FileCheck %s

; A seq_cst load or store needs load-acquire and store-release, which come
; with -mcpu=v4, like any other atomic load or store.

; CHECK: error: {{.*}} in function load i64 (ptr): sequentially consistent (seq_cst) atomic load/store needs load-acquire and store-release (-mcpu=v4)
define i64 @load(ptr %p) {
  %v = load atomic i64, ptr %p seq_cst, align 8
  ret i64 %v
}

; CHECK: error: {{.*}} in function store void (ptr, i64): sequentially consistent (seq_cst) atomic load/store needs load-acquire and store-release (-mcpu=v4)
define void @store(ptr %p, i64 %v) {
  store atomic i64 %v, ptr %p seq_cst, align 8
  ret void
}
