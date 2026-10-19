; RUN: llc -mtriple=bpfel < %s | FileCheck %s
; RUN: llc -mtriple=bpfel -function-sections < %s | FileCheck %s
; RUN: llc -mtriple=bpfel -filetype=obj -o %t < %s
; RUN: llvm-readelf -S %t | FileCheck --check-prefix=OBJ %s

; libbpf takes the name of an executable section for the type of the programs
; in it, and .text for the functions programs call. A function goes to .text
; unless its section attribute names another: a cold function, as one that
; only traps is, does not go to .text.unlikely., nor a hot one to .text.hot.,
; nor any function to a section of its own for -ffunction-sections. A
; function in a COMDAT group gets a section of the group, named after it and
; without a prefix, which libbpf's linker joins to .text.

$in_group = comdat any

define void @plain() {
  ret void
}

define void @cold_fn() #0 {
  ret void
}

define void @hot_fn() #1 {
  ret void
}

define linkonce_odr void @in_group() #0 comdat {
  ret void
}

define void @prog() section "xdp" {
  call void @cold_fn()
  ret void
}

attributes #0 = { cold }
attributes #1 = { hot }

; CHECK:      .text
; CHECK-NOT:  .section
; CHECK:      plain:
; CHECK-NOT:  .section
; CHECK:      cold_fn:
; CHECK-NOT:  .section
; CHECK:      hot_fn:
; CHECK:      .section .text.in_group,"axG",@progbits,in_group,comdat
; CHECK-NEXT: .weak in_group
; CHECK:      .section xdp,"ax",@progbits

; OBJ-NOT: .text.
; OBJ:     .text PROGBITS
; OBJ-NOT: .text.
; OBJ:     .text.in_group PROGBITS {{.*}} AXG
; OBJ-NOT: .text.
