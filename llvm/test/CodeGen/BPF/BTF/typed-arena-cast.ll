; RUN: opt -O2 -mtriple=bpf-pc-linux -S -o %t1 %s
; RUN: llc -filetype=asm -show-mc-encoding -o - %t1 | FileCheck %s
;
; Source:
;   struct node { struct task_struct *__kptr task; long value; };
;   long read(unsigned long v) {
;     return ((struct node *)__builtin_bpf_typed_arena_cast(v, *(struct node *)0))->value;
;   }
;   void write(unsigned long v, long x) {
;     ((struct node *)__builtin_bpf_typed_arena_cast(v, *(struct node *)0))->value = x;
;   }
; Compilation flags:
;   clang -target bpf -O2 -g -S -emit-llvm -Xclang -disable-llvm-passes t.c

%struct.node = type { ptr, i64 }

define dso_local i64 @read(i64 %v) !dbg !7 {
entry:
  %0 = call ptr @llvm.bpf.typed.arena.cast(i64 %v, ptr null), !dbg !14, !llvm.preserve.access.index !12
  %value = getelementptr inbounds %struct.node, ptr %0, i32 0, i32 1, !dbg !14
  %1 = load i64, ptr %value, align 8, !dbg !14
  ret i64 %1, !dbg !15
}

define dso_local void @write(i64 %v, i64 %x) !dbg !22 {
entry:
  %0 = call ptr @llvm.bpf.typed.arena.cast(i64 %v, ptr null), !dbg !23, !llvm.preserve.access.index !12
  %value = getelementptr inbounds %struct.node, ptr %0, i32 0, i32 1, !dbg !23
  store i64 %x, ptr %value, align 8, !dbg !23
  ret void, !dbg !24
}

; CHECK-LABEL: read:
; CHECK:       r1 = typed_arena_cast(r1, [[ID:[0-9]+]]) # encoding: [0xbf,0x11,0x02,0x00,0x{{0[0-9a-f]}},0x00,0x00,0x00]
; CHECK-NEXT:  r0 = *(u64 *)(r1 + 8)

; CHECK-LABEL: write:
; CHECK:       r1 = typed_arena_cast(r1, [[ID]])
; CHECK-NEXT:  *(u64 *)(r1 + 8) = r2

; The local type ID of struct node is relocated into the imm of each cast.
; CHECK:       .long   16                              # FieldReloc
; CHECK-NEXT:  .long   {{[0-9]+}}                      # Field reloc section string offset={{[0-9]+}}
; CHECK-NEXT:  .long   2
; CHECK-NEXT:  .long   .Ltmp{{[0-9]+}}
; CHECK-NEXT:  .long   [[ID]]
; CHECK-NEXT:  .long   {{[0-9]+}}
; CHECK-NEXT:  .long   6
; CHECK-NEXT:  .long   .Ltmp{{[0-9]+}}
; CHECK-NEXT:  .long   [[ID]]
; CHECK-NEXT:  .long   {{[0-9]+}}
; CHECK-NEXT:  .long   6

declare ptr @llvm.bpf.typed.arena.cast(i64, ptr)

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4, !5}
!llvm.ident = !{!6}

!0 = distinct !DICompileUnit(language: DW_LANG_C11, file: !1, producer: "clang", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, retainedTypes: !2, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "t.c", directory: "/tmp")
!2 = !{!12}
!3 = !{i32 7, !"Dwarf Version", i32 5}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{i32 1, !"wchar_size", i32 4}
!6 = !{!"clang"}
!7 = distinct !DISubprogram(name: "read", scope: !1, file: !1, line: 2, type: !8, scopeLine: 2, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!8 = !DISubroutineType(types: !9)
!9 = !{!10, !11}
!10 = !DIBasicType(name: "long", size: 64, encoding: DW_ATE_signed)
!11 = !DIBasicType(name: "unsigned long", size: 64, encoding: DW_ATE_unsigned)
!12 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "node", file: !1, line: 1, size: 128, elements: !13)
!13 = !{!16, !19}
!14 = !DILocation(line: 3, column: 5, scope: !7)
!15 = !DILocation(line: 3, column: 3, scope: !7)
!16 = !DIDerivedType(tag: DW_TAG_member, name: "task", scope: !12, file: !1, line: 1, baseType: !17, size: 64)
!17 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !18, size: 64, annotations: !20)
!18 = !DICompositeType(tag: DW_TAG_structure_type, name: "task_struct", file: !1, line: 1, flags: DIFlagFwdDecl)
!19 = !DIDerivedType(tag: DW_TAG_member, name: "value", scope: !12, file: !1, line: 1, baseType: !10, size: 64, offset: 64)
!20 = !{!21}
!21 = !{!"btf_type_tag", !"kptr"}
!22 = distinct !DISubprogram(name: "write", scope: !1, file: !1, line: 5, type: !25, scopeLine: 5, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!23 = !DILocation(line: 6, column: 5, scope: !22)
!24 = !DILocation(line: 7, column: 1, scope: !22)
!25 = !DISubroutineType(types: !26)
!26 = !{null, !11, !10}
