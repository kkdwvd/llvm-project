; RUN: llc -mtriple=bpfel -mcpu=v4 -filetype=obj -o %t1 %s
; RUN: llvm-objcopy --dump-section='.BTF'=%t2 %t1
; RUN: %python %p/print_btf.py %t2 | FileCheck %s

; A thread-local variable is emulated: its data is the __emutls_v. control
; block and __emutls_t. template that LowerEmuTLS makes from it, and an
; access calls __emutls_get_address(). The variable itself is never emitted,
; so BTF, which otherwise describes it from its debug info, leaves it out;
; the two globals have no debug info and are described as the bytes they
; are, like any global the compiler makes up, in .data and .rodata, and a
; declaration's control block as an extern. The libcall is declared with the
; prototype compiler-rt's emutls.c defines it with, the struct as a forward
; declaration that libbpf takes for the definition's. The IR is clang's, from
;
;   clang++ --target=bpf -mcpu=v4 -O2 -g
;
; of
;
;   thread_local int counter = 5;
;   static thread_local long hits;
;   extern thread_local int other;
;   int bump() { hits++; return counter++ + other; }
;
; without the C++ wrappers of the variables.

@counter = dso_local thread_local global i32 5, align 4, !dbg !0
@_ZL4hits = internal thread_local global i64 0, align 8, !dbg !5
@other = external thread_local global i32, align 4, !dbg !8

define dso_local i32 @_Z4bumpv() !dbg !14 {
  %h = call ptr @llvm.threadlocal.address.p0(ptr @_ZL4hits), !dbg !18
  %hv = load i64, ptr %h, !dbg !18
  %hn = add i64 %hv, 1, !dbg !18
  store i64 %hn, ptr %h, !dbg !18
  %c = call ptr @llvm.threadlocal.address.p0(ptr @counter), !dbg !18
  %cv = load i32, ptr %c, !dbg !18
  %cn = add i32 %cv, 1, !dbg !18
  store i32 %cn, ptr %c, !dbg !18
  %o = call ptr @llvm.threadlocal.address.p0(ptr @other), !dbg !18
  %ov = load i32, ptr %o, !dbg !18
  %r = add i32 %cv, %ov, !dbg !18
  ret i32 %r, !dbg !18
}

declare ptr @llvm.threadlocal.address.p0(ptr)

; CHECK:      [[[PROTO:[0-9]+]]] FUNC_PROTO '(anon)' ret_type_id=[[VOIDP:[0-9]+]] vlen=1
; CHECK-NEXT:         '(anon)' type_id=[[CTRLP:[0-9]+]]
; CHECK-NEXT: [[[VOIDP]]] PTR '(anon)' type_id=0
; CHECK-NEXT: [[[CTRLP]]] PTR '(anon)' type_id=[[CTRL:[0-9]+]]
; CHECK-NEXT: [[[CTRL]]] FWD '__emutls_control' fwd_kind=struct
; CHECK-NEXT: [{{[0-9]+}}] FUNC '__emutls_get_address' type_id=[[PROTO]] linkage=extern
; CHECK:      [[[BYTES32:[0-9]+]]] ARRAY '(anon)' type_id=[[CHAR:[0-9]+]] index_type_id={{[0-9]+}} nr_elems=32
; CHECK-NEXT: [[[VCOUNTER:[0-9]+]]] VAR '__emutls_v.counter' type_id=[[BYTES32]], linkage=global
; CHECK-NEXT: [[[BYTES4:[0-9]+]]] ARRAY '(anon)' type_id=[[CHAR]] index_type_id={{[0-9]+}} nr_elems=4
; CHECK-NEXT: [[[TCOUNTER:[0-9]+]]] VAR '__emutls_t.counter' type_id=[[BYTES4]], linkage=global
; CHECK-NEXT: [[[BYTES0:[0-9]+]]] ARRAY '(anon)' type_id=[[CHAR]] index_type_id={{[0-9]+}} nr_elems=0
; CHECK-NEXT: [{{[0-9]+}}] VAR '__emutls_v.other' type_id=[[BYTES0]], linkage=extern
; CHECK-NEXT: [{{[0-9]+}}] DATASEC '.data' size=0 vlen=1
; CHECK-NEXT:         type_id=[[VCOUNTER]] offset=0 size=32
; CHECK-NEXT: [{{[0-9]+}}] DATASEC '.rodata' size=0 vlen=1
; CHECK-NEXT:         type_id=[[TCOUNTER]] offset=0 size=4
; CHECK-NOT:  VAR 'counter'
; CHECK-NOT:  VAR 'hits'
; CHECK-NOT:  VAR '_ZL4hits'
; CHECK-NOT:  VAR 'other'
; CHECK-NOT:  __emutls_v._ZL4hits

!llvm.dbg.cu = !{!10}
!llvm.module.flags = !{!12, !13}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(name: "counter", scope: !10, file: !11, line: 1, type: !2, isLocal: false, isDefinition: true)
!2 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!5 = !DIGlobalVariableExpression(var: !6, expr: !DIExpression())
!6 = distinct !DIGlobalVariable(name: "hits", linkageName: "_ZL4hits", scope: !10, file: !11, line: 2, type: !7, isLocal: true, isDefinition: true)
!7 = !DIBasicType(name: "long", size: 64, encoding: DW_ATE_signed)
!8 = !DIGlobalVariableExpression(var: !9, expr: !DIExpression())
!9 = distinct !DIGlobalVariable(name: "other", scope: !10, file: !11, line: 3, type: !2, isLocal: false, isDefinition: false)
!10 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !11, isOptimized: true, emissionKind: FullDebug, globals: !{!0, !5, !8})
!11 = !DIFile(filename: "tls.cpp", directory: "/some/dir")
!12 = !{i32 7, !"Dwarf Version", i32 5}
!13 = !{i32 2, !"Debug Info Version", i32 3}
!14 = distinct !DISubprogram(name: "bump", linkageName: "_Z4bumpv", scope: !11, file: !11, line: 4, type: !15, scopeLine: 4, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !10)
!15 = !DISubroutineType(types: !16)
!16 = !{!2}
!18 = !DILocation(line: 4, column: 14, scope: !14)
