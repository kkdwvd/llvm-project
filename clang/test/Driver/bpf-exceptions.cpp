// RUN: %clang %s --target=bpfel -### 2>&1 | FileCheck %s -check-prefix=DEFAULT
// RUN: %clang %s --target=bpfel -fexceptions -### 2>&1 | FileCheck %s -check-prefix=USERPROVIDED

int main() { return 0; }
// DEFAULT-NOT: "-fcxx-exceptions"
// DEFAULT-NOT: "-fexceptions"
// USERPROVIDED: "-fcxx-exceptions"
// USERPROVIDED: "-fexceptions"
