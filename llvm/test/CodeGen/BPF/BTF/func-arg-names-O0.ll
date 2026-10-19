; RUN: llc -mtriple=bpfel -filetype=obj -o %t1 %s
; RUN: llvm-objcopy --dump-section='.BTF'=%t2 %t1
; RUN: %python %p/print_btf.py %t2 | FileCheck -check-prefixes=CHECK-BTF %s

; Source:
;   int add(int a, int b) { return a + b; }
; Compilation flags:
;   clang --target=bpf -O0 -g -S -emit-llvm t.c
;
; Without optimization clang retains no variables of the arguments in the
; subprogram, and only the function's debug records refer to them. BTF names
; the parameters after them all the same: the kernel refuses a global function
; with a parameter it does not name.

define dso_local i32 @add(i32 noundef %a, i32 noundef %b) #0 !dbg !6 {
entry:
  %a.addr = alloca i32, align 4
  %b.addr = alloca i32, align 4
  store i32 %a, ptr %a.addr, align 4
    #dbg_declare(ptr %a.addr, !12, !DIExpression(), !13)
  store i32 %b, ptr %b.addr, align 4
    #dbg_declare(ptr %b.addr, !14, !DIExpression(), !15)
  %0 = load i32, ptr %a.addr, align 4, !dbg !16
  %1 = load i32, ptr %b.addr, align 4, !dbg !16
  %add = add nsw i32 %0, %1, !dbg !16
  ret i32 %add, !dbg !16
}

attributes #0 = { noinline nounwind optnone }

; CHECK-BTF:      [{{[0-9]+}}] FUNC_PROTO '(anon)' ret_type_id={{[0-9]+}} vlen=2
; CHECK-BTF-NEXT:         'a' type_id={{[0-9]+}}
; CHECK-BTF-NEXT:         'b' type_id={{[0-9]+}}
; CHECK-BTF-NEXT: [{{[0-9]+}}] FUNC 'add' type_id={{[0-9]+}} linkage=global

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3}

!0 = distinct !DICompileUnit(language: DW_LANG_C11, file: !1, isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!1 = !DIFile(filename: "t.c", directory: "/some/dir")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!6 = distinct !DISubprogram(name: "add", scope: !1, file: !1, line: 1, type: !8, scopeLine: 1, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !11)
!8 = !DISubroutineType(types: !9)
!9 = !{!10, !10, !10}
!10 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!11 = !{}
!12 = !DILocalVariable(name: "a", arg: 1, scope: !6, file: !1, line: 1, type: !10)
!13 = !DILocation(line: 1, column: 13, scope: !6)
!14 = !DILocalVariable(name: "b", arg: 2, scope: !6, file: !1, line: 1, type: !10)
!15 = !DILocation(line: 1, column: 20, scope: !6)
!16 = !DILocation(line: 1, column: 25, scope: !6)
