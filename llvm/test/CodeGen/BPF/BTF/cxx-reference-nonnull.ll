; RUN: llc -mtriple=bpfel -filetype=asm -o - %s | FileCheck %s
; RUN: llc -mtriple=bpfeb -filetype=asm -o - %s | FileCheck %s
;
; A reference parameter and the object parameter of a member function are
; never null: a global function's gets the arg:nonnull decl tag, so that the
; kernel types it as a pointer the caller must pass a valid object for, not
; as memory that may be null. A static or a hidden function, which the kernel
; verifies in each caller's context, a program, whose parameter is its
; context, and a function compiled with -fno-delete-null-pointer-checks get
; none; a parameter that carries the tag itself keeps the one it has.
;
; Source code:
;   #define __tag(x) __attribute__((btf_decl_tag(x)))
;   struct S { long a, b, c; };
;   typedef const S &SRef;
;   struct M { long v; long get() const; };
;   struct xdp_md { unsigned data, data_end; };
;
;   long by_ref(const S &s) { return s.a; }
;   long by_rvalue_ref(S &&s) { return s.b; }
;   long by_typedef_ref(SRef s) { return s.c; }
;   long tagged(const S &s __tag("arg:nonnull")) { return s.a; }
;   long mixed(long x, S *p, const S &s) { return x + p->a + s.a; }
;   long M::get() const { return v; }
;   S make(long x, const S &s) { S r = {x, s.b, s.c}; return r; }
;   __attribute__((noinline)) static long by_ref_static(const S &s) { return s.b; }
;   __attribute__((visibility("hidden"))) long by_ref_hidden(const S &s) { return s.c; }
;   long calls(const S &s) { return by_ref_static(s) + by_ref_hidden(s); }
;   __attribute__((section("xdp"))) int prog(xdp_md &ctx) { return ctx.data; }
; Compilation flag:
;   clang++ --target=bpf -O2 -g -S -emit-llvm nonnull.cpp
; with by_ref_npv, below, added from a compilation with
; -fno-delete-null-pointer-checks.

target datalayout = "e-m:e-p:64:64-i64:64-i128:128-n32:64-S128"
target triple = "bpf"

