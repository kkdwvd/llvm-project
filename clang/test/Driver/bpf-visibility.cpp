// libbpf makes a hidden function a static subprogram in BTF and a default one
// a global subprogram. C++ on BPF keeps C's rule for the functions the author
// writes, and hides what it cannot write otherwise: the inline member
// functions of classes and class templates, which would be weak global
// subprograms with untyped pointer parameters, and the global allocation
// functions, which take and return void pointers. An inline free function
// and a free function template's instance keep default visibility.
// -fno-visibility-inlines-hidden and -fvisibility-global-new-delete= say
// otherwise, and C is as it was.

// RUN: %clang -### --target=bpfel -c %s 2>&1 | FileCheck --check-prefix=CXX %s
// RUN: %clang -### --target=bpfeb -c %s 2>&1 | FileCheck --check-prefix=CXX %s
// RUN: %clang -### --target=bpfel -fvisibility=hidden -c %s 2>&1 | FileCheck --check-prefixes=CXX,HIDDEN %s
// CXX: "-cc1"
// HIDDEN-SAME: "-fvisibility=hidden"
// CXX-SAME: "-fvisibility-inlines-hidden"
// CXX-SAME: "-fvisibility-global-new-delete=force-hidden"

// RUN: %clang -### --target=bpfel -fno-visibility-inlines-hidden -c %s 2>&1 \
// RUN:   | FileCheck --check-prefix=NOINLINES %s
// NOINLINES: "-cc1"
// NOINLINES-NOT: "-fvisibility-inlines-hidden"
// NOINLINES-SAME: "-fvisibility-global-new-delete=force-hidden"

// RUN: %clang -### --target=bpfel -fvisibility-global-new-delete=source -c %s 2>&1 \
// RUN:   | FileCheck --check-prefix=SOURCE %s
// SOURCE: "-cc1"
// SOURCE-SAME: "-fvisibility-inlines-hidden"
// SOURCE-SAME: "-fvisibility-global-new-delete=source"
// SOURCE-NOT: "-fvisibility-global-new-delete=force-hidden"

// RUN: %clang -### --target=bpfel -x c -c %s 2>&1 | FileCheck --check-prefix=C %s
// RUN: %clang -### --target=x86_64-linux-gnu -c %s 2>&1 | FileCheck --check-prefix=C %s
// C: "-cc1"
// C-NOT: "-fvisibility

int main() { return 0; }
