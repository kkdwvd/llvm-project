; RUN: llc -mtriple=bpfel -mcpu=v3 < %s | FileCheck %s
; RUN: llc -mtriple=bpfel -mcpu=v2 < %s | FileCheck --check-prefix=V2 %s
;
; An atomic insn that returns the old value is fully ordered. v1 and v2 have
; 'lock *(u64 *)(r10 - 8) += r1' only, which is not: there it is xchg.

define void @fence() {
; CHECK-LABEL: fence:
; CHECK:       r{{[0-9]}} = atomic_fetch_add((u64 *)(r10 - 8), r{{[0-9]}})
; V2-LABEL:    fence:
; V2:          r{{[0-9]}} = xchg_64(r10 - 8, r{{[0-9]}})
  fence seq_cst
  ret void
}
