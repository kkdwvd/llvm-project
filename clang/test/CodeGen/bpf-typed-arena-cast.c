// REQUIRES: bpf-registered-target
// RUN: %clang -target bpf -emit-llvm -S -g -Xclang -disable-llvm-passes %s -o - | FileCheck %s
// RUN: %clang -target bpf -emit-llvm -S -g -O2 %s -o - | FileCheck %s --check-prefix=OPT
// RUN: not %clang -target bpf -emit-llvm -S %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=NO-DEBUG

#define __arena __attribute__((address_space(1)))
#define __kptr __attribute__((btf_type_tag("kptr")))

struct task_struct;

struct node {
  struct task_struct *__kptr task;
  struct node *next;
  long value;
};

struct other {
  struct task_struct *__kptr task;
  long value;
};

#define cast(v, T) ((T *)__builtin_bpf_typed_arena_cast((v), *(T *)0))

// CHECK-LABEL: define dso_local i64 @from_arena
// CHECK: %[[P:[0-9]+]] = load ptr addrspace(1), ptr %p.addr
// CHECK: %[[V:[0-9]+]] = ptrtoint ptr addrspace(1) %[[P]] to i64
// CHECK: call ptr @llvm.bpf.typed.arena.cast(i64 %[[V]], ptr null), !dbg !{{[0-9]+}}, !llvm.preserve.access.index ![[NODE:[0-9]+]]
long from_arena(void __arena *p) { return cast(p, struct node)->value; }

// CHECK-LABEL: define dso_local i64 @from_int
// CHECK: %[[C:[0-9]+]] = zext i32 %{{[0-9]+}} to i64
// CHECK: call ptr @llvm.bpf.typed.arena.cast(i64 %[[C]], ptr null), !dbg !{{[0-9]+}}, !llvm.preserve.access.index ![[NODE]]
long from_int(unsigned int h) { return cast(h, struct node)->value; }

// CHECK-LABEL: define dso_local i64 @from_other
// CHECK: call ptr @llvm.bpf.typed.arena.cast(i64 %{{[0-9]+}}, ptr null), !dbg !{{[0-9]+}}, !llvm.preserve.access.index ![[OTHER:[0-9]+]]
long from_other(unsigned long v) { return cast(v, struct other)->value; }

// Two casts of one value to one type merge; a cast to another type does not.
// OPT-LABEL: define dso_local i64 @merge
// OPT: call ptr @llvm.bpf.typed.arena.cast(i64 %v, ptr nonnull @"llvm.btf_type_id.[[N:[0-9]+]]$6")
// OPT-NOT: @"llvm.btf_type_id.[[N]]$6"
// OPT: call ptr @llvm.bpf.typed.arena.cast(i64 %v, ptr nonnull @"llvm.btf_type_id.{{[0-9]+}}$6")
// OPT: ret i64
long merge(unsigned long v) {
  return cast(v, struct node)->value + cast(v, struct node)->value +
         cast(v, struct other)->value;
}

// CHECK: ![[NODE]] = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "node"
// CHECK: ![[OTHER]] = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "other"

// NO-DEBUG: error: using builtin function without -g
