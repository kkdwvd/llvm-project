; RUN: llc -mtriple=bpfel -mcpu=v4 -stop-after=bpf-unwind-prepare -o - %s | FileCheck %s
; RUN: llc -mtriple=bpfel -mcpu=v4 -enable-new-pm -stop-after=bpf-unwind-prepare -o - %s | FileCheck %s
; RUN: llc -mtriple=bpfel -mcpu=v4 -verify-machineinstrs -o /dev/null %s

; The selector a pad computes for each kind of clause. A pad with a catch
; clause for a type calls the personality routine, which reads it from the
; function's table; a pad with a catch-all or an empty filter alone knows it
; without one. unwind-typed-catch.ll has the cases clang emits for C++.

@_ZTIi = external constant ptr
@_ZTIl = external constant ptr

declare void @may_throw()
declare void @use(i32) nounwind
declare i32 @__gxx_bpf_personality_v0(ptr)

; A catch-all matches without asking the runtime, and its type id is that of
; a null type, as the personality routine would report.
define void @catch_all() uwtable personality ptr @__gxx_bpf_personality_v0 {
entry:
  invoke void @may_throw()
          to label %ok unwind label %lpad

ok:
  ret void

lpad:
  %lp = landingpad { ptr, i32 } catch ptr null
  %sel = extractvalue { ptr, i32 } %lp, 1
  call void @use(i32 %sel)
  ret void
}

