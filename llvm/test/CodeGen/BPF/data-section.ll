; RUN: llc -mtriple=bpfel < %s | FileCheck %s
; RUN: llc -mtriple=bpfel -data-sections < %s | FileCheck %s
; RUN: llc -mtriple=bpfel -data-sections -partition-static-data-sections < %s | FileCheck %s
; RUN: llc -mtriple=bpfel -filetype=obj -o %t < %s
; RUN: llvm-readelf -S %t | FileCheck --check-prefix=OBJ %s
; RUN: llc -mtriple=bpfel -data-sections -filetype=obj -o %t < %s
; RUN: llvm-readelf -S %t | FileCheck --check-prefix=OBJ %s

; libbpf makes a map of each data section, named after it: .data, .rodata and
; .bss are the maps of the global data, .data.<name> and .rodata.<name> the
; custom maps a program asks for by naming them, and a program can use 64 maps.
; Data goes to the section of its kind unless its section attribute names
; another: no section of its own for -fdata-sections, and no .hot or .unlikely
; prefix for -fpartition-static-data-sections. Mergeable strings and constants
; go to .rodata.str1.<n> and .rodata.cst<n> as before. Data in a COMDAT group
; gets a section of the group, named after its symbol, which libbpf's linker
; and loader join to the base section.

$group_data = comdat any
$group_rodata = comdat any
$group_bss = comdat any

@data = global i32 1
@hot_data = global i32 2, !section_prefix !0
@bss = global i32 0
@cold_bss = global i32 0, !section_prefix !1
@rodata = constant i32 3
@local_rodata = internal constant [4 x i8] c"abc\00"
@relro = constant ptr @data
@.str = private unnamed_addr constant [4 x i8] c"abc\00"
@cst = private unnamed_addr constant i64 42
@group_data = linkonce_odr global i32 1, comdat
@group_rodata = linkonce_odr constant i32 1, comdat
@group_bss = linkonce_odr global i32 0, comdat
@custom = global i32 1, section ".data.custom"
@map = global { [1 x ptr] } zeroinitializer, section ".maps"
@_license = global [4 x i8] c"GPL\00", section "license"
@arena = addrspace(1) global i32 0

!0 = !{!"section_prefix", !"hot"}
!1 = !{!"section_prefix", !"unlikely"}

; CHECK:      .data
; CHECK-NOT:  .section
; CHECK:      data:
; CHECK-NOT:  .section
; CHECK:      hot_data:
; CHECK:      .section .bss,"aw",@nobits
; CHECK-NOT:  .section
; CHECK:      bss:
; CHECK-NOT:  .section
; CHECK:      cold_bss:
; CHECK:      .section .rodata,"a",@progbits
; CHECK-NOT:  .section
; CHECK:      rodata:
; CHECK-NOT:  .section
; CHECK:      local_rodata:
; CHECK:      .section .data.rel.ro,"aw",@progbits
; CHECK-NOT:  .section
; CHECK:      relro:
; CHECK:      .section .rodata.str1.1,"aMS",@progbits,1
; CHECK-NEXT: .L.str:
; CHECK:      .section .rodata.cst8,"aM",@progbits,8
; CHECK-NOT:  .section
; CHECK:      .Lcst:
; CHECK:      .section .data.group_data,"awG",@progbits,group_data,comdat
; CHECK-NEXT: .weak group_data
; CHECK:      .section .rodata.group_rodata,"aG",@progbits,group_rodata,comdat
; CHECK-NEXT: .weak group_rodata
; CHECK:      .section .bss.group_bss,"awG",@nobits,group_bss,comdat
; CHECK-NEXT: .weak group_bss
; CHECK:      .section .data.custom,"aw",@progbits
; CHECK-NOT:  .section
; CHECK:      custom:
; CHECK:      .section .maps,"aw",@progbits
; CHECK-NOT:  .section
; CHECK:      map:
; CHECK:      .section license,"aw",@progbits
; CHECK-NOT:  .section
; CHECK:      _license:
; CHECK:      .section .addr_space.1,"aw",@progbits
; CHECK-NOT:  .section
; CHECK:      arena:

; OBJ:      .data PROGBITS {{.*}} WA
; OBJ-NEXT: .bss NOBITS {{.*}} WA
; OBJ-NEXT: .rodata PROGBITS {{.*}} A
; OBJ-NEXT: .data.rel.ro PROGBITS {{.*}} WA
; OBJ-NEXT: .rel.data.rel.ro REL
; OBJ-NEXT: .rodata.str1.1 PROGBITS {{.*}} AMS
; OBJ-NEXT: .rodata.cst8 PROGBITS {{.*}} AM
; OBJ-NEXT: .group GROUP
; OBJ-NEXT: .data.group_data PROGBITS {{.*}} WAG
; OBJ-NEXT: .group GROUP
; OBJ-NEXT: .rodata.group_rodata PROGBITS {{.*}} AG
; OBJ-NEXT: .group GROUP
; OBJ-NEXT: .bss.group_bss NOBITS {{.*}} WAG
; OBJ-NEXT: .data.custom PROGBITS {{.*}} WA
; OBJ-NEXT: .maps PROGBITS {{.*}} WA
; OBJ-NEXT: license PROGBITS {{.*}} WA
; OBJ-NEXT: .addr_space.1 PROGBITS {{.*}} WA
; OBJ-NEXT: .symtab SYMTAB
