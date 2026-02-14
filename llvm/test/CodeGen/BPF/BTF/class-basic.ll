; RUN: llc -mtriple=bpfel -filetype=asm -o - %s | FileCheck -check-prefixes=CHECK %s
; RUN: llc -mtriple=bpfeb -filetype=asm -o - %s | FileCheck -check-prefixes=CHECK %s

; Source code:
;   class Foo {
;     int x;
;   public:
;     int get() const { return x; }
;   };
;   Foo f;
;
; Verify that BTF encodes a C++ class as a struct with only data members,
; skipping methods.

%class.Foo = type { i32 }

@f = dso_local global %class.Foo zeroinitializer, align 4, !dbg !0

!llvm.dbg.cu = !{!2}
!llvm.module.flags = !{!14, !15, !16}
!llvm.ident = !{!17}

; The class should appear as BTF_KIND_STRUCT with 1 member (x only, not get).
; CHECK:             .long   1                       # BTF_KIND_STRUCT(id = 1)
; CHECK-NEXT:        .long   67108865                # 0x4000001
; CHECK-NEXT:        .long   4
; CHECK-NEXT:        .long   5
; CHECK-NEXT:        .long   2
; CHECK-NEXT:        .long   0                       # 0x0

; Verify the string "Foo" appears in BTF strings.
; CHECK:             .ascii  "Foo"
; CHECK-NEXT:        .byte   0

; Verify the string "x" appears.
; CHECK:             .byte   120                     # string offset=5
; CHECK-NEXT:        .byte   0

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(name: "f", scope: !2, file: !3, line: 8, type: !6, isLocal: false, isDefinition: true)
!2 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !3, producer: "clang", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, globals: !5, nameTableKind: None)
!3 = !DIFile(filename: "test.cpp", directory: "/tmp")
!4 = !{}
!5 = !{!0}
!6 = distinct !DICompositeType(tag: DW_TAG_class_type, name: "Foo", file: !3, line: 1, size: 32, elements: !7, identifier: "_ZTS3Foo")
!7 = !{!8, !10}
!8 = !DIDerivedType(tag: DW_TAG_member, name: "x", scope: !6, file: !3, line: 2, baseType: !9, size: 32)
!9 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!10 = !DISubprogram(name: "get", linkageName: "_ZNK3Foo3getEv", scope: !6, file: !3, line: 4, type: !11, scopeLine: 4, flags: DIFlagPublic | DIFlagPrototyped, spFlags: 0)
!11 = !DISubroutineType(types: !12)
!12 = !{!9, !13}
!13 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !6, size: 64, flags: DIFlagArtificial | DIFlagObjectPointer)
!14 = !{i32 2, !"Dwarf Version", i32 5}
!15 = !{i32 2, !"Debug Info Version", i32 3}
!16 = !{i32 1, !"wchar_size", i32 4}
!17 = !{!"clang"}
