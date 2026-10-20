; RUN: llc -mtriple=bpfel -filetype=obj -o %t1 %s
; RUN: llvm-objcopy --dump-section='.BTF'=%t2 %t1
; RUN: %python %p/print_btf.py %t2 | FileCheck %s

; A function that returns an aggregate too large for the r0:r2 pair returns it
; through the pointer its caller passes in r1, an sret argument, and returns
; void in IR: the ABI's lowering, not an optimization that removed the return
; value. Its BTF keeps the return type of the source, as a declaration of the
; function is described from its subprogram type alone, so that libbpf's
; linker can match the definition in one object against the calls in another,
; and as the kernel and libbpf read a FUNC_PROTO that returns a struct. The
; definition of def_big, the static definition of static_big, the alias
; alias_big of def_big and the declaration of make_big all return struct big.
;
; Source code:
;   struct big { long a, b, c; };
;   struct big make_big(long x);
;   struct big def_big(long x) { struct big r = {x, 0, 0}; return r; }
;   __attribute__((noinline))
;   static struct big static_big(long x) { struct big r = {x, 1, 0}; return r; }
;   struct big alias_big(long x) __attribute__((alias("def_big")));
;   long use(long x) { return make_big(x).a + static_big(x).b; }
; Compilation flags:
;   clang -target bpf -O2 -g -S -emit-llvm t.c

%struct.big = type { i64, i64, i64 }

@alias_big = dso_local alias void (ptr, i64), ptr @def_big

define dso_local void @def_big(ptr noalias sret(%struct.big) align 8 %agg.result, i64 noundef %x) !dbg !12 {
entry:
  store i64 %x, ptr %agg.result, align 8, !dbg !24
  %b = getelementptr inbounds nuw i8, ptr %agg.result, i64 8, !dbg !24
  store i64 0, ptr %b, align 8, !dbg !24
  %c = getelementptr inbounds nuw i8, ptr %agg.result, i64 16, !dbg !24
  store i64 0, ptr %c, align 8, !dbg !24
  ret void, !dbg !25
}

define dso_local i64 @use(i64 noundef %x) !dbg !26 {
entry:
  %ref.tmp = alloca %struct.big, align 8
  %ref.tmp1 = alloca %struct.big, align 8
  call void @make_big(ptr sret(%struct.big) align 8 %ref.tmp, i64 noundef %x), !dbg !30
  %0 = load i64, ptr %ref.tmp, align 8, !dbg !30
  call fastcc void @static_big(ptr sret(%struct.big) align 8 %ref.tmp1, i64 noundef %x), !dbg !30
  %b = getelementptr inbounds nuw i8, ptr %ref.tmp1, i64 8, !dbg !30
  %1 = load i64, ptr %b, align 8, !dbg !30
  %add = add i64 %0, %1, !dbg !30
  ret i64 %add, !dbg !31
}

declare !dbg !32 dso_local void @make_big(ptr sret(%struct.big) align 8, i64 noundef)

define internal fastcc void @static_big(ptr noalias sret(%struct.big) align 8 %agg.result, i64 noundef %x) !dbg !35 {
entry:
  store i64 %x, ptr %agg.result, align 8, !dbg !38
  %b = getelementptr inbounds nuw i8, ptr %agg.result, i64 8, !dbg !38
  store i64 1, ptr %b, align 8, !dbg !38
  %c = getelementptr inbounds nuw i8, ptr %agg.result, i64 16, !dbg !38
  store i64 0, ptr %c, align 8, !dbg !38
  ret void, !dbg !39
}

