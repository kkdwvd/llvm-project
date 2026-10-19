; RUN: llc -mtriple=bpfel -filetype=asm -o - %s | FileCheck %s

; A global symbol that the frontend makes on its own, such as a C++ type_info
; object or vtable, has no source type: no debug info, or for a vtable's
; definition an artificial `void *` for a debugger. libbpf links a symbol to its
; definition in another object through BTF all the same, and compares the
; types of the two. Each is described as the bytes it is, an extern as bytes of
; an unknown number: libbpf ignores the size of an array, so the descriptions
; match. A local symbol without debug info needs none and gets none.
;
;   namespace std {
;   class type_info;
;   }
;
;   struct Error {
;     int code;
;   };
;
;   struct V {
;     virtual void f();
;   };
;
;   void V::f() {}
;
;   extern "C" void take(const void *);
;
;   extern "C" void g()
;   {
;     take(&typeid(int));
;     take(&typeid(Error));
;   }
;
; clang --target=bpf -O2 -g -frtti -S -emit-llvm, with @local added.

$_ZTI5Error = comdat any

$_ZTS5Error = comdat any

@_ZTIi = external dso_local constant ptr
@_ZTI5Error = linkonce_odr dso_local constant { ptr, ptr } { ptr getelementptr inbounds (i8, ptr @_ZTVN10__cxxabiv117__class_type_infoE, i64 16), ptr @_ZTS5Error }, comdat, align 8
@_ZTVN10__cxxabiv117__class_type_infoE = external dso_local global [0 x ptr]
@_ZTS5Error = linkonce_odr dso_local constant [7 x i8] c"5Error\00", comdat, align 1
@_ZTV1V = dso_local local_unnamed_addr constant { [3 x ptr] } { [3 x ptr] [ptr null, ptr @_ZTI1V, ptr @_ZN1V1fEv] }, align 8, !dbg !0
@_ZTI1V = dso_local constant { ptr, ptr } { ptr getelementptr inbounds (i8, ptr @_ZTVN10__cxxabiv117__class_type_infoE, i64 16), ptr @_ZTS1V }, align 8
@_ZTS1V = dso_local constant [3 x i8] c"1V\00", align 1
@local = internal global i32 0, align 4

define dso_local void @_ZN1V1fEv(ptr nofree nonnull readnone align 8 captures(none) %this) unnamed_addr align 2 !dbg !29 {
entry:
    #dbg_value(ptr poison, !31, !DIExpression(), !33)
  ret void, !dbg !34
}

define dso_local void @g() local_unnamed_addr !dbg !35 {
entry:
  tail call void @take(ptr noundef nonnull @_ZTIi), !dbg !38
  tail call void @take(ptr noundef nonnull @_ZTI5Error), !dbg !39
  store i32 1, ptr @local, align 4, !dbg !40
  ret void, !dbg !40
}

declare !dbg !41 dso_local void @take(ptr noundef) local_unnamed_addr


!llvm.dbg.cu = !{!2}
!llvm.module.flags = !{!19, !20, !21, !22}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(name: "__clang_vtable", linkageName: "_ZTV1V", scope: !2, file: !3, type: !5, isLocal: false, isDefinition: true, declaration: !6, align: 64)
!2 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !3, producer: "clang", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, globals: !4, splitDebugInlining: false, nameTableKind: None)
!3 = !DIFile(filename: "rtti.cpp", directory: "/tmp")
!4 = !{!0}
!5 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: null, size: 64)
!6 = !DIDerivedType(tag: DW_TAG_variable, name: "__clang_vtable", scope: !7, file: !3, baseType: !5, flags: DIFlagPrivate | DIFlagArtificial | DIFlagStaticMember)
!7 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "V", file: !3, line: 9, size: 64, flags: DIFlagTypePassByReference | DIFlagNonTrivial, elements: !8, vtableHolder: !7)
!8 = !{!9, !15}
!9 = !DIDerivedType(tag: DW_TAG_member, name: "_vptr$V", scope: !3, file: !3, baseType: !10, size: 64, flags: DIFlagArtificial)
!10 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !11, size: 64)
!11 = !DIDerivedType(tag: DW_TAG_pointer_type, name: "__vtbl_ptr_type", baseType: !12, size: 64)
!12 = !DISubroutineType(types: !13)
!13 = !{!14}
!14 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!15 = !DISubprogram(name: "f", linkageName: "_ZN1V1fEv", scope: !7, file: !3, line: 10, type: !16, scopeLine: 10, containingType: !7, virtualIndex: 0, flags: DIFlagPrototyped, spFlags: DISPFlagVirtual | DISPFlagOptimized)
!16 = !DISubroutineType(types: !17)
!17 = !{null, !18}
!18 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !7, size: 64, flags: DIFlagArtificial | DIFlagObjectPointer)
!19 = !{i32 7, !"Dwarf Version", i32 5}
!20 = !{i32 2, !"Debug Info Version", i32 3}
!21 = !{i32 7, !"frame-pointer", i32 2}
!22 = !{i32 7, !"debug-info-assignment-tracking", i1 true}
!29 = distinct !DISubprogram(name: "f", linkageName: "_ZN1V1fEv", scope: !7, file: !3, line: 13, type: !16, scopeLine: 13, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !2, declaration: !15, retainedNodes: !30, keyInstructions: true)
!30 = !{!31}
!31 = !DILocalVariable(name: "this", arg: 1, scope: !29, type: !32, flags: DIFlagArtificial | DIFlagObjectPointer)
!32 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !7, size: 64)
!33 = !DILocation(line: 0, scope: !29)
!34 = !DILocation(line: 13, column: 14, scope: !29, atomGroup: 1, atomRank: 1)
!35 = distinct !DISubprogram(name: "g", scope: !3, file: !3, line: 17, type: !36, scopeLine: 18, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !2, keyInstructions: true)
!36 = !DISubroutineType(types: !37)
!37 = !{null}
!38 = !DILocation(line: 19, column: 3, scope: !35)
!39 = !DILocation(line: 20, column: 3, scope: !35)
!40 = !DILocation(line: 21, column: 1, scope: !35, atomGroup: 1, atomRank: 1)
!41 = !DISubprogram(name: "take", scope: !3, file: !3, line: 15, type: !42, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized, retainedNodes: !46)
!42 = !DISubroutineType(types: !43)
!43 = !{null, !44}
!44 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !45, size: 64)
!45 = !DIDerivedType(tag: DW_TAG_const_type, baseType: null)
!46 = !{!47}
!47 = !DILocalVariable(arg: 1, scope: !41, file: !3, line: 15, type: !44)

