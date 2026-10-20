; RUN: opt -S -mtriple=bpfel -mcpu=v3 -passes=bpf-align-atomics < %s \
; RUN:   | FileCheck %s --check-prefix=IR
; RUN: llc < %s -mtriple=bpfel -mcpu=v3 -verify-machineinstrs \
; RUN:   | FileCheck %s --check-prefix=ASM

; An 8- or 16-bit atomic works on the 32-bit word around it. Before
; AtomicExpand, BPF gives it the alignment its address is known to have,
; raising that of the stack object or global it points into to 4 where
; allowed, so that the word needs no masking of the address (p & -4), which
; the verifier refuses on a pointer to the stack or a map value.

; The globals are printed first.
; IR: @g = internal global i16 0, align 4
; IR: @gs = internal global [8 x i8] zeroinitializer, align 4

declare void @use(ptr)

; IR-LABEL: @stack(
; IR:         %a = alloca i8, align 4
; IR:         atomicrmw or ptr %a, i8 -16 seq_cst, align 4
; ASM-LABEL: stack:
; ASM-NOT:     &= -4
; ASM:         atomic_fetch_or((u32 *)(r10 - 4), w{{[0-9]+}})
define i8 @stack() {
  %a = alloca i8, align 1
  store i8 15, ptr %a
  %old = atomicrmw or ptr %a, i8 -16 seq_cst, align 1
  call void @use(ptr %a)
  ret i8 %old
}

; A member at an offset that keeps the word aligned.
; IR-LABEL: @stack_member(
; IR:         %s = alloca [8 x i8], align 4
; IR:         atomicrmw xchg ptr %m, i8 1 seq_cst, align 4
; ASM-LABEL: stack_member:
; ASM-NOT:     &= -4
; ASM:         cmpxchg32_32(r10 - 4,
define i8 @stack_member() {
  %s = alloca [8 x i8], align 1
  %m = getelementptr inbounds i8, ptr %s, i64 4
  %old = atomicrmw xchg ptr %m, i8 1 seq_cst, align 1
  call void @use(ptr %s)
  ret i8 %old
}

@g = internal global i16 0, align 2

; IR-LABEL: @global(
; IR:         cmpxchg ptr @g, i16 0, i16 1 seq_cst seq_cst, align 4
; ASM-LABEL: global:
; ASM-NOT:     &= -4
; ASM:         cmpxchg32_32
define { i16, i1 } @global() {
  %res = cmpxchg ptr @g, i16 0, i16 1 seq_cst seq_cst, align 2
  ret { i16, i1 } %res
}

; A member at an odd offset keeps its own alignment, and AtomicExpand masks
; its address; but the object is aligned, so the mask clears bits known to be
; zero in the object's address and folds into the offset (and-add-aligned.ll).
; IR-LABEL: @stack_odd_member(
; IR:         %s = alloca [8 x i8], align 4
; IR:         atomicrmw or ptr %m, i8 1 seq_cst, align 1
; ASM-LABEL: stack_odd_member:
; ASM-NOT:     &=
; ASM:         atomic_fetch_or((u32 *)(r10 - 8), w{{[0-9]+}})
define i8 @stack_odd_member() {
  %s = alloca [8 x i8], align 1
  %m = getelementptr inbounds i8, ptr %s, i64 1
  %old = atomicrmw or ptr %m, i8 1 seq_cst, align 1
  call void @use(ptr %s)
  ret i8 %old
}

@gs = internal global [8 x i8] zeroinitializer, align 1

; IR-LABEL: @global_odd_member(
; IR:         atomicrmw or ptr getelementptr inbounds (i8, ptr @gs, i64 5), i8 1 seq_cst, align 1
; ASM-LABEL: global_odd_member:
; ASM-NOT:     &=
; ASM:         r{{[0-9]+}} = gs ll
; ASM-NEXT:    atomic_fetch_or((u32 *)(r{{[0-9]+}} + 4), w{{[0-9]+}})
define i8 @global_odd_member() {
  %old = atomicrmw or ptr getelementptr inbounds (i8, ptr @gs, i64 5), i8 1 seq_cst, align 1
  ret i8 %old
}

; A pointer of unknown alignment is masked as before.
; IR-LABEL: @argument(
; IR:         atomicrmw or ptr %p, i8 1 seq_cst, align 1
; ASM-LABEL: argument:
; ASM:         &= -4
define i8 @argument(ptr %p) {
  %old = atomicrmw or ptr %p, i8 1 seq_cst, align 1
  ret i8 %old
}