%struct.S = type { i64, i64, i64 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(argmem: read)
define dso_local noundef i64 @_Z6by_refRK1S(ptr nofree noundef nonnull readonly align 8 captures(none) dereferenceable(24) %s) local_unnamed_addr #0 !dbg !12 {
entry:
    #dbg_value(ptr %s, !25, !DIExpression(), !26)
  %0 = load i64, ptr %s, align 8, !dbg !27, !tbaa !28
  ret i64 %0, !dbg !31
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(argmem: read)
define dso_local noundef i64 @_Z13by_rvalue_refO1S(ptr nofree noundef nonnull readonly align 8 captures(none) dereferenceable(24) %s) local_unnamed_addr #0 !dbg !32 {
entry:
    #dbg_value(ptr %s, !37, !DIExpression(), !38)
  %b = getelementptr inbounds nuw i8, ptr %s, i64 8, !dbg !39
  %0 = load i64, ptr %b, align 8, !dbg !40, !tbaa !41
  ret i64 %0, !dbg !42
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(argmem: read)
define dso_local noundef i64 @_Z14by_typedef_refRK1S(ptr nofree noundef nonnull readonly align 8 captures(none) dereferenceable(24) %s) local_unnamed_addr #0 !dbg !43 {
entry:
    #dbg_value(ptr %s, !48, !DIExpression(), !49)
  %c = getelementptr inbounds nuw i8, ptr %s, i64 16, !dbg !50
  %0 = load i64, ptr %c, align 8, !dbg !51, !tbaa !52
  ret i64 %0, !dbg !53
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(argmem: read)
define dso_local noundef i64 @_Z6taggedRK1S(ptr nofree noundef nonnull readonly align 8 captures(none) dereferenceable(24) %s) local_unnamed_addr #0 !dbg !54 {
entry:
    #dbg_value(ptr %s, !56, !DIExpression(), !59)
  %0 = load i64, ptr %s, align 8, !dbg !60, !tbaa !28
  ret i64 %0, !dbg !61
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(argmem: read)
define dso_local noundef i64 @_Z5mixedlP1SRKS_(i64 noundef %x, ptr nofree noundef readonly captures(none) %p, ptr nofree noundef nonnull readonly align 8 captures(none) dereferenceable(24) %s) local_unnamed_addr #0 !dbg !62 {
entry:
    #dbg_value(i64 %x, !67, !DIExpression(), !70)
    #dbg_value(ptr %p, !68, !DIExpression(), !70)
    #dbg_value(ptr %s, !69, !DIExpression(), !70)
  %0 = load i64, ptr %p, align 8, !dbg !71, !tbaa !28
  %add = add nsw i64 %0, %x, !dbg !72
  %1 = load i64, ptr %s, align 8, !dbg !73, !tbaa !28
  %add2 = add nsw i64 %add, %1, !dbg !74
  ret i64 %add2, !dbg !75
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(argmem: read)
define dso_local noundef i64 @_ZNK1M3getEv(ptr nofree noundef nonnull readonly align 8 captures(none) dereferenceable(8) %this) local_unnamed_addr #0 align 2 !dbg !76 {
entry:
    #dbg_value(ptr %this, !86, !DIExpression(), !88)
  %0 = load i64, ptr %this, align 8, !dbg !89, !tbaa !90
  ret i64 %0, !dbg !92
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(argmem: readwrite)
define dso_local void @_Z4makelRK1S(ptr dead_on_unwind noalias nofree writable writeonly sret(%struct.S) align 8 captures(none) initializes((0, 24)) %agg.result, i64 noundef %x, ptr nofree noundef nonnull readonly align 8 captures(none) dereferenceable(24) %s) local_unnamed_addr #1 !dbg !93 {
entry:
    #dbg_value(i64 %x, !97, !DIExpression(), !100)
    #dbg_value(ptr %s, !98, !DIExpression(), !100)
    #dbg_declare(ptr %agg.result, !99, !DIExpression(), !101)
  store i64 %x, ptr %agg.result, align 8, !dbg !102, !tbaa !28
  %b = getelementptr inbounds nuw i8, ptr %agg.result, i64 8, !dbg !103
  %b1 = getelementptr inbounds nuw i8, ptr %s, i64 8, !dbg !104
  %0 = load i64, ptr %b1, align 8, !dbg !105, !tbaa !41
  store i64 %0, ptr %b, align 8, !dbg !102, !tbaa !41
  %c = getelementptr inbounds nuw i8, ptr %agg.result, i64 16, !dbg !103
  %c2 = getelementptr inbounds nuw i8, ptr %s, i64 16, !dbg !106
  %1 = load i64, ptr %c2, align 8, !dbg !107, !tbaa !52
  store i64 %1, ptr %c, align 8, !dbg !102, !tbaa !52
  ret void, !dbg !108
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(argmem: read)
define hidden noundef i64 @_Z13by_ref_hiddenRK1S(ptr nofree noundef nonnull readonly align 8 captures(none) dereferenceable(24) %s) local_unnamed_addr #0 !dbg !109 {
entry:
    #dbg_value(ptr %s, !111, !DIExpression(), !112)
  %c = getelementptr inbounds nuw i8, ptr %s, i64 16, !dbg !113
  %0 = load i64, ptr %c, align 8, !dbg !114, !tbaa !52
  ret i64 %0, !dbg !115
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(argmem: read)
define dso_local noundef i64 @_Z5callsRK1S(ptr nofree noundef nonnull readonly align 8 captures(none) dereferenceable(24) %s) local_unnamed_addr #0 !dbg !116 {
entry:
    #dbg_value(ptr %s, !118, !DIExpression(), !119)
  %call = tail call fastcc noundef i64 @_ZL13by_ref_staticRK1S(ptr noundef nonnull align 8 dereferenceable(24) %s), !dbg !120
    #dbg_value(ptr %s, !111, !DIExpression(), !121)
  %c.i = getelementptr inbounds nuw i8, ptr %s, i64 16, !dbg !123
  %0 = load i64, ptr %c.i, align 8, !dbg !124, !tbaa !52
  %add = add nsw i64 %0, %call, !dbg !125
  ret i64 %add, !dbg !126
}

