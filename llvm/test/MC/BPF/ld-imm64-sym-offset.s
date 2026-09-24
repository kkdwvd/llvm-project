# RUN: llvm-mc -triple bpfel -filetype=obj -o %t %s
# RUN: llvm-objdump --no-print-imm-hex -d -r %t | FileCheck %s

// A 64-bit immediate load accepts a symbol plus a constant offset. The
// assembler writes the resolved offset into the instruction and emits an
// R_BPF_64_64 relocation for the symbol.

  .section .rodata,"a",@progbits
  .balign 8
tbl:
  .quad 1
  .quad 2
  .size tbl, .-tbl

  .text
  r1 = tbl ll
  r2 = tbl + 8 ll
  r3 = ext_sym + 16 ll
  exit

// CHECK: 18 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 	r1 = 0 ll
// CHECK-NEXT: R_BPF_64_64 .rodata
// CHECK: 18 02 00 00 08 00 00 00 00 00 00 00 00 00 00 00 	r2 = 8 ll
// CHECK-NEXT: R_BPF_64_64 .rodata
// CHECK: 18 03 00 00 10 00 00 00 00 00 00 00 00 00 00 00 	r3 = 16 ll
// CHECK-NEXT: R_BPF_64_64 ext_sym
// CHECK: 95 00 00 00 00 00 00 00 	exit
