; RUN: llc -mtriple=bpfel -mcpu=v4 -filetype=obj -o %t1 %s
; RUN: llvm-objcopy --dump-section='.BTF'=%t2 %t1
; RUN: %python %p/print_btf.py %t2 | FileCheck -check-prefixes=CHECK-BTF %s
; RUN: llc -mtriple=bpfel -mcpu=v4 -filetype=asm -o - %s | FileCheck %s

; A libcall that the backend emits, for a memcpy of a size it does not know
; or for 128-bit arithmetic, calls a function that nothing in the IR names,
; and so had no BTF. libbpf links the call to the function's definition in
; another object through BTF, and matches the prototypes: the backend
; declares each with the debug info of its C prototype, as a definition in
; C has it.

define dso_local void @copy(ptr %d, ptr %s, i64 %n) !dbg !5 {
entry:
  call void @llvm.memcpy.p0.p0.i64(ptr %d, ptr %s, i64 %n, i1 false), !dbg !9
  call void @llvm.memmove.p0.p0.i64(ptr %d, ptr %s, i64 %n, i1 false), !dbg !9
  call void @llvm.memset.p0.i64(ptr %d, i8 0, i64 %n, i1 false), !dbg !9
  ret void, !dbg !9
}

define dso_local i128 @arith(i128 %a, i128 %b) !dbg !10 {
entry:
  %m = mul i128 %a, %b, !dbg !14
  %d = sdiv i128 %m, %b, !dbg !14
  %r = srem i128 %d, %a, !dbg !14
  %ud = udiv i128 %r, %b, !dbg !14
  %ur = urem i128 %ud, %a, !dbg !14
  ret i128 %ur, !dbg !14
}

declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)
declare void @llvm.memmove.p0.p0.i64(ptr, ptr, i64, i1)
declare void @llvm.memset.p0.i64(ptr, i8, i64, i1)

; CHECK-LABEL: copy:
; CHECK:         call memcpy
; CHECK:         call memmove
; CHECK:         call memset
; CHECK-LABEL: arith:
; CHECK:         call __multi3
; CHECK:         call __divti3
; CHECK:         call __modti3
; CHECK:         call __udivti3
; CHECK:         call __umodti3

; void *memcpy(void *, const void *, unsigned long), and memmove alike.
; CHECK-BTF:      [[[MEMCPY:[0-9]+]]] FUNC_PROTO '(anon)' ret_type_id=[[VOIDPTR:[0-9]+]] vlen=3
; CHECK-BTF-NEXT:         '(anon)' type_id=[[VOIDPTR]]
; CHECK-BTF-NEXT:         '(anon)' type_id=[[CONSTVOIDPTR:[0-9]+]]
; CHECK-BTF-NEXT:         '(anon)' type_id=[[ULONG:[0-9]+]]
; CHECK-BTF-NEXT: [[[VOIDPTR]]] PTR '(anon)' type_id=0
; CHECK-BTF-NEXT: [[[CONSTVOIDPTR]]] PTR '(anon)' type_id=[[CONST:[0-9]+]]
; CHECK-BTF-NEXT: [[[CONST]]] CONST '(anon)' type_id=0
; CHECK-BTF-NEXT: [[[ULONG]]] INT 'unsigned long' size=8 bits_offset=0 nr_bits=64 encoding=(none)
; CHECK-BTF-NEXT: [{{[0-9]+}}] FUNC 'memcpy' type_id=[[MEMCPY]] linkage=extern
; CHECK-BTF-NEXT: [[[MEMMOVE:[0-9]+]]] FUNC_PROTO '(anon)' ret_type_id=[[VOIDPTR]] vlen=3
; CHECK-BTF-NEXT:         '(anon)' type_id=[[VOIDPTR]]
; CHECK-BTF-NEXT:         '(anon)' type_id=[[CONSTVOIDPTR]]
; CHECK-BTF-NEXT:         '(anon)' type_id=[[ULONG]]
; CHECK-BTF-NEXT: [{{[0-9]+}}] FUNC 'memmove' type_id=[[MEMMOVE]] linkage=extern
; void *memset(void *, int, unsigned long)
; CHECK-BTF-NEXT: [[[MEMSET:[0-9]+]]] FUNC_PROTO '(anon)' ret_type_id=[[VOIDPTR]] vlen=3
; CHECK-BTF-NEXT:         '(anon)' type_id=[[VOIDPTR]]
; CHECK-BTF-NEXT:         '(anon)' type_id=[[INT:[0-9]+]]
; CHECK-BTF-NEXT:         '(anon)' type_id=[[ULONG]]
; CHECK-BTF-NEXT: [[[INT]]] INT 'int' size=4 bits_offset=0 nr_bits=32 encoding=SIGNED
; CHECK-BTF-NEXT: [{{[0-9]+}}] FUNC 'memset' type_id=[[MEMSET]] linkage=extern
; __int128 __multi3(__int128, __int128), and __divti3 and __modti3 alike.
; CHECK-BTF:      [[[I128:[0-9]+]]] INT '__int128' size=16 bits_offset=0 nr_bits=128 encoding=SIGNED
; CHECK-BTF-NEXT: [{{[0-9]+}}] FUNC 'arith'
; CHECK-BTF-NEXT: [[[MULTI3:[0-9]+]]] FUNC_PROTO '(anon)' ret_type_id=[[I128]] vlen=2
; CHECK-BTF-NEXT:         '(anon)' type_id=[[I128]]
; CHECK-BTF-NEXT:         '(anon)' type_id=[[I128]]
; CHECK-BTF-NEXT: [{{[0-9]+}}] FUNC '__multi3' type_id=[[MULTI3]] linkage=extern
; CHECK-BTF:      FUNC '__divti3' type_id={{[0-9]+}} linkage=extern
; CHECK-BTF:      FUNC '__modti3' type_id={{[0-9]+}} linkage=extern
; unsigned __int128 __udivti3(unsigned __int128, unsigned __int128), and __umodti3.
; CHECK-BTF-NEXT: [[[UDIVTI3:[0-9]+]]] FUNC_PROTO '(anon)' ret_type_id=[[U128:[0-9]+]] vlen=2
; CHECK-BTF-NEXT:         '(anon)' type_id=[[U128]]
; CHECK-BTF-NEXT:         '(anon)' type_id=[[U128]]
; CHECK-BTF-NEXT: [[[U128]]] INT 'unsigned __int128' size=16 bits_offset=0 nr_bits=128 encoding=(none)
; CHECK-BTF-NEXT: [{{[0-9]+}}] FUNC '__udivti3' type_id=[[UDIVTI3]] linkage=extern
; CHECK-BTF:      FUNC '__umodti3' type_id={{[0-9]+}} linkage=extern

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3}

!0 = distinct !DICompileUnit(language: DW_LANG_C11, file: !1, producer: "clang", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, nameTableKind: None)
!1 = !DIFile(filename: "libcalls.c", directory: "/tmp")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !{null}
!5 = distinct !DISubprogram(name: "copy", scope: !1, file: !1, line: 1, type: !6, scopeLine: 1, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!6 = !DISubroutineType(types: !4)
!9 = !DILocation(line: 2, scope: !5)
!10 = distinct !DISubprogram(name: "arith", scope: !1, file: !1, line: 5, type: !11, scopeLine: 5, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!11 = !DISubroutineType(types: !12)
!12 = !{!13, !13, !13}
!13 = !DIBasicType(name: "__int128", size: 128, encoding: DW_ATE_signed)
!14 = !DILocation(line: 6, scope: !10)