; Function Attrs: mustprogress nofree noinline norecurse nosync nounwind willreturn memory(argmem: read)
define internal fastcc noundef i64 @_ZL13by_ref_staticRK1S(ptr nofree noundef nonnull readonly align 8 captures(none) dereferenceable(24) %s) unnamed_addr #2 !dbg !127 {
entry:
    #dbg_value(ptr %s, !129, !DIExpression(), !130)
  %b = getelementptr inbounds nuw i8, ptr %s, i64 8, !dbg !131
  %0 = load i64, ptr %b, align 8, !dbg !132, !tbaa !41
  ret i64 %0, !dbg !133
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(argmem: read)
define dso_local noundef i32 @_Z4progR6xdp_md(ptr nofree noundef nonnull readonly align 4 captures(none) dereferenceable(8) %ctx) local_unnamed_addr #0 section "xdp" !dbg !134 {
entry:
    #dbg_value(ptr %ctx, !145, !DIExpression(), !146)
  %0 = load i32, ptr %ctx, align 4, !dbg !147, !tbaa !148
  ret i32 %0, !dbg !150
}

; by_ref compiled with -fno-delete-null-pointer-checks: clang marks the
; reference nonnull in IR no more, and the function says so.
; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(argmem: read) null_pointer_is_valid
define dso_local noundef i64 @_Z10by_ref_npvRK1S(ptr nofree noundef readonly align 8 captures(none) dereferenceable(24) %s) local_unnamed_addr #3 !dbg !151 {
entry:
    #dbg_value(ptr %s, !152, !DIExpression(), !154)
  %0 = load i64, ptr %s, align 8, !dbg !155, !tbaa !28
  ret i64 %0, !dbg !155
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind willreturn memory(argmem: read) "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" }
attributes #1 = { mustprogress nofree norecurse nosync nounwind willreturn memory(argmem: readwrite) "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" }
attributes #3 = { mustprogress nofree norecurse nosync nounwind willreturn memory(argmem: read) null_pointer_is_valid "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" }
attributes #2 = { mustprogress nofree noinline norecurse nosync nounwind willreturn memory(argmem: read) "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3, !4, !5}
!llvm.ident = !{!6}
!llvm.errno.tbaa = !{!7}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "nonnull.cpp", directory: "", checksumkind: CSK_MD5, checksum: "10a09294a9d54ee33f8442401f31289b")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{i32 7, !"debug-info-assignment-tracking", i1 true}
!6 = !{!"clang"}
!7 = !{!8, !9, i64 0}
!8 = !{!"__libc_errno", !9, i64 0}
!9 = !{!"int", !10, i64 0}
!10 = !{!"omnipotent char", !11, i64 0}
!11 = !{!"Simple C++ TBAA"}
!12 = distinct !DISubprogram(name: "by_ref", linkageName: "_Z6by_refRK1S", scope: !13, file: !13, line: 7, type: !14, scopeLine: 7, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !24, keyInstructions: true)
!13 = !DIFile(filename: "nonnull.cpp", directory: "", checksumkind: CSK_MD5, checksum: "10a09294a9d54ee33f8442401f31289b")
!14 = !DISubroutineType(types: !15)
!15 = !{!16, !17}
!16 = !DIBasicType(name: "long", size: 64, encoding: DW_ATE_signed)
!17 = !DIDerivedType(tag: DW_TAG_reference_type, baseType: !18, size: 64)
!18 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !19)
!19 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "S", file: !13, line: 2, size: 192, flags: DIFlagTypePassByValue, elements: !20, identifier: "_ZTS1S")
!20 = !{!21, !22, !23}
!21 = !DIDerivedType(tag: DW_TAG_member, name: "a", scope: !19, file: !13, line: 2, baseType: !16, size: 64)
!22 = !DIDerivedType(tag: DW_TAG_member, name: "b", scope: !19, file: !13, line: 2, baseType: !16, size: 64, offset: 64)
!23 = !DIDerivedType(tag: DW_TAG_member, name: "c", scope: !19, file: !13, line: 2, baseType: !16, size: 64, offset: 128)
!24 = !{!25}
!25 = !DILocalVariable(name: "s", arg: 1, scope: !12, file: !13, line: 7, type: !17)
!26 = !DILocation(line: 0, scope: !12)
!27 = !DILocation(line: 7, column: 36, scope: !12, atomGroup: 1, atomRank: 2)
!28 = !{!29, !30, i64 0}
!29 = !{!"_ZTS1S", !30, i64 0, !30, i64 8, !30, i64 16}
!30 = !{!"long", !10, i64 0}
!31 = !DILocation(line: 7, column: 27, scope: !12, atomGroup: 1, atomRank: 1)
!32 = distinct !DISubprogram(name: "by_rvalue_ref", linkageName: "_Z13by_rvalue_refO1S", scope: !13, file: !13, line: 8, type: !33, scopeLine: 8, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !36, keyInstructions: true)
!33 = !DISubroutineType(types: !34)
!34 = !{!16, !35}
!35 = !DIDerivedType(tag: DW_TAG_rvalue_reference_type, baseType: !19, size: 64)
!36 = !{!37}
!37 = !DILocalVariable(name: "s", arg: 1, scope: !32, file: !13, line: 8, type: !35)
!38 = !DILocation(line: 0, scope: !32)
!39 = !DILocation(line: 8, column: 38, scope: !32)
!40 = !DILocation(line: 8, column: 38, scope: !32, atomGroup: 1, atomRank: 2)
!41 = !{!29, !30, i64 8}
!42 = !DILocation(line: 8, column: 29, scope: !32, atomGroup: 1, atomRank: 1)
!43 = distinct !DISubprogram(name: "by_typedef_ref", linkageName: "_Z14by_typedef_refRK1S", scope: !13, file: !13, line: 9, type: !44, scopeLine: 9, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !47, keyInstructions: true)
!44 = !DISubroutineType(types: !45)
!45 = !{!16, !46}
!46 = !DIDerivedType(tag: DW_TAG_typedef, name: "SRef", file: !13, line: 3, baseType: !17)
!47 = !{!48}
!48 = !DILocalVariable(name: "s", arg: 1, scope: !43, file: !13, line: 9, type: !46)
!49 = !DILocation(line: 0, scope: !43)
!50 = !DILocation(line: 9, column: 40, scope: !43)
!51 = !DILocation(line: 9, column: 40, scope: !43, atomGroup: 1, atomRank: 2)
!52 = !{!29, !30, i64 16}
!53 = !DILocation(line: 9, column: 31, scope: !43, atomGroup: 1, atomRank: 1)
!54 = distinct !DISubprogram(name: "tagged", linkageName: "_Z6taggedRK1S", scope: !13, file: !13, line: 10, type: !14, scopeLine: 10, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !55, keyInstructions: true)
!55 = !{!56}
!56 = !DILocalVariable(name: "s", arg: 1, scope: !54, file: !13, line: 10, type: !17, annotations: !57)
!57 = !{!58}
!58 = !{!"btf_decl_tag", !"arg:nonnull"}
!59 = !DILocation(line: 0, scope: !54)
!60 = !DILocation(line: 10, column: 57, scope: !54, atomGroup: 1, atomRank: 2)
!61 = !DILocation(line: 10, column: 48, scope: !54, atomGroup: 1, atomRank: 1)
!62 = distinct !DISubprogram(name: "mixed", linkageName: "_Z5mixedlP1SRKS_", scope: !13, file: !13, line: 11, type: !63, scopeLine: 11, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !66, keyInstructions: true)
!63 = !DISubroutineType(types: !64)
!64 = !{!16, !16, !65, !17}
!65 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !19, size: 64)
!66 = !{!67, !68, !69}
!67 = !DILocalVariable(name: "x", arg: 1, scope: !62, file: !13, line: 11, type: !16)
!68 = !DILocalVariable(name: "p", arg: 2, scope: !62, file: !13, line: 11, type: !65)
!69 = !DILocalVariable(name: "s", arg: 3, scope: !62, file: !13, line: 11, type: !17)
!70 = !DILocation(line: 0, scope: !62)
!71 = !DILocation(line: 11, column: 54, scope: !62)
!72 = !DILocation(line: 11, column: 49, scope: !62)
!73 = !DILocation(line: 11, column: 60, scope: !62)
!74 = !DILocation(line: 11, column: 56, scope: !62, atomGroup: 1, atomRank: 2)
!75 = !DILocation(line: 11, column: 40, scope: !62, atomGroup: 1, atomRank: 1)
!76 = distinct !DISubprogram(name: "get", linkageName: "_ZNK1M3getEv", scope: !77, file: !13, line: 12, type: !81, scopeLine: 12, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, declaration: !80, retainedNodes: !85, keyInstructions: true)
!77 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "M", file: !13, line: 4, size: 64, flags: DIFlagTypePassByValue, elements: !78, identifier: "_ZTS1M")
!78 = !{!79, !80}
!79 = !DIDerivedType(tag: DW_TAG_member, name: "v", scope: !77, file: !13, line: 4, baseType: !16, size: 64)
!80 = !DISubprogram(name: "get", linkageName: "_ZNK1M3getEv", scope: !77, file: !13, line: 4, type: !81, scopeLine: 4, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!81 = !DISubroutineType(types: !82)
!82 = !{!16, !83}
!83 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !84, size: 64, flags: DIFlagArtificial | DIFlagObjectPointer)
!84 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !77)
!85 = !{!86}
!86 = !DILocalVariable(name: "this", arg: 1, scope: !76, type: !87, flags: DIFlagArtificial | DIFlagObjectPointer)
!87 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !84, size: 64)
!88 = !DILocation(line: 0, scope: !76)
!89 = !DILocation(line: 12, column: 30, scope: !76, atomGroup: 1, atomRank: 2)
!90 = !{!91, !30, i64 0}
!91 = !{!"_ZTS1M", !30, i64 0}
!92 = !DILocation(line: 12, column: 23, scope: !76, atomGroup: 1, atomRank: 1)
!93 = distinct !DISubprogram(name: "make", linkageName: "_Z4makelRK1S", scope: !13, file: !13, line: 13, type: !94, scopeLine: 13, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !96, keyInstructions: true)
!94 = !DISubroutineType(types: !95)
!95 = !{!19, !16, !17}
!96 = !{!97, !98, !99}
!97 = !DILocalVariable(name: "x", arg: 1, scope: !93, file: !13, line: 13, type: !16)
!98 = !DILocalVariable(name: "s", arg: 2, scope: !93, file: !13, line: 13, type: !17)
!99 = !DILocalVariable(name: "r", scope: !93, file: !13, line: 13, type: !19)
!100 = !DILocation(line: 0, scope: !93)
!101 = !DILocation(line: 13, column: 32, scope: !93)
!102 = !DILocation(line: 13, column: 36, scope: !93, atomGroup: 1, atomRank: 1)
!103 = !DILocation(line: 13, column: 36, scope: !93)
!104 = !DILocation(line: 13, column: 42, scope: !93)
!105 = !DILocation(line: 13, column: 42, scope: !93, atomGroup: 1, atomRank: 2)
!106 = !DILocation(line: 13, column: 47, scope: !93)
!107 = !DILocation(line: 13, column: 47, scope: !93, atomGroup: 1, atomRank: 2)
!108 = !DILocation(line: 13, column: 51, scope: !93, atomGroup: 2, atomRank: 1)
!109 = distinct !DISubprogram(name: "by_ref_hidden", linkageName: "_Z13by_ref_hiddenRK1S", scope: !13, file: !13, line: 15, type: !14, scopeLine: 15, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !110, keyInstructions: true)
!110 = !{!111}
!111 = !DILocalVariable(name: "s", arg: 1, scope: !109, file: !13, line: 15, type: !17)
!112 = !DILocation(line: 0, scope: !109)
!113 = !DILocation(line: 15, column: 81, scope: !109)
!114 = !DILocation(line: 15, column: 81, scope: !109, atomGroup: 1, atomRank: 2)
!115 = !DILocation(line: 15, column: 72, scope: !109, atomGroup: 1, atomRank: 1)
!116 = distinct !DISubprogram(name: "calls", linkageName: "_Z5callsRK1S", scope: !13, file: !13, line: 16, type: !14, scopeLine: 16, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !117, keyInstructions: true)
!117 = !{!118}
!118 = !DILocalVariable(name: "s", arg: 1, scope: !116, file: !13, line: 16, type: !17)
!119 = !DILocation(line: 0, scope: !116)
!120 = !DILocation(line: 16, column: 33, scope: !116)
!121 = distinct !DILocation(line: 0, scope: !109, inlinedAt: !122)
!122 = distinct !DILocation(line: 16, column: 52, scope: !116)
!123 = distinct !DILocation(line: 15, column: 81, scope: !109, inlinedAt: !122)
!124 = distinct !DILocation(line: 15, column: 81, scope: !109, inlinedAt: !122, atomGroup: 1, atomRank: 2)
!125 = !DILocation(line: 16, column: 50, scope: !116, atomGroup: 1, atomRank: 2)
!126 = !DILocation(line: 16, column: 26, scope: !116, atomGroup: 1, atomRank: 1)
!127 = distinct !DISubprogram(name: "by_ref_static", linkageName: "_ZL13by_ref_staticRK1S", scope: !13, file: !13, line: 14, type: !14, scopeLine: 14, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !128, keyInstructions: true)
!128 = !{!129}
!129 = !DILocalVariable(name: "s", arg: 1, scope: !127, file: !13, line: 14, type: !17)
!130 = !DILocation(line: 0, scope: !127)
!131 = !DILocation(line: 14, column: 76, scope: !127)
!132 = !DILocation(line: 14, column: 76, scope: !127, atomGroup: 1, atomRank: 2)
!133 = !DILocation(line: 14, column: 67, scope: !127, atomGroup: 1, atomRank: 1)
!134 = distinct !DISubprogram(name: "prog", linkageName: "_Z4progR6xdp_md", scope: !13, file: !13, line: 17, type: !135, scopeLine: 17, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !144, keyInstructions: true)
!135 = !DISubroutineType(types: !136)
!136 = !{!137, !138}
!137 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!138 = !DIDerivedType(tag: DW_TAG_reference_type, baseType: !139, size: 64)
!139 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "xdp_md", file: !13, line: 5, size: 64, flags: DIFlagTypePassByValue, elements: !140, identifier: "_ZTS6xdp_md")
!140 = !{!141, !143}
!141 = !DIDerivedType(tag: DW_TAG_member, name: "data", scope: !139, file: !13, line: 5, baseType: !142, size: 32)
!142 = !DIBasicType(name: "unsigned int", size: 32, encoding: DW_ATE_unsigned)
!143 = !DIDerivedType(tag: DW_TAG_member, name: "data_end", scope: !139, file: !13, line: 5, baseType: !142, size: 32, offset: 32)
!144 = !{!145}
!145 = !DILocalVariable(name: "ctx", arg: 1, scope: !134, file: !13, line: 17, type: !138)
!146 = !DILocation(line: 0, scope: !134)
!147 = !DILocation(line: 17, column: 68, scope: !134, atomGroup: 1, atomRank: 2)
!148 = !{!149, !9, i64 0}
!149 = !{!"_ZTS6xdp_md", !9, i64 0, !9, i64 4}
!150 = !DILocation(line: 17, column: 57, scope: !134, atomGroup: 1, atomRank: 1)
!151 = distinct !DISubprogram(name: "by_ref_npv", linkageName: "_Z10by_ref_npvRK1S", scope: !13, file: !13, line: 18, type: !14, scopeLine: 18, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !153, keyInstructions: true)
!152 = !DILocalVariable(name: "s", arg: 1, scope: !151, file: !13, line: 18, type: !17)
!153 = !{!152}
!154 = !DILocation(line: 0, scope: !151)
!155 = !DILocation(line: 18, column: 40, scope: !151)

