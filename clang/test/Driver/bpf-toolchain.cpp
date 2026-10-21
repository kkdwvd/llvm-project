// C++ against the BPF sysroot: libc++'s headers from include/<triple>/c++/v1
// and include/c++/v1 before the C headers, and the C++ ABI's cxxabi.h, which
// declares the runtime functions clang calls on its own, included in every
// unit.

// RUN: %clangxx -### --target=bpfel -c %s 2>&1 \
// RUN:     -ccc-install-dir %S/Inputs/basic_bpf_sysroot_tree/bin \
// RUN:     -resource-dir %S/Inputs/basic_bpf_sysroot_tree/resource \
// RUN:   | FileCheck -DTRIPLE=bpfel-unknown-none --check-prefix=SYSROOT %s
// RUN: %clangxx -### --target=bpfeb -c %s 2>&1 \
// RUN:     -ccc-install-dir %S/Inputs/basic_bpf_sysroot_tree/bin \
// RUN:     -resource-dir %S/Inputs/basic_bpf_sysroot_tree/resource \
// RUN:   | FileCheck -DTRIPLE=bpfeb-unknown-none --check-prefix=SYSROOT %s
// SYSROOT: "-cc1"
// SYSROOT-SAME: "-nostdsysteminc"
// SYSROOT-SAME: "-resource-dir" "[[RESOURCE:[^"]+]]"
// SYSROOT-SAME: "-internal-isystem" "[[ROOT:[^"]+]]/share/bpf-sysroot/include/[[TRIPLE]]/c++/v1"
// SYSROOT-SAME: "-internal-isystem" "[[ROOT]]/share/bpf-sysroot/include/c++/v1"
// SYSROOT-SAME: "-include" "[[ROOT]]/share/bpf-sysroot/include/[[TRIPLE]]/c++/v1/cxxabi.h"
// SYSROOT-SAME: "-internal-isystem" "[[RESOURCE]]/include"
// SYSROOT-SAME: "-internal-isystem" "[[ROOT]]/share/bpf-sysroot/include/[[TRIPLE]]"
// SYSROOT-SAME: "-internal-isystem" "[[ROOT]]/share/bpf-sysroot/include"
// SYSROOT-NOT: "-internal-isystem"
// SYSROOT-NOT: "-include"

// Without a sysroot there are no C++ headers, as before the toolchain.

// RUN: %clangxx -### --target=bpf -c %s 2>&1 | FileCheck --check-prefix=NOSYSROOT %s
// NOSYSROOT: "-cc1"
// NOSYSROOT-NOT: "-internal-isystem"
// NOSYSROOT-NOT: "-include"

// A sysroot with C headers alone has nothing to add for C++.

// RUN: %clangxx -### --target=bpfel -c %s 2>&1 \
// RUN:     --sysroot=%S/Inputs/bpf_c_sysroot_tree \
// RUN:   | FileCheck --check-prefix=CONLY %s
// CONLY: "-cc1"
// CONLY-NOT: "c++/v1"
// CONLY-NOT: "-include"
// CONLY-SAME: "-internal-isystem" "{{.*}}/Inputs/bpf_c_sysroot_tree/include/bpfel-unknown-none"
// CONLY-SAME: "-internal-isystem" "{{.*}}/Inputs/bpf_c_sysroot_tree/include"
// CONLY-NOT: "-include"

// -nostdinc++ leaves the C++ headers and cxxabi.h out, -nostdlibinc the
// sysroot's headers and -nostdinc all of them.

// RUN: %clangxx -### --target=bpfel -nostdinc++ -c %s 2>&1 \
// RUN:     -ccc-install-dir %S/Inputs/basic_bpf_sysroot_tree/bin \
// RUN:     -resource-dir %S/Inputs/basic_bpf_sysroot_tree/resource \
// RUN:   | FileCheck --check-prefix=NOSTDINCXX %s
// NOSTDINCXX: "-cc1"
// NOSTDINCXX-NOT: "c++/v1"
// NOSTDINCXX-NOT: "-include"
// NOSTDINCXX-SAME: "-internal-isystem" "{{.*}}/resource/include"
// NOSTDINCXX-SAME: "-internal-isystem" "{{.*}}/share/bpf-sysroot/include/bpfel-unknown-none"
// NOSTDINCXX-SAME: "-internal-isystem" "{{.*}}/share/bpf-sysroot/include"
// NOSTDINCXX-NOT: "-include"

// RUN: %clangxx -### --target=bpfel -nostdlibinc -c %s 2>&1 \
// RUN:     -ccc-install-dir %S/Inputs/basic_bpf_sysroot_tree/bin \
// RUN:     -resource-dir %S/Inputs/basic_bpf_sysroot_tree/resource \
// RUN:   | FileCheck --check-prefix=NOSTDLIBINC %s
// NOSTDLIBINC: "-cc1"
// NOSTDLIBINC-NOT: "-include"
// NOSTDLIBINC-SAME: "-internal-isystem" "{{.*}}/resource/include"
// NOSTDLIBINC-NOT: "-internal-isystem"
// NOSTDLIBINC-NOT: "-include"

