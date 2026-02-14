// RUN: %clang_cc1 -x c++ -triple bpf-pc-linux-gnu -dwarf-version=4 -fsyntax-only -verify %s

#define __reloc__ __attribute__((preserve_access_index))
#define __err_reloc__ __attribute__((preserve_access_index(0)))

// The attribute should be accepted on structs in C++ mode.
struct t1 {
  int a;
  int b[4];
  int c:1;
} __reloc__;

union t2 {
  int a;
  int b[4];
  int c:1;
} __reloc__;

struct t3 {
  int a;
} __err_reloc__; // expected-error {{'preserve_access_index' attribute takes no arguments}}

// The attribute should be accepted on C++ classes.
class t4 {
  int x;
public:
  int get() const { return x; }
  void set(int v) { x = v; }
} __reloc__;

// Class with inner struct.
class t5 {
  struct inner {
    int a;
    int b;
  } __reloc__;
  inner s;
public:
  int get_a() const { return s.a; }
} __reloc__;

// Errors on non-record types should still be reported.
int a __reloc__; // expected-error {{'preserve_access_index' attribute only applies to structs, unions, and classes}}
struct s *p __reloc__; // expected-error {{'preserve_access_index' attribute only applies to structs, unions, and classes}}