; by_ref(const S &)
; CHECK:      .long 16 # BTF_KIND_FUNC(id = 6)
; CHECK-NEXT: .long 201326593 # 0xc000001
; CHECK-NEXT: .long 5
; CHECK-NEXT: .long 30 # BTF_KIND_DECL_TAG(id = 7)
; CHECK-NEXT: .long 285212672 # 0x11000000
; CHECK-NEXT: .long 6
; CHECK-NEXT: .long 0

; by_rvalue_ref(S &&)
; CHECK:      .long 60 # BTF_KIND_FUNC(id = 10)
; CHECK-NEXT: .long 201326593 # 0xc000001
; CHECK-NEXT: .long 9
; CHECK-NEXT: .long 30 # BTF_KIND_DECL_TAG(id = 11)
; CHECK-NEXT: .long 285212672 # 0x11000000
; CHECK-NEXT: .long 10
; CHECK-NEXT: .long 0

; by_typedef_ref(SRef)
; CHECK:      .long 86 # BTF_KIND_FUNC(id = 14)
; CHECK-NEXT: .long 201326593 # 0xc000001
; CHECK-NEXT: .long 13
; CHECK-NEXT: .long 30 # BTF_KIND_DECL_TAG(id = 15)
; CHECK-NEXT: .long 285212672 # 0x11000000
; CHECK-NEXT: .long 14
; CHECK-NEXT: .long 0

