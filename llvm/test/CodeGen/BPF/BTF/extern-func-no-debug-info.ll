; RUN: llc -mtriple=bpfel -filetype=obj -o %t1 %s
; RUN: llvm-objcopy --dump-section='.BTF'=%t2 %t1
; RUN: %python %p/print_btf.py %t2 | FileCheck -check-prefixes=CHECK-BTF %s

; clang calls std::terminate() on its own, from the __clang_call_terminate()
; it defines for an exception that would leave a noexcept function, and
; without a declaration of std::terminate() in the unit gives the call no
; debug info. A void(void) callee like it is described so that it has BTF,
; named by its symbol. A callee of another type is not: the IR does not have
; its source types.
define dso_local void @foo() !dbg !7 {
  call void @_ZSt9terminatev()
  call void @other(i32 1)
  ret void
}

declare void @_ZSt9terminatev()
declare void @other(i32)

; CHECK-BTF:     FUNC 'foo' type_id={{[0-9]+}} linkage=global
; CHECK-BTF:     FUNC '_ZSt9terminatev' type_id={{[0-9]+}} linkage=extern
; CHECK-BTF-NOT: FUNC 'other'

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, emissionKind: FullDebug)
!1 = !DIFile(filename: "terminate.cpp", directory: "/some/dir")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!7 = distinct !DISubprogram(name: "foo", linkageName: "foo", scope: !1, file: !1, line: 1, type: !8, scopeLine: 1, spFlags: DISPFlagDefinition, unit: !0)
!8 = !DISubroutineType(types: !9)
!9 = !{null}
