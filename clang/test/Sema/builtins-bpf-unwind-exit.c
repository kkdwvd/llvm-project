// RUN: %clang_cc1 -triple bpfel -fsyntax-only -verify %s

// __builtin_bpf_unwind_exit() takes the exception object and does not
// return, so a function that ends with it needs no return statement.

int raise(void *obj) {
  __builtin_bpf_unwind_exit(obj);
}

void wrong(int i) {
  __builtin_bpf_unwind_exit(); // expected-error {{too few arguments to function call, expected 1, have 0}}
  __builtin_bpf_unwind_exit(i); // expected-error {{incompatible integer to pointer conversion passing 'int' to parameter of type 'void *'}}
}
