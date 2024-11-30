// RUN: %clang_cc1 -fsyntax-only -verify %s
// RUN: %clang_cc1 -fsyntax-only -verify -x c %s

// expected-no-diagnostics

// The attribute is accepted in C++ as in C: BPF programs in C++ take the
// kernel's types, which carry it, from vmlinux.h.
#ifdef __cplusplus
static_assert(__builtin_is_implicit_lifetime(int __attribute__((btf_type_tag("user"))) *));
#endif
int __attribute__((btf_type_tag("user"))) *ptr;
