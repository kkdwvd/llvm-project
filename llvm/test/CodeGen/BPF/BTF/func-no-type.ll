; RUN: llc -mtriple=bpfel -filetype=obj -o %t1 %s
; RUN: llvm-objcopy --dump-section='.BTF'=%t2 %t1
; RUN: %python %p/print_btf.py %t2 | FileCheck -check-prefixes=CHECK-BTF %s

; A subroutine type without elements, not even the return type, is what
; clang gives a function that has no declaration of its own. BTF describes
; such a function as returning void and taking nothing, here a thunk that
; returns int and takes this. The encoder used to compute the number of
; parameters as the number of elements less one, and leave the FUNC without a
; FUNC_PROTO, referring to whatever the uninitialized id held.

define dso_local i32 @_ZThn8_N4Both1mEv(ptr %this) !dbg !5 {
  ret i32 3
}

; CHECK-BTF:      [[[PROTO:[0-9]+]]] FUNC_PROTO '(anon)' ret_type_id=0 vlen=0
; CHECK-BTF-NEXT: [{{[0-9]+}}] FUNC '_ZThn8_N4Both1mEv' type_id=[[PROTO]] linkage=global

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, emissionKind: FullDebug)
!1 = !DIFile(filename: "thunk.cpp", directory: "/some/dir")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!5 = distinct !DISubprogram(linkageName: "_ZThn8_N4Both1mEv", scope: !1, file: !1, line: 1, type: !6, flags: DIFlagArtificial | DIFlagThunk, spFlags: DISPFlagDefinition, unit: !0)
!6 = !DISubroutineType(types: !7)
!7 = !{}
