; RUN: opt < %s -passes=deadargelim -S | FileCheck %s
; RUN: opt < %s -mtriple=x86_64 -passes=deadargelim -S | FileCheck --check-prefix=OTHER %s
;
; On BPF a function with external linkage is a global subprogram, whose
; parameters the kernel's verifier checks at every call site against its
; prototype, read or not. A call must keep passing its arguments; a function
; with internal linkage keeps losing its dead parameters. The target says so
; through TargetTransformInfo::mustPreserveCallArguments(); another target
; keeps passing poison.

target triple = "bpf"

%struct.__sk_buff = type { i32 }

define i32 @unused_ctx(ptr %skb) {
; CHECK-LABEL: define i32 @unused_ctx(ptr %skb)
  ret i32 1
}

define internal i32 @unused_ctx_internal(ptr %skb) {
; CHECK-LABEL: define internal i32 @unused_ctx_internal()
  ret i32 2
}

define i32 @prog(ptr %skb) {
; CHECK-LABEL: define i32 @prog(ptr %skb)
; CHECK: call i32 @unused_ctx(ptr %skb)
; CHECK: call i32 @unused_ctx_internal()
; OTHER-LABEL: define i32 @prog(ptr %skb)
; OTHER: call i32 @unused_ctx(ptr poison)
; OTHER: call i32 @unused_ctx_internal()
  %a = call i32 @unused_ctx(ptr %skb)
  %b = call i32 @unused_ctx_internal(ptr %skb)
  %r = add i32 %a, %b
  ret i32 %r
}
