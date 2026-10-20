// RUN: %clang_cc1 -triple bpfel -O2 -disable-llvm-passes -emit-llvm -debug-info-kind=standalone %s -o - | FileCheck %s
// RUN: %clang_cc1 -triple bpfel -O2 -disable-llvm-passes -emit-llvm -debug-info-kind=constructor %s -o - | FileCheck %s
// RUN: %clang_cc1 -triple bpfel -O2 -disable-llvm-passes -emit-llvm %s -o - | FileCheck %s

// A C99 inline definition is emitted available externally so that the
// optimizer inlines it, as it is on every target: the author wrote it to be
// inlined, and a BPF program has no other definition of it anywhere. Only C++
// refers to an available-externally function as a declaration under debug
// info (bpf-available-externally.cpp), for the inline members of an explicit
// instantiation declaration, which the library defines out of line.

inline int f(int x) { return x + 1; }

int g(int x) { return f(x); }

// CHECK-DAG: call i32 @f(
// CHECK-DAG: define available_externally i32 @f(
