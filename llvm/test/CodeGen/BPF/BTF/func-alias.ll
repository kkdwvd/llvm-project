; RUN: llc -mtriple=bpfel -filetype=obj -o %t1 %s
; RUN: llvm-objcopy --dump-section='.BTF'=%t2 %t1
; RUN: %python %p/print_btf.py %t2 | FileCheck -check-prefixes=CHECK-BTF %s
; RUN: llvm-readelf -s %t1 | FileCheck -check-prefixes=CHECK-SYM %s

; An alias of a function is a FUNC symbol of its own, and libbpf's linker
; looks every global symbol up among the FUNCs of the object's BTF and refuses
; the object when it finds none. Nothing in the debug info describes an
; alias: the subprogram is the function's. llvm-libc exports every C function
; as an alias of its definition in the library's namespace:
;
;   namespace __llvm_libc {
;   void *__memcpy_impl__(void *dst, const void *src, unsigned long n) asm("memcpy");
;   decltype(__memcpy_impl__) memcpy [[gnu::alias("memcpy")]];
;   void *__memcpy_impl__(void *dst, const void *src, unsigned long n) { return dst; }
;   }
;   static int f(int x) { return x; }
;   static int f_internal(int) __attribute__((alias("f")));
;
; The alias gets a FUNC named by its symbol, scoped by its linkage, with the
; prototype of the function it names.

@_ZN11__llvm_libc6memcpyEPvPKvm = hidden alias ptr (ptr, ptr, i64), ptr @memcpy
@f_internal = internal alias i32 (i32), ptr @f

define hidden ptr @memcpy(ptr noundef %dst, ptr noundef %src, i64 noundef %n) !dbg !10 {
entry:
  ret ptr %dst, !dbg !18
}

define internal i32 @f(i32 noundef %x) !dbg !19 {
entry:
  ret i32 %x, !dbg !24
}

; CHECK-SYM: GLOBAL HIDDEN {{.*}} memcpy
; CHECK-SYM: GLOBAL HIDDEN {{.*}} _ZN11__llvm_libc6memcpyEPvPKvm

; CHECK-BTF: FUNC_PROTO '(anon)' ret_type_id=[[VOIDP:[0-9]+]] vlen=3
; CHECK-BTF-NEXT: 'dst' type_id=[[VOIDP]]
; CHECK-BTF-NEXT: 'src' type_id=[[CVOIDP:[0-9]+]]
; CHECK-BTF-NEXT: 'n' type_id=[[ULONG:[0-9]+]]
; CHECK-BTF: FUNC 'memcpy' type_id={{[0-9]+}} linkage=global
; CHECK-BTF: FUNC_PROTO '(anon)' ret_type_id=[[INT:[0-9]+]] vlen=1
; CHECK-BTF-NEXT: 'x' type_id=[[INT]]
; CHECK-BTF: FUNC 'f' type_id={{[0-9]+}} linkage=static
; CHECK-BTF: FUNC_PROTO '(anon)' ret_type_id=[[VOIDP]] vlen=3
; CHECK-BTF-NEXT: 'dst' type_id=[[VOIDP]]
; CHECK-BTF-NEXT: 'src' type_id=[[CVOIDP]]
; CHECK-BTF-NEXT: 'n' type_id=[[ULONG]]
; CHECK-BTF-NEXT: FUNC '_ZN11__llvm_libc6memcpyEPvPKvm' type_id={{[0-9]+}} linkage=global
; CHECK-BTF: FUNC_PROTO '(anon)' ret_type_id=[[INT]] vlen=1
; CHECK-BTF-NEXT: 'x' type_id=[[INT]]
; CHECK-BTF-NEXT: FUNC 'f_internal' type_id={{[0-9]+}} linkage=static

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, nameTableKind: None)
!1 = !DIFile(filename: "alias.cpp", directory: "/tmp")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: null, size: 64)
!5 = !DIDerivedType(tag: DW_TAG_const_type, baseType: null)
!6 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !5, size: 64)
!7 = !DIBasicType(name: "unsigned long", size: 64, encoding: DW_ATE_unsigned)
!8 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!9 = !DINamespace(name: "__llvm_libc", scope: null)
!10 = distinct !DISubprogram(name: "__memcpy_impl__", linkageName: "memcpy", scope: !9, file: !1, line: 3, type: !11, scopeLine: 3, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !13)
!11 = !DISubroutineType(types: !12)
!12 = !{!4, !4, !6, !7}
!13 = !{!14, !15, !16}
!14 = !DILocalVariable(name: "dst", arg: 1, scope: !10, file: !1, line: 3, type: !4)
!15 = !DILocalVariable(name: "src", arg: 2, scope: !10, file: !1, line: 3, type: !6)
!16 = !DILocalVariable(name: "n", arg: 3, scope: !10, file: !1, line: 3, type: !7)
!18 = !DILocation(line: 3, scope: !10)
!19 = distinct !DISubprogram(name: "f", scope: !1, file: !1, line: 5, type: !20, scopeLine: 5, flags: DIFlagPrototyped, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !22)
!20 = !DISubroutineType(types: !21)
!21 = !{!8, !8}
!22 = !{!23}
!23 = !DILocalVariable(name: "x", arg: 1, scope: !19, file: !1, line: 5, type: !8)
!24 = !DILocation(line: 5, scope: !19)
