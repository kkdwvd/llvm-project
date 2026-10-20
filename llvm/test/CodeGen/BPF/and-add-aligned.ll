; RUN: llc < %s -mtriple=bpfel -mcpu=v4 -verify-machineinstrs | FileCheck %s

; (and (add x, c1), c2), with c2 clearing low bits alone and x known to have
; those bits clear, is (add x, c1 & c2): the word around a narrow atomic at an
; odd offset in an aligned stack object or global, which AtomicExpand finds by
; masking the address (p & -4). The verifier refuses a bitwise operation on a
; pointer to the stack or a map value, and the add keeps the value a pointer.
; An address whose alignment is not known, a mask that is not of the low bits
; alone, and a 32-bit and stay as they are.

@g = internal global [8 x i8] zeroinitializer, align 8

declare void @use(ptr)

; (r10 - 7) & -4 is r10 - 8: the object is 8-aligned.
define i64 @stack_odd() {
; CHECK-LABEL: stack_odd:
; CHECK-NOT:     &=
; CHECK:         r{{[0-9]+}} = r10
; CHECK-NEXT:    r{{[0-9]+}} += -8
; CHECK-NOT:     &=
; CHECK:         exit
  %a = alloca [8 x i8], align 8
  call void @use(ptr %a)
  %p = getelementptr inbounds i8, ptr %a, i64 1
  %i = ptrtoint ptr %p to i64
  %m = and i64 %i, -4
  ret i64 %m
}

; (g + 5) & -4 is g + 4.
define i64 @global_odd() {
; CHECK-LABEL: global_odd:
; CHECK:         r{{[0-9]+}} = g ll
; CHECK-NEXT:    r{{[0-9]+}} += 4
; CHECK-NEXT:    exit
  %i = ptrtoint ptr getelementptr inbounds (i8, ptr @g, i64 5) to i64
  %m = and i64 %i, -4
  ret i64 %m
}

; A pointer of unknown alignment is masked as before.
define i64 @argument(ptr %p) {
; CHECK-LABEL: argument:
; CHECK:         r{{[0-9]+}} += 1
; CHECK-NEXT:    r{{[0-9]+}} &= -4
; CHECK-NEXT:    exit
  %q = getelementptr inbounds i8, ptr %p, i64 1
  %i = ptrtoint ptr %q to i64
  %m = and i64 %i, -4
  ret i64 %m
}

; A mask that clears more than the object's alignment is known to clear stays
; an and (on the object's address: the 1 it clears is folded away).
define i64 @mask_too_wide() {
; CHECK-LABEL: mask_too_wide:
; CHECK:         r{{[0-9]+}} &= -16
; CHECK-NEXT:    r0 = r{{[0-9]+}}
; CHECK-NEXT:    exit
  %a = alloca [8 x i8], align 8
  call void @use(ptr %a)
  %p = getelementptr inbounds i8, ptr %a, i64 1
  %i = ptrtoint ptr %p to i64
  %m = and i64 %i, -16
  ret i64 %m
}

; A mask that keeps low bits is not one that clears them: the and stays, on
; the value the DAG makes of the add, an or of the bit the alignment frees.
define i64 @mask_keeps_low_bits() {
; CHECK-LABEL: mask_keeps_low_bits:
; CHECK:         w{{[0-9]+}} |= 1
; CHECK-NEXT:    r{{[0-9]+}} &= 249
; CHECK-NEXT:    r0 = r{{[0-9]+}}
; CHECK-NEXT:    exit
  %a = alloca [8 x i8], align 8
  call void @use(ptr %a)
  %p = getelementptr inbounds i8, ptr %a, i64 1
  %i = ptrtoint ptr %p to i64
  %m = and i64 %i, 255
  ret i64 %m
}

; 32-bit registers hold no pointer.
define i32 @and_i32(i32 %x) {
; CHECK-LABEL: and_i32:
; CHECK:         w{{[0-9]+}} += 1
; CHECK-NEXT:    w{{[0-9]+}} &= -4
; CHECK-NEXT:    exit
  %y = add i32 %x, 1
  %m = and i32 %y, -4
  ret i32 %m
}
