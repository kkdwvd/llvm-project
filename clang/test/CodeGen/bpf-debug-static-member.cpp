// REQUIRES: bpf-registered-target
// RUN: %clang_cc1 -triple bpf -x c++ -std=c++20 -debug-info-kind=limited -emit-llvm %s -o - | FileCheck %s

// Class-scoped static data member declarations must not be treated as
// external (ksym-style) variable references for BTF debug info: the
// resulting debug-info global would be scoped to a type, which DIBuilder
// asserts on. Seen in the wild via libstdc++'s <coroutine>, whose
// noop_coroutine_handle declares a static frame member.

struct Frame {
  static Frame _S_fr;
};

Frame *get() { return &Frame::_S_fr; }

// CHECK-NOT: DIGlobalVariableExpression
