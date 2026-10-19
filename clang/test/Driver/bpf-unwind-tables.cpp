// The BPF backend unwinds through a function only if it has unwind tables, so
// enabling exceptions enables them, freestanding or not, and nothing else does.

// RUN: %clang -### --target=bpf -fexceptions -c %s 2>&1 | FileCheck --check-prefix=TABLES %s
// RUN: %clang -### --target=bpf -fcxx-exceptions -c %s 2>&1 | FileCheck --check-prefix=TABLES %s
// RUN: %clang -### --target=bpfel -fexceptions -ffreestanding -c %s 2>&1 | FileCheck --check-prefix=TABLES %s
// RUN: %clang -### --target=bpf -fexceptions -x c -c %s 2>&1 | FileCheck --check-prefix=TABLES %s
// RUN: %clang -### --target=bpf -fno-exceptions -fcxx-exceptions -c %s 2>&1 | FileCheck --check-prefix=TABLES %s
// RUN: %clang -### --target=bpf -fexceptions -fno-cxx-exceptions -c %s 2>&1 | FileCheck --check-prefix=TABLES %s
// RUN: %clang -### --target=bpf -fexceptions -fno-unwind-tables -c %s 2>&1 | FileCheck --check-prefix=NONE %s
// RUN: %clang -### --target=bpf -fexceptions -fno-exceptions -c %s 2>&1 | FileCheck --check-prefix=NONE %s
// RUN: %clang -### --target=bpf -fcxx-exceptions -fno-cxx-exceptions -c %s 2>&1 | FileCheck --check-prefix=NONE %s
// RUN: %clang -### --target=bpf -c %s 2>&1 | FileCheck --check-prefix=NONE %s
// RUN: %clang -### --target=bpf -x c -c %s 2>&1 | FileCheck --check-prefix=NONE %s

// TABLES: "-cc1"
// TABLES-SAME: "-funwind-tables=1"
// NONE: "-cc1"
// NONE-NOT: "-funwind-tables