; CHECK-LABEL: define void @catch_all(
; CHECK:       lpad:
; CHECK-NEXT:    %lp = landingpad { ptr, i32 }
; CHECK-NEXT:            cleanup
; CHECK-NEXT:            catch ptr null
; CHECK-NEXT:    %exn = extractvalue { ptr, i32 } %lp, 0
; CHECK-NEXT:    [[ALL:%.*]] = call i32 @llvm.eh.typeid.for.p0(ptr null)
; CHECK-NEXT:    [[VAL0:%.*]] = insertvalue { ptr, i32 } poison, ptr %exn, 0
; CHECK-NEXT:    [[VAL:%.*]] = insertvalue { ptr, i32 } [[VAL0]], i32 [[ALL]], 1
; CHECK-NEXT:    %sel = extractvalue { ptr, i32 } [[VAL]], 1
; CHECK-NOT:     __gxx_bpf_personality_v0

; An empty filter lets nothing through, so it is a catch-all too; after a
; clause for a type it is one more entry of the table, which the personality
; reports with a negative selector as any filter's.
define void @catch_then_empty_filter() uwtable personality ptr @__gxx_bpf_personality_v0 {
entry:
  invoke void @may_throw()
          to label %ok unwind label %lpad

ok:
  ret void

lpad:
  %lp = landingpad { ptr, i32 }
          catch ptr @_ZTIi
          filter [0 x ptr] zeroinitializer
  %sel = extractvalue { ptr, i32 } %lp, 1
  call void @use(i32 %sel)
  ret void
}

; CHECK-LABEL: define void @catch_then_empty_filter(
; CHECK:         call void @llvm.bpf.landingpad.index(i32 0)
; CHECK-NEXT:    %lpad.context = call ptr @_Unwind_GetBPFLPadContext() #[[NOUNWIND:[0-9]+]]
; CHECK-NEXT:    store i64 0, ptr %lpad.context, align 8
; CHECK-NEXT:    %lsda = call ptr @llvm.bpf.lsda()
; CHECK-NEXT:    %lsda.slot = getelementptr inbounds { i64, ptr, i64 }, ptr %lpad.context, i32 0, i32 1
; CHECK-NEXT:    store ptr %lsda, ptr %lsda.slot, align 8
; CHECK-NEXT:    call i32 @__gxx_bpf_personality_v0(ptr %exn) #[[NOUNWIND]]
; CHECK-NEXT:    %selector.slot = getelementptr inbounds { i64, ptr, i64 }, ptr %lpad.context, i32 0, i32 2
; CHECK-NEXT:    %selector = load i64, ptr %selector.slot, align 8
; CHECK-NEXT:    [[SEL:%.*]] = trunc i64 %selector to i32
; CHECK-NEXT:    [[VAL0:%.*]] = insertvalue { ptr, i32 } poison, ptr %exn, 0
; CHECK-NEXT:    [[VAL:%.*]] = insertvalue { ptr, i32 } [[VAL0]], i32 [[SEL]], 1
; CHECK-NEXT:    %sel = extractvalue { ptr, i32 } [[VAL]], 1

; What comes after a clause that matches everything is never asked about.
define void @empty_filter_then_catch() uwtable personality ptr @__gxx_bpf_personality_v0 {
entry:
  invoke void @may_throw()
          to label %ok unwind label %lpad

ok:
  ret void

lpad:
  %lp = landingpad { ptr, i32 }
          filter [0 x ptr] zeroinitializer
          catch ptr @_ZTIi
  %sel = extractvalue { ptr, i32 } %lp, 1
  call void @use(i32 %sel)
  ret void
}

; CHECK-LABEL: define void @empty_filter_then_catch(
; CHECK:         %exn = extractvalue { ptr, i32 } %lp, 0
; CHECK-NEXT:    [[VAL0:%.*]] = insertvalue { ptr, i32 } poison, ptr %exn, 0
; CHECK-NEXT:    [[VAL:%.*]] = insertvalue { ptr, i32 } [[VAL0]], i32 -1, 1
; CHECK-NOT:     __gxx_bpf_personality_v0
; CHECK:       {{^ *}}}

; A function that catches everything is nounwind, and its pads compute their
; selectors all the same.
define i32 @nounwind_typed() nounwind uwtable personality ptr @__gxx_bpf_personality_v0 {
entry:
  invoke void @may_throw()
          to label %ok unwind label %lpad

ok:
  ret i32 0

lpad:
  %lp = landingpad { ptr, i32 }
          catch ptr @_ZTIi
          catch ptr @_ZTIl
          catch ptr null
  %sel = extractvalue { ptr, i32 } %lp, 1
  ret i32 %sel
}

; CHECK-LABEL: define i32 @nounwind_typed(
; CHECK:         call void @llvm.bpf.landingpad.index(i32 0)
; CHECK:         call i32 @__gxx_bpf_personality_v0(ptr %exn)
; CHECK-NOT:     llvm.eh.typeid.for

; A pad whose value nothing reads asks nothing, but is entered whatever was
; thrown all the same: it is a cleanup, so that DwarfEHPrepare keeps a resume
; that only a pad with catch clauses reaches.
define void @unused() uwtable personality ptr @__gxx_bpf_personality_v0 {
entry:
  invoke void @may_throw()
          to label %ok unwind label %lpad

ok:
  ret void

lpad:
  %lp = landingpad { ptr, i32 } catch ptr @_ZTIi
  ret void
}

; CHECK-LABEL: define void @unused(
; CHECK:       lpad:
; CHECK-NEXT:    %lp = landingpad { ptr, i32 }
; CHECK-NEXT:            cleanup
; CHECK-NEXT:            catch ptr @_ZTIi
; CHECK-NEXT:    ret void

; Each pad of a function that calls has an index of its own, in order.
define i32 @two_pads(i32 %v) uwtable personality ptr @__gxx_bpf_personality_v0 {
entry:
  invoke void @may_throw()
          to label %next unwind label %lpad

next:
  invoke void @may_throw()
          to label %ok unwind label %lpad1

ok:
  ret i32 0

lpad:
  %lp = landingpad { ptr, i32 } catch ptr @_ZTIi
  %sel = extractvalue { ptr, i32 } %lp, 1
  ret i32 %sel

lpad1:
  %lp1 = landingpad { ptr, i32 } catch ptr @_ZTIl
  %sel1 = extractvalue { ptr, i32 } %lp1, 1
  ret i32 %sel1
}

; CHECK-LABEL: define i32 @two_pads(
; CHECK:       lpad:
; CHECK:         call void @llvm.bpf.landingpad.index(i32 0)
; CHECK:         store i64 0, ptr %lpad.context, align 8
; CHECK:       lpad1:
; CHECK:         call void @llvm.bpf.landingpad.index(i32 1)
; CHECK:         store i64 1, ptr %lpad.context{{[0-9]+}}, align 8

; The calls of the runtime do not unwind, so no unwind_goto follows them.
; CHECK: declare {{.*}}ptr @_Unwind_GetBPFLPadContext() #[[NOUNWIND]]
; CHECK: attributes #[[NOUNWIND]] = { nounwind }
