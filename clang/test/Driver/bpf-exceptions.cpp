// C++ exceptions are on by default on BPF, as on hosted targets: the backend
// lowers unwinding to the instructions the kernel verifies, and the runtime
// comes with the sysroot. -fno-exceptions and -fno-cxx-exceptions turn them
// off, and C has none.

// RUN: %clang -### --target=bpfel -c %s 2>&1 | FileCheck %s -check-prefix=DEFAULT
// RUN: %clang -### --target=bpfeb -c %s 2>&1 | FileCheck %s -check-prefix=DEFAULT
// RUN: %clang -### --target=bpfel -fexceptions -c %s 2>&1 | FileCheck %s -check-prefix=DEFAULT
// RUN: %clang -### --target=bpfel -fno-exceptions -c %s 2>&1 | FileCheck %s -check-prefix=OFF
// RUN: %clang -### --target=bpfel -fno-cxx-exceptions -c %s 2>&1 | FileCheck %s -check-prefix=OFF
// RUN: %clang -### --target=bpfel -x c -c %s 2>&1 | FileCheck %s -check-prefix=OFF

int main() { return 0; }
// DEFAULT: "-fcxx-exceptions"
// DEFAULT-SAME: "-fexceptions"
// OFF-NOT: "-fcxx-exceptions"
// OFF-NOT: "-fexceptions"
