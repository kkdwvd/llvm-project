// BTF is built from the debug info of each object alone, and libbpf loads no
// map or global whose type it leaves declared, as limited debug info does with
// a C++ class whose constructor another unit defines: -g is standalone for
// C++ on BPF. C keeps the debug info, and the BTF, it had.

// RUN: %clang -### --target=bpfel -g -c %s 2>&1 | FileCheck --check-prefix=STANDALONE %s
// RUN: %clang -### --target=bpfeb -g -c %s 2>&1 | FileCheck --check-prefix=STANDALONE %s
// RUN: %clang -### --target=bpfel -g -fstandalone-debug -c %s 2>&1 | FileCheck --check-prefix=STANDALONE %s
// STANDALONE: "-debug-info-kind=standalone"

// RUN: %clang -### --target=bpfel -g -fno-standalone-debug -c %s 2>&1 | FileCheck --check-prefix=LIMITED %s
// RUN: %clang -### --target=bpfel -g -x c -c %s 2>&1 | FileCheck --check-prefix=LIMITED %s
// LIMITED: "-debug-info-kind=constructor"

// RUN: %clang -### --target=bpfel -gline-tables-only -c %s 2>&1 | FileCheck --check-prefix=LINES %s
// LINES: "-debug-info-kind=line-tables-only"

// RUN: %clang -### --target=bpfel -c %s 2>&1 | FileCheck --check-prefix=NODEBUG %s
// NODEBUG-NOT: "-debug-info-kind

int main() { return 0; }
