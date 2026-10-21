; RUN: not llc -mtriple=bpfel -mcpu=v4 -filetype=asm -o - %s 2>&1 | FileCheck %s
; RUN: not llc -mtriple=bpfel -mcpu=v4 -enable-new-pm -filetype=asm -o - %s 2>&1 | FileCheck %s

; A landing pad with a catch clause for a type calls the personality routine
; itself, with the exception object, as a Wasm catch does: only
; __gxx_bpf_personality_v0 takes that call. The Itanium one, which an unwinder
; calls with five arguments between the frames, cannot be called from a pad.
; A pad with cleanups or a catch-all alone makes no call and takes any
; personality, see unwind.ll.

@_ZTIi = external constant ptr

declare void @may_throw()
declare i32 @__gxx_personality_v0(...)

define void @typed_itanium() uwtable personality ptr @__gxx_personality_v0 {
entry:
  invoke void @may_throw()
          to label %ok unwind label %lpad

ok:
  ret void

lpad:
  %lp = landingpad { ptr, i32 } catch ptr @_ZTIi
  %sel = extractvalue { ptr, i32 } %lp, 1
  ret void
}

; CHECK: error: {{.*}} BPF needs __gxx_bpf_personality_v0 as the personality of a function with a catch clause for a type
