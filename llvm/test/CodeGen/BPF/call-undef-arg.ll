; RUN: llc -mtriple=bpfel -mcpu=v4 < %s | FileCheck %s
;
; The verifier refuses a call that reads a register nothing wrote. The
; optimizer passes an undefined value where it has shown that the result it
; wants does not depend on it: the high half of a 128-bit operand of __multi3
; when only the low half of the product is used, as in
;
;   unsigned long low(unsigned __int128 a, unsigned long b) { return a * b; }
;
; The backend passes zero for it, and returns zero for an undefined return
; value, which the verifier reads at every exit the same way:
;
;   long f(void) { long u; return u; }

declare i64 @g(i64, i64, i64)

define i64 @f(i64 %x) {
; CHECK-LABEL: f:
; CHECK: r2 = 0
; CHECK: r3 = 0
; CHECK: call g
  %r = call i64 @g(i64 %x, i64 undef, i64 undef)
  ret i64 %r
}

define i64 @low(i128 %a, i64 %b) {
; CHECK-LABEL: low:
; CHECK-NOT: implicit-def
; CHECK: call __multi3
  %b128 = zext i64 %b to i128
  %p = mul i128 %a, %b128
  %lo = trunc i128 %p to i64
  ret i64 %lo
}

define i64 @ret_undef() {
; CHECK-LABEL: ret_undef:
; CHECK: r0 = 0
; CHECK-NEXT: exit
  ret i64 undef
}

define { i64, i64 } @ret_pair_half_undef(i64 %x) {
; CHECK-LABEL: ret_pair_half_undef:
; CHECK-DAG: r0 = r1
; CHECK-DAG: r2 = 0
; CHECK: exit
  %r = insertvalue { i64, i64 } undef, i64 %x, 0
  ret { i64, i64 } %r
}