; tagged(const S & __tag("arg:nonnull")): the source's tag, once
; CHECK:      .long 109 # BTF_KIND_FUNC(id = 17)
; CHECK-NEXT: .long 201326593 # 0xc000001
; CHECK-NEXT: .long 16
; CHECK-NEXT: .long 30 # BTF_KIND_DECL_TAG(id = 18)
; CHECK-NEXT: .long 285212672 # 0x11000000
; CHECK-NEXT: .long 17
; CHECK-NEXT: .long 0
; CHECK-NEXT: .long 0 # BTF_KIND_PTR(id = 19)

; mixed(long, S *, const S &): the reference alone, the third parameter
; CHECK:      .long 127 # BTF_KIND_FUNC(id = 21)
; CHECK-NEXT: .long 201326593 # 0xc000001
; CHECK-NEXT: .long 20
; CHECK-NEXT: .long 30 # BTF_KIND_DECL_TAG(id = 22)
; CHECK-NEXT: .long 285212672 # 0x11000000
; CHECK-NEXT: .long 21
; CHECK-NEXT: .long 2

; M::get() const: this
; CHECK:      .long 153 # BTF_KIND_FUNC(id = 28)
; CHECK-NEXT: .long 201326593 # 0xc000001
; CHECK-NEXT: .long 26
; CHECK-NEXT: .long 30 # BTF_KIND_DECL_TAG(id = 29)
; CHECK-NEXT: .long 285212672 # 0x11000000
; CHECK-NEXT: .long 28
; CHECK-NEXT: .long 0

