; RUN: llc -mtriple=bpfel -mcpu=v4 -verify-machineinstrs -o - %s | FileCheck %s
; RUN: llc -mtriple=bpfel -mcpu=v4 -enable-new-pm -verify-machineinstrs -o - %s | FileCheck %s
; RUN: llc -mtriple=bpfel -mcpu=v4 -filetype=obj -o %t1 %s
; RUN: llvm-objcopy --dump-section='.BTF'=%t2 %t1
; RUN: %python %p/print_btf.py %t2 | FileCheck -check-prefix=CHECK-BTF %s

; An exception that would leave a noexcept function calls
; __clang_call_terminate(), the helper clang defines in each translation unit
; on every target: it begins catching the exception, so that a terminate
; handler sees it through std::current_exception(), then calls
; std::terminate(). clang gives the helper an artificial subprogram with its
; prototype, which BTF describes as it describes any function, so that libbpf
; links the pad's call to it and the kernel has func_info for it. The pad
; passes it the exception object, which is in r0 when the pad is entered.
; The IR is clang's, from
;
;   clang++ --target=bpf -mcpu=v4 -O2 -g
;
; of
;
;   void may_throw();
;   void no_throw() noexcept { may_throw(); }
;
; with fewer attributes.

$__clang_call_terminate = comdat any

define dso_local void @_Z8no_throwv() uwtable personality ptr @__gxx_personality_v0 !dbg !7 {
entry:
  invoke void @_Z9may_throwv()
          to label %invoke.cont unwind label %terminate.lpad, !dbg !10

invoke.cont:
  ret void, !dbg !10

terminate.lpad:
  %0 = landingpad { ptr, i32 }
          catch ptr null, !dbg !10
  %1 = extractvalue { ptr, i32 } %0, 0, !dbg !10
  tail call void @__clang_call_terminate(ptr %1) #1, !dbg !10
  unreachable, !dbg !10
}

; CHECK-LABEL: _Z8no_throwv:
; CHECK:         call _Z9may_throwv
; CHECK-NEXT:    unwind_goto [[PAD:.LBB[0-9_]+]]
; CHECK:       [[PAD]]:
; CHECK-NEXT:    r1 = r0
; CHECK-NEXT:    call __clang_call_terminate

define linkonce_odr hidden void @__clang_call_terminate(ptr noundef %0) #0 comdat !dbg !11 {
  %2 = tail call ptr @__cxa_begin_catch(ptr %0) #2, !dbg !15
  tail call void @_ZSt9terminatev() #1, !dbg !15
  unreachable, !dbg !15
}

; CHECK-LABEL: __clang_call_terminate:
; CHECK:         call __cxa_begin_catch
; CHECK:         call _ZSt9terminatev

declare !dbg !16 dso_local void @_Z9may_throwv()
declare dso_local i32 @__gxx_personality_v0(...)
declare !dbg !17 dso_local ptr @__cxa_begin_catch(ptr)
declare dso_local void @_ZSt9terminatev()

; The helper has a FUNC with the prototype of its subprogram, void (char *),
; and the runtime functions it calls have theirs: __cxa_begin_catch() from
; its declaration, std::terminate() as a void(void) callee without one.
; CHECK-BTF:      [[[PROTO:[0-9]+]]] FUNC_PROTO '(anon)' ret_type_id=0 vlen=1
; CHECK-BTF-NEXT:         '(anon)' type_id=[[CHARP:[0-9]+]]
; CHECK-BTF-NEXT: [[[CHARP]]] PTR '(anon)' type_id=[[CHAR:[0-9]+]]
; CHECK-BTF-NEXT: [[[CHAR]]] INT 'char' size=1 bits_offset=0 nr_bits=8 encoding=SIGNED
; CHECK-BTF-NEXT: [{{[0-9]+}}] FUNC '__clang_call_terminate' type_id=[[PROTO]] linkage=global
; CHECK-BTF:      FUNC '__cxa_begin_catch' type_id={{[0-9]+}} linkage=extern
; CHECK-BTF:      FUNC '_ZSt9terminatev' type_id={{[0-9]+}} linkage=extern

attributes #0 = { noinline noreturn nounwind }
attributes #1 = { noreturn nounwind }
attributes #2 = { nounwind }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3, !4}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, isOptimized: true, emissionKind: FullDebug)
!1 = !DIFile(filename: "noexcept.cpp", directory: "/some/dir")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !{i32 7, !"uwtable", i32 1}
!7 = distinct !DISubprogram(name: "no_throw", linkageName: "_Z8no_throwv", scope: !1, file: !1, line: 2, type: !8, scopeLine: 2, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!8 = !DISubroutineType(types: !9)
!9 = !{null}
!10 = !DILocation(line: 2, column: 28, scope: !7)
!11 = distinct !DISubprogram(linkageName: "__clang_call_terminate", scope: !1, file: !1, type: !12, flags: DIFlagArtificial, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!12 = !DISubroutineType(types: !13)
!13 = !{null, !14}
!14 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !18, size: 64)
!15 = !DILocation(line: 0, scope: !11)
!16 = !DISubprogram(name: "may_throw", linkageName: "_Z9may_throwv", scope: !1, file: !1, line: 1, type: !8, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!17 = !DISubprogram(name: "__cxa_begin_catch", scope: !1, file: !1, line: 1, type: !19, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!18 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!19 = !DISubroutineType(types: !20)
!20 = !{!21, !21}
!21 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: null, size: 64)
