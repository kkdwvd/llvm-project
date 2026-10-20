; RUN: llc -mtriple=bpfel -mcpu=v4 -verify-machineinstrs -o - %s | FileCheck %s
; RUN: llc -mtriple=bpfel -mcpu=v4 -enable-new-pm -verify-machineinstrs -o - %s | FileCheck %s

; A call through a pointer unwinds like a call by name: every call that may
; unwind is followed by unwind_goto, whatever the callee. An invoke goes to
; its own pad, a plain call to the pad the backend adds, which resumes, and a
; call site that is nounwind, as the frontend makes one of a noexcept pointee,
; gets none: whether a call through a pointer may unwind is the frontend's
; decision, and the backend follows the call site.

declare void @release(ptr) nounwind
declare i32 @__gxx_personality_v0(...)

define i64 @invoke_indirect(ptr %f, ptr %node) uwtable personality ptr @__gxx_personality_v0 {
entry:
  %r = invoke i64 %f()
          to label %done unwind label %cleanup

done:
  ret i64 %r

cleanup:
  %lp = landingpad { ptr, i32 } cleanup
  call void @release(ptr %node)
  resume { ptr, i32 } %lp
}

; CHECK-LABEL: invoke_indirect:
; CHECK:         callx
; CHECK-NEXT:    unwind_goto [[PAD:.LBB[0-9_]+]]
; CHECK:         exit
; CHECK:       [[PAD]]:
; CHECK-NEXT:    [[EXN:r[6-9]]] = r0
; CHECK:         call release
; CHECK-NEXT:    r0 = [[EXN]]
; CHECK-NEXT:    unwind_exit

define i64 @call_indirect(ptr %f) uwtable {
entry:
  %r = call i64 %f()
  ret i64 %r
}

; CHECK-LABEL: call_indirect:
; CHECK:         callx
; CHECK-NEXT:    unwind_goto [[PAD:.LBB[0-9_]+]]
; CHECK:         exit
; CHECK:       [[PAD]]:
; CHECK-NEXT:    unwind_exit

define i64 @call_indirect_nounwind(ptr %f) uwtable {
entry:
  %r = call i64 %f() nounwind
  ret i64 %r
}

; CHECK-LABEL: call_indirect_nounwind:
; CHECK:         callx
; CHECK-NOT:     unwind_
; CHECK:         exit
