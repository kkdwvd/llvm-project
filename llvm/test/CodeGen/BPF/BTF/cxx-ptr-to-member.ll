; RUN: llc -mtriple=bpfel -filetype=obj -o %t1 %s
; RUN: llvm-objcopy --dump-section='.BTF'=%t2 %t1
; RUN: %python %p/print_btf.py %t2 | FileCheck -check-prefixes=CHECK-BTF %s

; C++ has two scalars that BTF has no kind for and C no type: a pointer to a
; member, and std::nullptr_t. A pointer to a data member is the member's
; offset, a ptrdiff_t by the Itanium C++ ABI; a pointer to a member function
; is two words, the function's address or vtable offset and the this
; adjustment. Either travels in registers as an integer of its size does,
; and is described as one, signed as a ptrdiff_t is. std::nullptr_t is
; DWARF's unspecified type, without a size in the debug info: a null pointer,
; a scalar of the pointer's size. The encoder used to add no type for any of
; them and assert on the lookup.
;
;   struct M { int i; int f(); };
;   int M::* h(int M::* p) { return p; }
;   int (M::* g(int (M::* q)()))() { return q; }
;   decltype(nullptr) k(decltype(nullptr) n) { return n; }

define dso_local i64 @_Z1hM1Mi(i64 returned %p) !dbg !10 {
entry:
  ret i64 %p, !dbg !19
}

define dso_local { i64, i64 } @_Z1gM1MFivE(i64 %q.coerce0, i64 %q.coerce1) !dbg !20 {
entry:
  %0 = insertvalue { i64, i64 } poison, i64 %q.coerce0, 0, !dbg !31
  %1 = insertvalue { i64, i64 } %0, i64 %q.coerce1, 1, !dbg !31
  ret { i64, i64 } %1, !dbg !31
}

define dso_local ptr @_Z1kDn(ptr %n) !dbg !32 {
entry:
  ret ptr null, !dbg !37
}

; CHECK-BTF:      [[[PTRDIFF:[0-9]+]]] INT 'long' size=8 bits_offset=0 nr_bits=64 encoding=SIGNED
; CHECK-BTF-NEXT: [[[P1:[0-9]+]]] FUNC_PROTO '(anon)' ret_type_id=[[PTRDIFF]] vlen=1
; CHECK-BTF-NEXT:         'p' type_id=[[PTRDIFF]]
; CHECK-BTF-NEXT: [{{[0-9]+}}] FUNC '_Z1hM1Mi' type_id=[[P1]] linkage=global
; CHECK-BTF-NEXT: [[[PAIR:[0-9]+]]] INT '__int128' size=16 bits_offset=0 nr_bits=128 encoding=SIGNED
; CHECK-BTF-NEXT: [[[P2:[0-9]+]]] FUNC_PROTO '(anon)' ret_type_id=[[PAIR]] vlen=1
; CHECK-BTF-NEXT:         'q' type_id=[[PAIR]]
; CHECK-BTF-NEXT: [{{[0-9]+}}] FUNC '_Z1gM1MFivE' type_id=[[P2]] linkage=global
; CHECK-BTF-NEXT: [[[NULLPTR:[0-9]+]]] INT 'decltype(nullptr)' size=8 bits_offset=0 nr_bits=64 encoding=(none)
; CHECK-BTF-NEXT: [[[P3:[0-9]+]]] FUNC_PROTO '(anon)' ret_type_id=[[NULLPTR]] vlen=1
; CHECK-BTF-NEXT:         'n' type_id=[[NULLPTR]]
; CHECK-BTF-NEXT: [{{[0-9]+}}] FUNC '_Z1kDn' type_id=[[P3]] linkage=global

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, nameTableKind: None)
!1 = !DIFile(filename: "pm.cpp", directory: "/tmp")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!5 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "M", file: !1, line: 1, size: 32, flags: DIFlagTypePassByValue, elements: !6, identifier: "_ZTS1M")
!6 = !{!7}
!7 = !DIDerivedType(tag: DW_TAG_member, name: "i", scope: !5, file: !1, line: 1, baseType: !4, size: 32)
!10 = distinct !DISubprogram(name: "h", linkageName: "_Z1hM1Mi", scope: !1, file: !1, line: 2, type: !11, scopeLine: 2, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !16)
!11 = !DISubroutineType(types: !12)
!12 = !{!13, !13}
!13 = !DIDerivedType(tag: DW_TAG_ptr_to_member_type, baseType: !4, size: 64, extraData: !5)
!16 = !{!17}
!17 = !DILocalVariable(name: "p", arg: 1, scope: !10, file: !1, line: 2, type: !13)
!19 = !DILocation(line: 2, column: 26, scope: !10)
!20 = distinct !DISubprogram(name: "g", linkageName: "_Z1gM1MFivE", scope: !1, file: !1, line: 3, type: !21, scopeLine: 3, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !28)
!21 = !DISubroutineType(types: !22)
!22 = !{!23, !23}
!23 = !DIDerivedType(tag: DW_TAG_ptr_to_member_type, baseType: !24, size: 128, extraData: !5)
!24 = !DISubroutineType(types: !25)
!25 = !{!4, !26}
!26 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !5, size: 64, flags: DIFlagArtificial | DIFlagObjectPointer)
!28 = !{!29}
!29 = !DILocalVariable(name: "q", arg: 1, scope: !20, file: !1, line: 3, type: !23)
!31 = !DILocation(line: 3, column: 32, scope: !20)
!32 = distinct !DISubprogram(name: "k", linkageName: "_Z1kDn", scope: !1, file: !1, line: 4, type: !33, scopeLine: 4, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !35)
!33 = !DISubroutineType(types: !34)
!34 = !{!8, !8}
!8 = !DIBasicType(tag: DW_TAG_unspecified_type, name: "decltype(nullptr)")
!35 = !{!36}
!36 = !DILocalVariable(name: "n", arg: 1, scope: !32, file: !1, line: 4, type: !8)
!37 = !DILocation(line: 4, column: 40, scope: !32)
