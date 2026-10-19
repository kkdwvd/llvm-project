// C++ on the BPF toolchain has no C++ headers, as before the toolchain, and a
// link with clang++ adds no library.

// RUN: %clangxx -### --target=bpf -c %s 2>&1 | FileCheck --check-prefix=CC1 %s
// CC1: "-cc1"
// CC1-NOT: "-internal-isystem"
// CC1-NOT: "-include"

// RUN: %clangxx -### --target=bpf %s -o out.o 2>&1 | FileCheck --check-prefix=LINK %s
// LINK: "-cc1" {{.*}} "-o" "[[temp:[^"]*]]"
// LINK: bpftool{{(\.exe)?}}" "gen" "object" "out.o" "[[temp]]"{{$}}

int f() { return 0; }
