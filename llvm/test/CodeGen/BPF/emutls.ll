; RUN: llc -mtriple=bpfel -mcpu=v4 -o - %s | FileCheck %s
; RUN: llc -mtriple=bpfel -mcpu=v4 -enable-new-pm -o - %s | FileCheck %s
; RUN: llc -mtriple=bpfel -mcpu=v4 -emulated-tls -o - %s | FileCheck %s
; RUN: not llc -mtriple=bpfel -mcpu=v4 -emulated-tls=0 -o /dev/null %s 2>&1 \
; RUN:   | FileCheck --check-prefix=NOEMU %s

; Thread-local storage is emulated on BPF, by default, as on Android before
; 29, OpenBSD and Cygwin: LowerEmuTLS gives a defined variable its
; __emutls_v. control block and, when its initial value is not zero, the
; __emutls_t. template, and an access is a call of __emutls_get_address()
; with the block's address, whose result is the variable's address on the
; current thread, whatever the runtime takes a thread to be. The variables
; themselves are not emitted. Both pass managers lower it. BPF has no thread
; pointer, so there is no other model: asking for one is an error.

@counter = dso_local thread_local global i32 5, align 4
@hits = internal thread_local global i64 0, align 8
@other = external thread_local global i32, align 4

define i32 @bump() {
  %h = call ptr @llvm.threadlocal.address.p0(ptr @hits)
  %hv = load i64, ptr %h
  %hn = add i64 %hv, 1
  store i64 %hn, ptr %h
  %c = call ptr @llvm.threadlocal.address.p0(ptr @counter)
  %cv = load i32, ptr %c
  %cn = add i32 %cv, 1
  store i32 %cn, ptr %c
  %o = call ptr @llvm.threadlocal.address.p0(ptr @other)
  %ov = load i32, ptr %o
  %r = add i32 %cv, %ov
  ret i32 %r
}

declare ptr @llvm.threadlocal.address.p0(ptr)

; CHECK-LABEL: bump:
; CHECK:         r1 = __emutls_v.hits ll
; CHECK-NEXT:    call __emutls_get_address
; CHECK-NEXT:    r1 = *(u64 *)(r0 + 0)
; CHECK-NEXT:    r1 += 1
; CHECK-NEXT:    *(u64 *)(r0 + 0) = r1
; CHECK-NEXT:    r1 = __emutls_v.counter ll
; CHECK-NEXT:    call __emutls_get_address
; CHECK:         r1 = __emutls_v.other ll
; CHECK-NEXT:    call __emutls_get_address
; CHECK:         exit

; The control block is a word for the size, one for the alignment, the
; address on the current thread, null at first, and the template, in .data
; like any initialized variable; the template is in .rodata. A variable with
; no initial value has no template, and a declaration has neither.
; CHECK:       .type __emutls_v.counter,@object
; CHECK-NEXT:  .data
; CHECK-NEXT:  .globl __emutls_v.counter
; CHECK-NEXT:  .p2align 3, 0x0
; CHECK-NEXT: __emutls_v.counter:
; CHECK:       .quad 4
; CHECK-NEXT:  .quad 4
; CHECK-NEXT:  .quad 0
; CHECK-NEXT:  .quad __emutls_t.counter
; CHECK-NEXT:  .size __emutls_v.counter, 32
; CHECK:       .type __emutls_t.counter,@object
; CHECK-NEXT:  .section .rodata,"a",@progbits
; CHECK-NEXT:  .globl __emutls_t.counter
; CHECK-NEXT:  .p2align 2, 0x0
; CHECK-NEXT: __emutls_t.counter:
; CHECK:       .long 5
; CHECK-NEXT:  .size __emutls_t.counter, 4
; CHECK:       .type __emutls_v.hits,@object
; CHECK-NEXT:  .data
; CHECK-NEXT:  .p2align 3, 0x0
; CHECK-NEXT: __emutls_v.hits:
; CHECK-NEXT:  .quad 8
; CHECK-NEXT:  .quad 8
; CHECK-NEXT:  .quad 0
; CHECK-NEXT:  .quad 0
; CHECK-NEXT:  .size __emutls_v.hits, 32
; CHECK-NOT:  __emutls_t.hits
; CHECK-NOT:  __emutls_v.other:
; CHECK-NOT:  counter:
; CHECK-NOT:  hits:
; CHECK-NOT:  .tdata
; CHECK-NOT:  .tbss

; NOEMU: error: {{.*}}in function bump i32 (): thread-local storage is supported only as emulated TLS
