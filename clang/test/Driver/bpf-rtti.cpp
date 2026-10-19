// RTTI is on by default on BPF as everywhere but PS4/PS5 and DriverKit: BTF
// describes the type_info objects and vtables, and the runtime in the sysroot
// matches types for catch and dynamic_cast. -fno-rtti turns it off.

// RUN: %clang -### -c --target=bpfel %s 2>&1 | FileCheck -check-prefix=CHECK-RTTI %s
// RUN: %clang -### -c --target=bpfeb %s 2>&1 | FileCheck -check-prefix=CHECK-RTTI %s
// RUN: %clang -### -c --target=bpfel -frtti %s 2>&1 | FileCheck -check-prefix=CHECK-RTTI %s
// RUN: %clang -### -c --target=bpfel -fno-rtti %s 2>&1 | FileCheck -check-prefix=CHECK-NO-RTTI %s

int main() { return 0; }
// CHECK-RTTI-NOT: "-fno-rtti"
// CHECK-NO-RTTI: "-fno-rtti"