// RUN: %clangxx -### --target=bpfel -nostdinc -c %s 2>&1 \
// RUN:     -ccc-install-dir %S/Inputs/basic_bpf_sysroot_tree/bin \
// RUN:     -resource-dir %S/Inputs/basic_bpf_sysroot_tree/resource \
// RUN:   | FileCheck --check-prefix=NOSTDINC %s
// NOSTDINC: "-cc1"
// NOSTDINC-NOT: "-internal-isystem"
// NOSTDINC-NOT: "-include"

// libc++ is the only C++ standard library.

// RUN: not %clangxx -### --target=bpfel -stdlib=libstdc++ -c %s 2>&1 \
// RUN:     -ccc-install-dir %S/Inputs/basic_bpf_sysroot_tree/bin \
// RUN:   | FileCheck --check-prefix=STDLIB %s
// STDLIB: error: invalid library name in argument '-stdlib=libstdc++'

// A C++ link adds libc++, libc++abi and libunwind from the sysroot's
// lib/<triple> after the inputs, before compiler-rt's builtins, libc and the
// builtins again, the order the libraries depend on each other, which
// libbpf's linker wants as ld does; -nostdlib++ leaves the three out, -nolibc
// libc, and -nostdlib all of them.

// RUN: %clangxx -### --target=bpfel %s -o out.o 2>&1 \
// RUN:     -ccc-install-dir %S/Inputs/basic_bpf_sysroot_tree/bin \
// RUN:     -resource-dir %S/Inputs/basic_bpf_sysroot_tree/resource \
// RUN:   | FileCheck --check-prefix=LINK %s
// LINK: "-cc1" {{.*}} "-o" "[[temp:[^"]*]]"
// LINK: bpftool{{(\.exe)?}}" "gen" "object" "out.o" "[[temp]]" "[[ROOT:[^"]+]]/share/bpf-sysroot/lib/bpfel-unknown-none/libc++.a" "[[ROOT]]/share/bpf-sysroot/lib/bpfel-unknown-none/libc++abi.a" "[[ROOT]]/share/bpf-sysroot/lib/bpfel-unknown-none/libunwind.a" "{{.*}}/resource/lib/bpfel-unknown-none/libclang_rt.builtins.a" "[[ROOT]]/share/bpf-sysroot/lib/bpfel-unknown-none/libc.a" "{{.*}}/resource/lib/bpfel-unknown-none/libclang_rt.builtins.a"{{$}}

// RUN: %clangxx -### --target=bpfel %s -o out.o -nostdlib++ 2>&1 \
// RUN:     -ccc-install-dir %S/Inputs/basic_bpf_sysroot_tree/bin \
// RUN:     -resource-dir %S/Inputs/basic_bpf_sysroot_tree/resource \
// RUN:   | FileCheck --check-prefix=LINK_NOSTDLIBXX %s
// LINK_NOSTDLIBXX: "-cc1" {{.*}} "-o" "[[temp:[^"]*]]"
// LINK_NOSTDLIBXX: bpftool{{(\.exe)?}}" "gen" "object" "out.o" "[[temp]]" "{{.*}}/resource/lib/bpfel-unknown-none/libclang_rt.builtins.a" "{{.*}}/share/bpf-sysroot/lib/bpfel-unknown-none/libc.a" "{{.*}}/resource/lib/bpfel-unknown-none/libclang_rt.builtins.a"{{$}}

// RUN: %clangxx -### --target=bpfel %s -o out.o -nolibc 2>&1 \
// RUN:     -ccc-install-dir %S/Inputs/basic_bpf_sysroot_tree/bin \
// RUN:     -resource-dir %S/Inputs/basic_bpf_sysroot_tree/resource \
// RUN:   | FileCheck --check-prefix=LINK_NOLIBC %s
// LINK_NOLIBC: "-cc1" {{.*}} "-o" "[[temp:[^"]*]]"
// LINK_NOLIBC: bpftool{{(\.exe)?}}" "gen" "object" "out.o" "[[temp]]" "[[ROOT:[^"]+]]/share/bpf-sysroot/lib/bpfel-unknown-none/libc++.a" "[[ROOT]]/share/bpf-sysroot/lib/bpfel-unknown-none/libc++abi.a" "[[ROOT]]/share/bpf-sysroot/lib/bpfel-unknown-none/libunwind.a" "{{.*}}/resource/lib/bpfel-unknown-none/libclang_rt.builtins.a"{{$}}

// RUN: %clangxx -### --target=bpfel %s -o out.o -nostdlib 2>&1 \
// RUN:     -ccc-install-dir %S/Inputs/basic_bpf_sysroot_tree/bin \
// RUN:     -resource-dir %S/Inputs/basic_bpf_sysroot_tree/resource \
// RUN:   | FileCheck --check-prefix=LINK_NOSTDLIB %s
// RUN: %clangxx -### --target=bpf %s -o out.o 2>&1 | FileCheck --check-prefix=LINK_NOSTDLIB %s
// LINK_NOSTDLIB: "-cc1" {{.*}} "-o" "[[temp:[^"]*]]"
// LINK_NOSTDLIB: bpftool{{(\.exe)?}}" "gen" "object" "out.o" "[[temp]]"{{$}}

int f() { return 0; }
