; RUN: llc -mtriple=bpfel -filetype=obj -o %t1 %s
; RUN: llvm-objcopy --dump-section='.BTF'=%t2 %t1
; RUN: %python %p/print_btf.py %t2 | FileCheck -check-prefixes=CHECK-BTF %s
; RUN: llvm-readelf -s %t1 | FileCheck -check-prefixes=CHECK-SYM %s

; libbpf and the kernel match a FUNC to its ELF symbol: its linkage is the
; symbol's, from the function, not from the subprogram. clang marks a
; subprogram local to its unit as the function starts, before it gives a thunk
; its linkage, so the thunk of a class in an anonymous namespace has a
; subprogram that is not local while its symbol is. The FUNC used to say
; global, and libbpf's linker, which looks a non-static FUNC up among the
; global symbols, dropped it: calls of the thunk then had no BTF to relocate
; by.
;
;   namespace {
;   struct A { virtual ~A() {} int x; };
;   struct B { virtual ~B() {} virtual int b() { return 2; } int y; };
;   struct C : A, B { int b() override { return 3; } };
;   }
;   void *make() { return new C; }

define internal i32 @_ZThn16_N12_GLOBAL__N_11C1bEv(ptr %this) !dbg !10 {
entry:
  ret i32 3, !dbg !15
}

; A subprogram local to its unit on a function that is not: the symbol wins.
define dso_local i32 @global_but_marked_local() !dbg !16 {
entry:
  ret i32 1, !dbg !17
}

; CHECK-SYM: LOCAL {{.*}} _ZThn16_N12_GLOBAL__N_11C1bEv
; CHECK-SYM: GLOBAL {{.*}} global_but_marked_local

; CHECK-BTF: FUNC '_ZThn16_N12_GLOBAL__N_11C1bEv' type_id={{[0-9]+}} linkage=static
; CHECK-BTF: FUNC 'global_but_marked_local' type_id={{[0-9]+}} linkage=global

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, nameTableKind: None)
!1 = !DIFile(filename: "local.cpp", directory: "/tmp")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!5 = !DINamespace(scope: null)
!6 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "B", scope: !5, file: !1, line: 3, size: 128, flags: DIFlagTypePassByReference | DIFlagNonTrivial, elements: !7, vtableHolder: !6, identifier: "_ZTSN12_GLOBAL__N_11BE")
!7 = !{}
!8 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !6, size: 64, flags: DIFlagArtificial | DIFlagObjectPointer)
!10 = distinct !DISubprogram(linkageName: "_ZThn16_N12_GLOBAL__N_11C1bEv", scope: !1, file: !1, line: 4, type: !11, scopeLine: 4, flags: DIFlagArtificial | DIFlagThunk | DIFlagPrototyped, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !13)
!11 = !DISubroutineType(types: !12)
!12 = !{!4, !8}
!13 = !{!14}
!14 = !DILocalVariable(name: "this", arg: 1, scope: !10, type: !8, flags: DIFlagArtificial | DIFlagObjectPointer)
!15 = !DILocation(line: 4, scope: !10)
!16 = distinct !DISubprogram(name: "global_but_marked_local", scope: !1, file: !1, line: 6, type: !18, scopeLine: 6, flags: DIFlagPrototyped, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!17 = !DILocation(line: 6, scope: !16)
!18 = !DISubroutineType(types: !19)
!19 = !{!4}