; CHECK:      [1] INT 'long' size=8 bits_offset=0 nr_bits=64 encoding=SIGNED
; CHECK-NEXT: [2] FUNC_PROTO '(anon)' ret_type_id=3 vlen=1
; CHECK-NEXT:         'x' type_id=1
; CHECK-NEXT: [3] STRUCT 'big' size=24 vlen=3
; CHECK-NEXT:         'a' type_id=1 bits_offset=0
; CHECK-NEXT:         'b' type_id=1 bits_offset=64
; CHECK-NEXT:         'c' type_id=1 bits_offset=128
; CHECK-NEXT: [4] FUNC 'def_big' type_id=2 linkage=global
; CHECK-NEXT: [5] FUNC_PROTO '(anon)' ret_type_id=1 vlen=1
; CHECK-NEXT:         'x' type_id=1
; CHECK-NEXT: [6] FUNC 'use' type_id=5 linkage=global
; CHECK-NEXT: [7] FUNC_PROTO '(anon)' ret_type_id=3 vlen=1
; CHECK-NEXT:         '(anon)' type_id=1
; CHECK-NEXT: [8] FUNC 'make_big' type_id=7 linkage=extern
; CHECK-NEXT: [9] FUNC_PROTO '(anon)' ret_type_id=3 vlen=1
; CHECK-NEXT:         'x' type_id=1
; CHECK-NEXT: [10] FUNC 'static_big' type_id=9 linkage=static
; CHECK-NEXT: [11] FUNC_PROTO '(anon)' ret_type_id=3 vlen=1
; CHECK-NEXT:         'x' type_id=1
; CHECK-NEXT: [12] FUNC 'alias_big' type_id=11 linkage=global

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3}
!llvm.ident = !{!6}

!0 = distinct !DICompileUnit(language: DW_LANG_C11, file: !1, producer: "clang", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "t.c", directory: "/tmp")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!6 = !{!"clang"}
!12 = distinct !DISubprogram(name: "def_big", scope: !1, file: !1, line: 3, type: !13, scopeLine: 3, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !21)
!13 = !DISubroutineType(types: !14)
!14 = !{!15, !18}
!15 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "big", file: !1, line: 1, size: 192, elements: !16)
!16 = !{!17, !19, !20}
!17 = !DIDerivedType(tag: DW_TAG_member, name: "a", scope: !15, file: !1, line: 1, baseType: !18, size: 64)
!18 = !DIBasicType(name: "long", size: 64, encoding: DW_ATE_signed)
!19 = !DIDerivedType(tag: DW_TAG_member, name: "b", scope: !15, file: !1, line: 1, baseType: !18, size: 64, offset: 64)
!20 = !DIDerivedType(tag: DW_TAG_member, name: "c", scope: !15, file: !1, line: 1, baseType: !18, size: 64, offset: 128)
!21 = !{!22, !23}
!22 = !DILocalVariable(name: "x", arg: 1, scope: !12, file: !1, line: 3, type: !18)
!23 = !DILocalVariable(name: "r", scope: !12, file: !1, line: 3, type: !15)
!24 = !DILocation(line: 3, column: 45, scope: !12)
!25 = !DILocation(line: 3, column: 56, scope: !12)
!26 = distinct !DISubprogram(name: "use", scope: !1, file: !1, line: 7, type: !27, scopeLine: 7, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !29)
!27 = !DISubroutineType(types: !28)
!28 = !{!18, !18}
!29 = !{!33}
!30 = !DILocation(line: 7, column: 27, scope: !26)
!31 = !DILocation(line: 7, column: 20, scope: !26)
!32 = !DISubprogram(name: "make_big", scope: !1, file: !1, line: 2, type: !13, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized, retainedNodes: !34)
!33 = !DILocalVariable(name: "x", arg: 1, scope: !26, file: !1, line: 7, type: !18)
!34 = !{!40}
!35 = distinct !DISubprogram(name: "static_big", scope: !1, file: !1, line: 5, type: !13, scopeLine: 5, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !36)
!36 = !{!37, !41}
!37 = !DILocalVariable(name: "x", arg: 1, scope: !35, file: !1, line: 5, type: !18)
!38 = !DILocation(line: 5, column: 81, scope: !35)
!39 = !DILocation(line: 5, column: 92, scope: !35)
!40 = !DILocalVariable(name: "x", arg: 1, scope: !32, file: !1, line: 2, type: !18)
!41 = !DILocalVariable(name: "r", scope: !35, file: !1, line: 5, type: !15)
