; RUN: not llc -mtriple=bpfel -mcpu=v4 -filetype=asm -o - %s 2>&1 | FileCheck %s
; RUN: not llc -mtriple=bpfel -mcpu=v4 -enable-new-pm -filetype=asm -o - %s 2>&1 | FileCheck %s

; A filter with types, as clang emits for a dynamic exception specification,
; which C++17 removed. The empty one is a catch-all and is accepted, see
; unwind.ll and unwind-typed-catch-clauses.ll.

@_ZTIi = external constant ptr

declare void @may_throw()
declare i32 @rust_eh_personality(i32, i64, ptr, ptr)

define void @typed_filter() uwtable personality ptr @rust_eh_personality {
entry:
  invoke void @may_throw()
          to label %ok unwind label %lpad

ok:
  ret void

lpad:
  %lp = landingpad { ptr, i32 } filter [1 x ptr] [ptr @_ZTIi]
  ret void
}

; CHECK: error: {{.*}} BPF does not support exception filters with types, as of a dynamic exception specification