; make(long, const S &), which returns through an sret pointer in IR: the
; reference is the second parameter of the source prototype
; CHECK:      .long 166 # BTF_KIND_FUNC(id = 31)
; CHECK-NEXT: .long 201326593 # 0xc000001
; CHECK-NEXT: .long 30
; CHECK-NEXT: .long 30 # BTF_KIND_DECL_TAG(id = 32)
; CHECK-NEXT: .long 285212672 # 0x11000000
; CHECK-NEXT: .long 31
; CHECK-NEXT: .long 1

; by_ref_hidden(const S &): hidden, so static to libbpf, no tag
; CHECK:      .long 179 # BTF_KIND_FUNC(id = 34)
; CHECK-NEXT: .long 201326593 # 0xc000001
; CHECK-NEXT: .long 33
; CHECK-NEXT: .long 0 # BTF_KIND_FUNC_PROTO(id = 35)

; calls(const S &)
; CHECK:      .long 201 # BTF_KIND_FUNC(id = 36)
; CHECK-NEXT: .long 201326593 # 0xc000001
; CHECK-NEXT: .long 35
; CHECK-NEXT: .long 30 # BTF_KIND_DECL_TAG(id = 37)
; CHECK-NEXT: .long 285212672 # 0x11000000
; CHECK-NEXT: .long 36
; CHECK-NEXT: .long 0

; by_ref_static(const S &): static, no tag
; CHECK:      .long 214 # BTF_KIND_FUNC(id = 39)
; CHECK-NEXT: .long 201326592 # 0xc000000
; CHECK-NEXT: .long 38
; CHECK-NEXT: .long 0 # BTF_KIND_PTR(id = 40)

; prog(xdp_md &): a program, whose parameter is its context, no tag
; CHECK:      .long 279 # BTF_KIND_FUNC(id = 45)
; CHECK-NEXT: .long 201326593 # 0xc000001
; CHECK-NEXT: .long 43
; CHECK-NEXT: .long 0 # BTF_KIND_FUNC_PROTO(id = 46)

; by_ref_npv(const S &) under -fno-delete-null-pointer-checks: no tag
; CHECK:      .long 299 # BTF_KIND_FUNC(id = 47)
; CHECK-NEXT: .long 201326593 # 0xc000001
; CHECK-NEXT: .long 46
; CHECK-NEXT: .byte 0 # string offset=0

; CHECK: .ascii "arg:nonnull" # string offset=30
