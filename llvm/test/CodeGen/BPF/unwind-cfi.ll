; RUN: llc -mtriple=bpfel -mcpu=v4 -filetype=asm -o - %s | FileCheck --check-prefix=ASM %s
; RUN: llc -mtriple=bpfel -mcpu=v4 -enable-new-pm -filetype=asm -o - %s | FileCheck --check-prefix=ASM %s
; RUN: llc -mtriple=bpfel -mcpu=v4 -filetype=obj -o %t %s
; RUN: llvm-readelf -S %t | FileCheck --check-prefix=OBJ %s

; BPF unwinds without tables: neither uwtable nor a function that may unwind
; asks for an .eh_frame entry. With debug info, the frame has CFI for a
; debugger, as any BPF function does. Without, unwind.ll has none at all.

declare void @may_unwind()
declare void @release(ptr) nounwind
declare i32 @__gxx_personality_v0(...)

define void @f(ptr %p) uwtable personality ptr @__gxx_personality_v0 !dbg !5 {
entry:
  invoke void @may_unwind()
          to label %ok unwind label %pad, !dbg !8

ok:
  ret void, !dbg !8

pad:
  %lp = landingpad { ptr, i32 } cleanup
  call void @release(ptr %p), !dbg !8
  resume { ptr, i32 } %lp, !dbg !8
}

; ASM:         .cfi_sections .debug_frame
; ASM-NEXT:    .cfi_startproc
; ASM-NOT:     .cfi_personality
; ASM-NOT:     .cfi_lsda
; ASM:         unwind_goto
; ASM:         .cfi_endproc

; OBJ:     .debug_frame
; OBJ-NOT: .eh_frame

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, nameTableKind: None)
!1 = !DIFile(filename: "t.cpp", directory: "/tmp")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !{null}
!5 = distinct !DISubprogram(name: "f", scope: !1, file: !1, line: 1, type: !6, scopeLine: 1, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!6 = !DISubroutineType(types: !4)
!7 = !{}
!8 = !DILocation(line: 2, column: 1, scope: !5)
