; RUN: llc -mtriple=bpfel -mcpu=v4 < %s | FileCheck %s
;
; The verifier reads r0 at every exit and refuses a pointer into the stack
; there. A function that returns an aggregate through the pointer its caller
; passed has that pointer in hand, and the register allocator may leave it
; in r0, which is free in a function that returns nothing:
;
;   struct big { long a, b, c; };
;   struct big make(long x) { struct big r = {x, 0, 0}; return r; }
;
; Such a function returns zero.

%struct.big = type { i64, i64, i64 }

define void @make(ptr sret(%struct.big) %r, i64 %x) {
; CHECK-LABEL: make:
; CHECK: r0 = 0
; CHECK-NEXT: exit
  store i64 %x, ptr %r
  %b = getelementptr %struct.big, ptr %r, i32 0, i32 1
  store i64 0, ptr %b
  %c = getelementptr %struct.big, ptr %r, i32 0, i32 2
  store i64 0, ptr %c
  ret void
}

define void @plain(ptr %p) {
; CHECK-LABEL: plain:
; CHECK-NOT: r0 = 0
; CHECK: exit
  store i64 0, ptr %p
  ret void
}