; The extern FUNC take, then the types of the symbols.
; CHECK:         .long   201326594               # 0xc000002
; CHECK-NEXT:    .long   {{[0-9]+}}
; CHECK-NEXT:    .long   [[UCHAR_NAME:[0-9]+]] # BTF_KIND_INT(id = [[UCHAR:[0-9]+]])
; CHECK-NEXT:    .long   16777216                # 0x1000000
; CHECK-NEXT:    .long   1
; CHECK-NEXT:    .long   8                       # 0x8
; CHECK-NEXT:    .long   {{[0-9]+}}              # BTF_KIND_INT(id = [[INDEX:[0-9]+]])
; CHECK-NEXT:    .long   16777216                # 0x1000000
; CHECK-NEXT:    .long   4
; CHECK-NEXT:    .long   32                      # 0x20
; CHECK-NEXT:    .long   0                       # BTF_KIND_ARRAY(id = [[UNKNOWN:[0-9]+]])
; CHECK-NEXT:    .long   50331648                # 0x3000000
; CHECK-NEXT:    .long   0
; CHECK-NEXT:    .long   [[UCHAR]]
; CHECK-NEXT:    .long   [[INDEX]]
; CHECK-NEXT:    .long   0
; CHECK-NEXT:    .long   [[ZTII:[0-9]+]]         # BTF_KIND_VAR(id = {{[0-9]+}})
; CHECK-NEXT:    .long   234881024               # 0xe000000
; CHECK-NEXT:    .long   [[UNKNOWN]]
; CHECK-NEXT:    .long   2
; CHECK-NEXT:    .long   0                       # BTF_KIND_ARRAY(id = [[SIXTEEN:[0-9]+]])
; CHECK-NEXT:    .long   50331648                # 0x3000000
; CHECK-NEXT:    .long   0
; CHECK-NEXT:    .long   [[UCHAR]]
; CHECK-NEXT:    .long   [[INDEX]]
; CHECK-NEXT:    .long   16
; CHECK-NEXT:    .long   [[ZTIERROR:[0-9]+]]     # BTF_KIND_VAR(id = [[ZTIERROR_ID:[0-9]+]])
; CHECK-NEXT:    .long   234881024               # 0xe000000
; CHECK-NEXT:    .long   [[SIXTEEN]]
; CHECK-NEXT:    .long   1
; CHECK-NEXT:    .long   [[ZTVCLASS:[0-9]+]]     # BTF_KIND_VAR(id = {{[0-9]+}})
; CHECK-NEXT:    .long   234881024               # 0xe000000
; CHECK-NEXT:    .long   [[UNKNOWN]]
; CHECK-NEXT:    .long   2
; CHECK-NEXT:    .long   0                       # BTF_KIND_ARRAY(id = [[SEVEN:[0-9]+]])
; CHECK-NEXT:    .long   50331648                # 0x3000000
; CHECK-NEXT:    .long   0
; CHECK-NEXT:    .long   [[UCHAR]]
; CHECK-NEXT:    .long   [[INDEX]]
; CHECK-NEXT:    .long   7
; CHECK-NEXT:    .long   {{[0-9]+}}              # BTF_KIND_VAR(id = {{[0-9]+}})
; CHECK-NEXT:    .long   234881024               # 0xe000000
; CHECK-NEXT:    .long   [[SEVEN]]
; CHECK-NEXT:    .long   1
; CHECK-NEXT:    .long   0                       # BTF_KIND_ARRAY(id = [[VTABLE:[0-9]+]])
; CHECK-NEXT:    .long   50331648                # 0x3000000
; CHECK-NEXT:    .long   0
; CHECK-NEXT:    .long   [[UCHAR]]
; CHECK-NEXT:    .long   [[INDEX]]
; CHECK-NEXT:    .long   24
; CHECK-NEXT:    .long   [[ZTV1V:[0-9]+]]        # BTF_KIND_VAR(id = {{[0-9]+}})
; CHECK-NEXT:    .long   234881024               # 0xe000000
; CHECK-NEXT:    .long   [[VTABLE]]
; CHECK-NEXT:    .long   1
; CHECK:         # BTF_KIND_DATASEC(id = {{[0-9]+}})
; CHECK:         .long   [[ZTIERROR_ID]]
; CHECK-NEXT:    .long   _ZTI5Error
; CHECK-NEXT:    .long   16
; CHECK:         .ascii  "unsigned char"         # string offset=[[UCHAR_NAME]]
; CHECK:         .ascii  "_ZTIi"                 # string offset=[[ZTII]]
; CHECK:         .ascii  "_ZTI5Error"            # string offset=[[ZTIERROR]]
; CHECK:         .ascii  "_ZTVN10__cxxabiv117__class_type_infoE" # string offset=[[ZTVCLASS]]
; CHECK:         .ascii  "_ZTV1V"                # string offset=[[ZTV1V]]
; CHECK-NOT:     "local"
