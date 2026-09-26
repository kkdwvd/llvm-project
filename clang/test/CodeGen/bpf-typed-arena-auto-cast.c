// REQUIRES: bpf-registered-target
// RUN: %clang -target bpf -Xclang -target-feature -Xclang +typed-arena -emit-llvm -S -g -Xclang -disable-llvm-passes %s -o - | FileCheck %s
// RUN: %clang -target bpf -Xclang -target-feature -Xclang +typed-arena -emit-llvm -S -g -O2 %s -o - | FileCheck %s --check-prefix=OPT
// RUN: %clang -target bpf -emit-llvm -S -g -O2 %s -o - | FileCheck %s --check-prefix=OFF
// RUN: %clang -target bpf -Xclang -target-feature -Xclang +typed-arena -emit-llvm -S -O2 -DNO_BUILTIN %s -o - | FileCheck %s --check-prefix=NODBG

// With the typed-arena feature, a pointer to a typed record is cast wherever
// it is used as an address, and a value converted to such a pointer is cast
// at the conversion. Without the feature, or without debug information to
// name the record with, nothing is inserted.

#define __arena __attribute__((address_space(1)))
#define __kptr __attribute__((btf_type_tag("kptr")))
#define NULL ((void *)0)

struct task_struct;

struct node {
  struct task_struct __kptr *task;
  struct node *next;
  long value;
};

/* Typed only through its pointer to a typed record. */
struct head {
  struct node *first;
  long count;
};

/* Only pointers to itself and no special field: not typed. */
struct plain {
  struct plain *next;
  long value;
};

extern void *alloc(void);

// A walk casts the head before its first access and the node before each
// dereference; the test against NULL compares the raw value.
// CHECK-LABEL: define dso_local i64 @sum
// CHECK: %[[H:[0-9]+]] = ptrtoint ptr %{{[0-9]+}} to i64
// CHECK: call ptr @llvm.bpf.typed.arena.cast(i64 %[[H]], ptr null), !dbg !{{[0-9]+}}, !llvm.preserve.access.index ![[HEAD:[0-9]+]]
// CHECK: icmp ne ptr %{{[0-9]+}}, null
// CHECK-COUNT-2: call ptr @llvm.bpf.typed.arena.cast(i64 %{{[0-9]+}}, ptr null), !dbg !{{[0-9]+}}, !llvm.preserve.access.index ![[NODE:[0-9]+]]
// CHECK-NOT: call ptr @llvm.bpf.typed.arena.cast
// CHECK: ret i64
//
// Optimized, the node is one value per iteration and is cast once.
// OPT-LABEL: define dso_local i64 @sum
// OPT-COUNT-2: call ptr @llvm.bpf.typed.arena.cast(i64 %{{[a-z0-9.]+}}, ptr nonnull @"llvm.btf_type_id.{{[0-9]+}}$6")
// OPT-NOT: call ptr @llvm.bpf.typed.arena.cast
long sum(struct head *h)
{
  long s = 0;
  for (struct node *n = h->first; n; n = n->next)
    s += n->value;
  return s;
}

// Storing a typed pointer or NULL into a typed pointer field casts only the
// base; a scalar converted to a typed pointer is cast at the conversion.
// OPT-LABEL: define dso_local void @link
// OPT-COUNT-3: call ptr @llvm.bpf.typed.arena.cast
// OPT-NOT: call ptr @llvm.bpf.typed.arena.cast
void link(struct node *a, struct node *b, unsigned long v)
{
  a->next = b;
  b->next = (struct node *)v;
  b->value = 0;
  a->next = 0;
}

// The list terminator is stored as it is: a cast would turn it into object 0
// of the slice. Only the base is cast.
// CHECK-LABEL: define dso_local void @terminate
// CHECK-COUNT-1: call ptr @llvm.bpf.typed.arena.cast
// CHECK-NOT: call ptr @llvm.bpf.typed.arena.cast
// CHECK: store ptr null, ptr %next
// OPT-LABEL: define dso_local void @terminate
// OPT-COUNT-1: call ptr @llvm.bpf.typed.arena.cast
// OPT-NOT: call ptr @llvm.bpf.typed.arena.cast
void terminate(struct node *n)
{
  n->next = NULL;
}

// A void pointer converted to a typed pointer is cast at the conversion. The
// pointer then goes through a local, which the use casts again: the kernel
// lowers a cast of a pointer it already trusts to nothing.
// OPT-LABEL: define dso_local i64 @from_void
// OPT-COUNT-2: call ptr @llvm.bpf.typed.arena.cast
// OPT-NOT: call ptr @llvm.bpf.typed.arena.cast
long from_void(void)
{
  struct node *n = alloc();
  return n->value;
}

// The explicit builtin's result, used directly, is not cast again. The
// builtin needs debug information, so the run without it leaves it out.
#ifndef NO_BUILTIN
// OPT-LABEL: define dso_local i64 @explicit
// OPT-COUNT-1: call ptr @llvm.bpf.typed.arena.cast
// OPT-NOT: call ptr @llvm.bpf.typed.arena.cast
long explicit(unsigned long v)
{
  return ((struct node *)__builtin_bpf_typed_arena_cast(v, *(struct node *)0))->value;
}
#endif

// The kptr member's value is not a typed pointer; only its base is cast.
// OPT-LABEL: define dso_local ptr @kptr
// OPT-COUNT-1: call ptr @llvm.bpf.typed.arena.cast
// OPT-NOT: call ptr @llvm.bpf.typed.arena.cast
struct task_struct *kptr(struct node *n)
{
  return n->task;
}

// A typed record in raw arena memory is raw bytes.
// OPT-LABEL: define dso_local i64 @raw
// OPT-NOT: call ptr @llvm.bpf.typed.arena.cast
long raw(struct node __arena *n)
{
  return n->value;
}

// A struct with nothing but pointers to itself is not typed.
// OPT-LABEL: define dso_local i64 @plain_sum
// OPT-NOT: call ptr @llvm.bpf.typed.arena.cast
long plain_sum(struct plain *p)
{
  long s = 0;
  for (; p; p = p->next)
    s += p->value;
  return s;
}

// CHECK-DAG: ![[HEAD]] = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "head"
// CHECK-DAG: ![[NODE]] = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "node"

// Without the feature, or without debug information, the explicit builtin is
// the only source of casts.
// OFF-LABEL: define dso_local i64 @sum
// OFF-NOT: call ptr @llvm.bpf.typed.arena.cast
// OFF-LABEL: define dso_local i64 @explicit
// OFF: call ptr @llvm.bpf.typed.arena.cast
// OFF-LABEL: define dso_local ptr @kptr
// OFF-NOT: call ptr @llvm.bpf.typed.arena.cast
// NODBG-LABEL: define dso_local i64 @sum
// NODBG-NOT: call ptr @llvm.bpf.typed.arena.cast
// NODBG-LABEL: define dso_local void @link
