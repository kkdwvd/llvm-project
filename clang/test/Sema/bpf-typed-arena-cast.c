// RUN: %clang_cc1 -x c -triple bpf-pc-linux-gnu -fsyntax-only -verify %s

struct node {
  long value;
};

union u {
  long value;
};

void *ok_ptr(void *p) { return __builtin_bpf_typed_arena_cast(p, *(struct node *)0); }
void *ok_int(unsigned int h) { return __builtin_bpf_typed_arena_cast(h, *(struct node *)0); }
void *ok_union(long v) { return __builtin_bpf_typed_arena_cast(v, *(union u *)0); }

void *bad_value(struct node n) {
  return __builtin_bpf_typed_arena_cast(n, *(struct node *)0); // expected-error {{__builtin_bpf_typed_arena_cast argument 1 is not a pointer or integer}}
}

void *bad_type(long v) {
  return __builtin_bpf_typed_arena_cast(v, *(long *)0); // expected-error {{__builtin_bpf_typed_arena_cast argument 2 is not a struct or union}}
}

void *bad_count(long v) {
  return __builtin_bpf_typed_arena_cast(v); // expected-error {{too few arguments to function call, expected 2, have 1}}
}
