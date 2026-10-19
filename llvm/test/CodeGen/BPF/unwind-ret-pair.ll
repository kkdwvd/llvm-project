; RUN: llc -mtriple=bpfel -mcpu=v4 -verify-machineinstrs -filetype=asm -o - %s | FileCheck %s
; RUN: llc -mtriple=bpfel -mcpu=v4 -enable-new-pm -verify-machineinstrs -filetype=asm -o - %s | FileCheck %s
; RUN: llc -mtriple=bpfel -mcpu=v1 -verify-machineinstrs -filetype=asm -o - %s | FileCheck %s

; A value of 9 to 16 bytes is returned in r0 and r2. The verifier checks both
; at every exit of a global function, unwind_exit included, so unwind_exit
; sets both, whatever the cleanups on the way left in r2. r0 is the exception
; object.

declare void @lock() nounwind
declare void @unlock() nounwind
declare void @may_throw()
declare i32 @_Unwind_RaiseException(ptr)
declare i32 @__gxx_personality_v0(...)

@exc = internal global [16 x i8] zeroinitializer

; unsigned __int128 with a guard whose destructor runs on the way out:
;   guard g; if (len > 100) throw 1; return ((u128)len << 64) | 7;
define i128 @aggregate_return(i32 %len) uwtable personality ptr @__gxx_personality_v0 {
entry:
  call void @lock()
  %cmp = icmp sgt i32 %len, 100
  br i1 %cmp, label %throw, label %ok

throw:
  store i32 1, ptr @exc
  %r = invoke i32 @_Unwind_RaiseException(ptr @exc)
          to label %unreachable unwind label %lpad

lpad:
  %lp = landingpad { ptr, i32 } cleanup
  call void @unlock()
  resume { ptr, i32 } %lp

ok:
  %conv = sext i32 %len to i128
  %shl = shl nsw i128 %conv, 64
  %or = or disjoint i128 %shl, 7
  call void @unlock()
  ret i128 %or

unreachable:
  unreachable
}

; CHECK-LABEL: aggregate_return:
; CHECK:         call unlock
; CHECK-NEXT:    r0 = 7
; CHECK-NEXT:    r2 = r6
; CHECK-NEXT:    exit
; CHECK:         [[EXN:r[6-9]]] = r0
; CHECK-NEXT:    call unlock
; CHECK-NEXT:    r0 = [[EXN]]
; CHECK-NEXT:    r2 = 0
; CHECK-NEXT:    unwind_exit

; A struct of 16 bytes, as clang returns it, through a pad the backend adds.
define [2 x i64] @struct_return(i64 %a) uwtable {
entry:
  call void @may_throw()
  %v0 = insertvalue [2 x i64] poison, i64 %a, 0
  %v1 = insertvalue [2 x i64] %v0, i64 3, 1
  ret [2 x i64] %v1
}

; CHECK-LABEL: struct_return:
; CHECK:         call may_throw
; CHECK-NEXT:    unwind_goto [[PAD:.LBB[0-9_]+]]
; CHECK:         exit
; CHECK:       [[PAD]]:
; CHECK-NEXT:    r2 = 0
; CHECK-NEXT:    unwind_exit

; A function that only unwinds has no exit to take the registers from: they
; come from its type.
define i128 @always_throws() uwtable {
entry:
  %r = call i32 @_Unwind_RaiseException(ptr @exc)
  unreachable
}

; CHECK-LABEL: always_throws:
; CHECK:         r0 = exc ll
; CHECK-NEXT:    r2 = 0
; CHECK-NEXT:    unwind_exit
; CHECK-NEXT:  .Lfunc_end

; A value of one register leaves r2 alone.
define i64 @one_register(i64 %a) uwtable {
entry:
  call void @may_throw()
  ret i64 %a
}

; CHECK-LABEL: one_register:
; CHECK:         unwind_goto [[PAD:.LBB[0-9_]+]]
; CHECK:       [[PAD]]:
; CHECK-NEXT:    unwind_exit
