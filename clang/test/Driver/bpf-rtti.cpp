// RUN: %clang -### -c --target=bpfel %s 2>&1 | FileCheck -check-prefix=CHECK-NO-RTTI %s
// RUN: %clang -### -c --target=bpfel -frtti %s 2>&1 | FileCheck -check-prefix=CHECK-RTTI %s

int main() { return 0; }
// CHECK-NO-RTTI: "-fno-rtti"
// CHECK-RTTI-NOT: "-fno-rtti"
