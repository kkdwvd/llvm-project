; RUN: llc -mtriple=bpfel -mcpu=v4 -filetype=obj -o %t1 %s
; RUN: llvm-objcopy --dump-section='.BTF'=%t2 %t1
; RUN: %python %p/print_btf.py %t2 | FileCheck -check-prefixes=CHECK-BTF %s

; A pad with a catch clause for a type calls the personality routine, and
; before it the accessor of the landing pad context, two functions the backend
; declares (BPFUnwind.cpp). libbpf links the calls to the definitions in
; libc++abi and libunwind through BTF, so the declarations have the C
; prototypes of the definitions, as the backend's other libcalls have:
;   int __gxx_bpf_personality_v0(void *exception_object);
;   struct _Unwind_LandingPadContext *_Unwind_GetBPFLPadContext(void);
; the struct a forward declaration, which libbpf matches to the definition's.
;
;   extern "C" void may_throw();
;   extern "C" int f()
;   {
;           try {
;                   may_throw();
;           } catch (int) {
;                   return 1;
;           }
;           return 0;
;   }

@_ZTIi = external constant ptr

define i32 @f() uwtable personality ptr @__gxx_bpf_personality_v0 !dbg !5 {
entry:
  invoke void @may_throw()
          to label %return unwind label %lpad, !dbg !9

lpad:
  %0 = landingpad { ptr, i32 }
          catch ptr @_ZTIi, !dbg !10
  %1 = extractvalue { ptr, i32 } %0, 1, !dbg !10
  %2 = tail call i32 @llvm.eh.typeid.for.p0(ptr nonnull @_ZTIi) nounwind, !dbg !10
  %matches = icmp eq i32 %1, %2, !dbg !10
  br i1 %matches, label %catch, label %eh.resume, !dbg !10

catch:
  %3 = extractvalue { ptr, i32 } %0, 0, !dbg !10
  %4 = tail call ptr @__cxa_begin_catch(ptr %3) nounwind, !dbg !10
  tail call void @__cxa_end_catch() nounwind, !dbg !10
  br label %return

return:
  %retval.0 = phi i32 [ 1, %catch ], [ 0, %entry ], !dbg !10
  ret i32 %retval.0, !dbg !10

eh.resume:
  resume { ptr, i32 } %0, !dbg !10
}

declare !dbg !11 void @may_throw()
declare i32 @__gxx_bpf_personality_v0(ptr)
declare i32 @llvm.eh.typeid.for.p0(ptr) nounwind memory(none)
declare ptr @__cxa_begin_catch(ptr)
declare void @__cxa_end_catch()

; CHECK-BTF:      [[[INT:[0-9]+]]] INT 'int' size=4 bits_offset=0 nr_bits=32 encoding=SIGNED
; CHECK-BTF:      FUNC 'may_throw'
; CHECK-BTF-NEXT: [[[CTXPROTO:[0-9]+]]] FUNC_PROTO '(anon)' ret_type_id=[[CTXPTR:[0-9]+]] vlen=0
; CHECK-BTF-NEXT: [[[CTXPTR]]] PTR '(anon)' type_id=[[CTX:[0-9]+]]
; CHECK-BTF-NEXT: [[[CTX]]] FWD '_Unwind_LandingPadContext' fwd_kind=struct
; CHECK-BTF-NEXT: [{{[0-9]+}}] FUNC '_Unwind_GetBPFLPadContext' type_id=[[CTXPROTO]] linkage=extern
; CHECK-BTF-NEXT: [[[PERSPROTO:[0-9]+]]] FUNC_PROTO '(anon)' ret_type_id=[[INT]] vlen=1
; CHECK-BTF-NEXT:         '(anon)' type_id=[[VOIDPTR:[0-9]+]]
; CHECK-BTF-NEXT: [[[VOIDPTR]]] PTR '(anon)' type_id=0
; CHECK-BTF-NEXT: [{{[0-9]+}}] FUNC '__gxx_bpf_personality_v0' type_id=[[PERSPROTO]] linkage=extern

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "t.cpp", directory: "/tmp")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!5 = distinct !DISubprogram(name: "f", scope: !1, file: !1, line: 2, type: !6, scopeLine: 3, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!6 = !DISubroutineType(types: !7)
!7 = !{!4}
!8 = distinct !DILexicalBlock(scope: !5, file: !1, line: 4, column: 6)
!9 = !DILocation(line: 5, column: 3, scope: !8)
!10 = !DILocation(line: 6, column: 2, scope: !8)
!11 = !DISubprogram(name: "may_throw", scope: !1, file: !1, line: 1, type: !12, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!12 = !DISubroutineType(types: !13)
!13 = !{null}
