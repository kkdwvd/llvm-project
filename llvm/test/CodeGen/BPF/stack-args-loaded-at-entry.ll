; RUN: llc -mtriple=bpfel -mcpu=v4 -O2 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=bpfel -mcpu=v4 -O0 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=bpfel -mcpu=v1 -O2 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=bpfeb -mcpu=v4 -O2 -verify-machineinstrs < %s | FileCheck %s

; The verifier accepts a load of an incoming stack argument, from r11, only
; before the first call and the first store to the outgoing argument area of
; the function ("r11 load must be before any r11 store or call insn"). The
; loads are the first instructions of the entry block, whatever the DAG would
; have ordered them after.

; The libcall that the legalizer makes for a 64-bit multiply with overflow,
; __multi3, is chained to the entry node, so nothing in the DAG orders it
; after the loads.
define i64 @multiply(i64 %a1, i64 %a2, i64 %a3, i64 %a4, i64 %a5, i64 %a6, i64 %a7) {
entry:
  %m = call { i64, i1 } @llvm.umul.with.overflow.i64(i64 %a1, i64 %a2)
  %v = extractvalue { i64, i1 } %m, 0
  %o = extractvalue { i64, i1 } %m, 1
  br i1 %o, label %overflow, label %ok
ok:
  %s = add i64 %v, %a6
  %t = add i64 %s, %a7
  ret i64 %t
overflow:
  ret i64 0
}
; CHECK-LABEL: multiply:
; CHECK-NOT:   call
; CHECK:       = *(u64 *)(r11 + 8)
; CHECK-NOT:   call
; CHECK:       = *(u64 *)(r11 + 16)
; CHECK:       call __multi3
; CHECK-NOT:   (r11 + {{[0-9]+}})
; CHECK:       exit

; A call the function makes itself.
define i64 @sinks(i64 %a1, i64 %a2, i64 %a3, i64 %a4, i64 %a5, i64 %a6, i64 %a7) {
entry:
  call void @sink(i64 %a1)
  %s = add i64 %a6, %a7
  ret i64 %s
}
; CHECK-LABEL: sinks:
; CHECK-NOT:   call
; CHECK:       = *(u64 *)(r11 + 8)
; CHECK-NOT:   call
; CHECK:       = *(u64 *)(r11 + 16)
; CHECK:       call sink
; CHECK-NOT:   (r11 + {{[0-9]+}})
; CHECK:       exit

; A call that passes stack arguments on, stored below r11 before the call.
define i64 @passes_on(i64 %a1, i64 %a2, i64 %a3, i64 %a4, i64 %a5, i64 %a6, i64 %a7) {
entry:
  %r = call i64 @seven(i64 %a7, i64 %a6, i64 %a5, i64 %a4, i64 %a3, i64 %a2, i64 %a1)
  ret i64 %r
}
; CHECK-LABEL: passes_on:
; CHECK-NOT:   (r11 - {{[0-9]+}})
; CHECK:       = *(u64 *)(r11 + 8)
; CHECK-NOT:   (r11 - {{[0-9]+}})
; CHECK:       = *(u64 *)(r11 + 16)
; CHECK:       *(u64 *)(r11 - 8) = r{{[0-9]+}}
; CHECK:       *(u64 *)(r11 - 16) = r{{[0-9]+}}
; CHECK:       call seven
; CHECK-NOT:   (r11 + {{[0-9]+}})
; CHECK:       exit

; An argument used in a later block only is loaded at the entry all the same,
; as is one that is not used, whose load the debug info of the argument
; follows.
define i64 @later(i64 %a1, i64 %a2, i64 %a3, i64 %a4, i64 %a5, i64 %a6, i64 %a7) {
entry:
  %m = call { i64, i1 } @llvm.umul.with.overflow.i64(i64 %a1, i64 %a2)
  %v = extractvalue { i64, i1 } %m, 0
  %o = extractvalue { i64, i1 } %m, 1
  br i1 %o, label %overflow, label %ok
ok:
  ret i64 %v
overflow:
  ret i64 %a7
}
; CHECK-LABEL: later:
; CHECK-NOT:   call
; CHECK:       = *(u64 *)(r11 + 8)
; CHECK-NOT:   call
; CHECK:       = *(u64 *)(r11 + 16)
; CHECK:       call __multi3
; CHECK-NOT:   (r11 + {{[0-9]+}})
; CHECK:       exit

declare { i64, i1 } @llvm.umul.with.overflow.i64(i64, i64)
declare void @sink(i64)
declare i64 @seven(i64, i64, i64, i64, i64, i64, i64)
