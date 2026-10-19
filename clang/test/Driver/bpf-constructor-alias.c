// BPF has no BTF for an alias, and libbpf links and loads no symbol without
// BTF: no constructor or destructor is emitted as an alias of another.

// RUN: %clang -### --target=bpf -c %s 2>&1 | FileCheck %s
// RUN: %clang -### --target=bpfel -x c++ -c %s 2>&1 | FileCheck %s

// CHECK: "-cc1"
// CHECK-NOT: "-mconstructor-aliases"
