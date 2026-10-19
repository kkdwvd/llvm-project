; RUN: llc -mtriple=bpfel -filetype=obj -o %t1 %s
; RUN: llvm-objcopy --dump-section='.BTF'=%t2 %t1
; RUN: %python %p/print_btf.py %t2 | FileCheck -check-prefixes=CHECK-BTF %s
; RUN: llvm-readelf -s %t1 | FileCheck -check-prefixes=CHECK-SYM %s

; A C++ inline variable, here n::calls, has linkonce_odr linkage, sits in a
; COMDAT group of its own, and is a weak symbol. It gets a VAR named by its
; symbol, as libbpf's linker needs to match it, and a DATASEC.

$_ZN1n5callsE = comdat any

@_ZN1n5callsE = linkonce_odr dso_local global i64 0, comdat, align 8, !dbg !0

define dso_local i64 @count() !dbg !12 {
  %1 = load i64, ptr @_ZN1n5callsE, align 8
  %2 = add i64 %1, 1
  store i64 %2, ptr @_ZN1n5callsE, align 8
  ret i64 %2
}

; CHECK-BTF: VAR '_ZN1n5callsE' type_id={{[0-9]+}}, linkage=global
; CHECK-BTF: DATASEC '.bss._ZN1n5callsE' size=0 vlen=1
; CHECK-SYM: WEAK {{.*}} _ZN1n5callsE

!llvm.dbg.cu = !{!2}
!llvm.module.flags = !{!10, !11}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(name: "calls", linkageName: "_ZN1n5callsE", scope: !3, file: !4, line: 3, type: !5, isLocal: false, isDefinition: true)
!2 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !4, emissionKind: FullDebug, globals: !6)
!3 = !DINamespace(name: "n", scope: null)
!4 = !DIFile(filename: "inline.cpp", directory: "/some/dir")
!5 = !DIBasicType(name: "unsigned long", size: 64, encoding: DW_ATE_unsigned)
!6 = !{!0}
!10 = !{i32 7, !"Dwarf Version", i32 5}
!11 = !{i32 2, !"Debug Info Version", i32 3}
!12 = distinct !DISubprogram(name: "count", linkageName: "count", scope: !4, file: !4, line: 5, type: !13, scopeLine: 5, spFlags: DISPFlagDefinition, unit: !2)
!13 = !DISubroutineType(types: !14)
!14 = !{!5}
