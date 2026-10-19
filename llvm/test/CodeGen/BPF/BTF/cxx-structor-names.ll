; RUN: llc -mtriple=bpfel -mcpu=v4 -filetype=asm -o - %s | FileCheck %s

; libbpf matches a function's BTF to its ELF symbol by name. The subprogram of
; a declaration of a constructor or destructor has the linkage name that stands
; for all of its variants (C4, D4), while a call is of one of them (C1, D1):
; the FUNC is named by the symbol of the function called.
;
;   struct Guard {
;     Guard();
;     ~Guard();
;   };
;
;   extern "C" __attribute__((section("tc"), used)) int prog(int *ctx)
;   {
;     Guard g;
;
;     return *ctx;
;   }
;
; clang --target=bpf -mcpu=v4 -O2 -g -S -emit-llvm


%struct.Guard = type { i8 }

@llvm.compiler.used = appending global [1 x ptr] [ptr @prog], section "llvm.metadata"

define dso_local i32 @prog(ptr nofree noundef readonly captures(none) %ctx) section "tc" !dbg !12 {
entry:
  %g = alloca %struct.Guard, align 1, !DIAssignID !21
    #dbg_assign(i1 poison, !19, !DIExpression(), !21, ptr %g, !DIExpression(), !22)
    #dbg_value(ptr %ctx, !18, !DIExpression(), !22)
  call void @llvm.lifetime.start.p0(ptr nonnull %g), !dbg !23
  call void @_ZN5GuardC1Ev(ptr noundef nonnull align 1 dereferenceable(1) %g), !dbg !24
  %0 = load i32, ptr %ctx, align 4, !dbg !25
  call void @_ZN5GuardD1Ev(ptr noundef nonnull align 1 dereferenceable(1) %g), !dbg !27
  call void @llvm.lifetime.end.p0(ptr nonnull %g), !dbg !27
  ret i32 %0, !dbg !28
}

declare void @llvm.lifetime.start.p0(ptr captures(none))

declare !dbg !29 dso_local void @_ZN5GuardC1Ev(ptr noundef nonnull align 1 dereferenceable(1)) unnamed_addr

declare !dbg !33 dso_local void @_ZN5GuardD1Ev(ptr noundef nonnull align 1 dereferenceable(1)) unnamed_addr

declare void @llvm.lifetime.end.p0(ptr captures(none))


!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3, !4, !5}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "guard.cpp", directory: "/tmp")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{i32 7, !"debug-info-assignment-tracking", i1 true}
!12 = distinct !DISubprogram(name: "prog", scope: !1, file: !1, line: 5, type: !13, scopeLine: 6, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !17, keyInstructions: true)
!13 = !DISubroutineType(types: !14)
!14 = !{!15, !16}
!15 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!16 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !15, size: 64)
!17 = !{!18, !19}
!18 = !DILocalVariable(name: "ctx", arg: 1, scope: !12, file: !1, line: 5, type: !16)
!19 = !DILocalVariable(name: "g", scope: !12, file: !1, line: 7, type: !20)
!20 = !DICompositeType(tag: DW_TAG_structure_type, name: "Guard", file: !1, line: 1, size: 8, flags: DIFlagFwdDecl | DIFlagNonTrivial, identifier: "_ZTS5Guard")
!21 = distinct !DIAssignID()
!22 = !DILocation(line: 0, scope: !12)
!23 = !DILocation(line: 7, column: 2, scope: !12)
!24 = !DILocation(line: 7, column: 8, scope: !12)
!25 = !DILocation(line: 8, column: 9, scope: !12, atomGroup: 2, atomRank: 2)
!27 = !DILocation(line: 9, column: 1, scope: !12)
!28 = !DILocation(line: 9, column: 1, scope: !12, atomGroup: 2, atomRank: 1)
!29 = !DISubprogram(name: "Guard", linkageName: "_ZN5GuardC4Ev", scope: !20, file: !1, line: 2, type: !30, scopeLine: 2, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!30 = !DISubroutineType(types: !31)
!31 = !{null, !32}
!32 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !20, size: 64, flags: DIFlagArtificial | DIFlagObjectPointer)
!33 = !DISubprogram(name: "~Guard", linkageName: "_ZN5GuardD4Ev", scope: !20, file: !1, line: 3, type: !30, scopeLine: 3, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)

; CHECK:       .ascii  "_ZN5GuardC1Ev"
; CHECK:       .ascii  "_ZN5GuardD1Ev"
; CHECK-NOT:   C4Ev
; CHECK-NOT:   D4Ev
; CHECK:       .section .BTF.ext
