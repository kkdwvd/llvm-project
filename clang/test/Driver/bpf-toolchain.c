// The BPF toolchain compiles against a sysroot when there is one, --sysroot or
// share/bpf-sysroot next to the installation, with include/<triple> in it.
// Without one a compilation is what it was before the toolchain: the driver
// adds no include directory and the frontend adds the host's, as it does for
// any target without an OS.

// RUN: %clang -### --target=bpf -c %s 2>&1 | FileCheck --check-prefix=NOSYSROOT %s
// RUN: %clang -### --target=bpfel -c %s 2>&1 | FileCheck --check-prefix=NOSYSROOT %s
// RUN: %clang -### --target=bpfeb -c %s 2>&1 | FileCheck --check-prefix=NOSYSROOT %s
// RUN: %clang -### --target=bpfel-unknown-none -c %s 2>&1 | FileCheck --check-prefix=NOSYSROOT %s
// RUN: %clang -### --target=bpf -ccc-install-dir %S/Inputs/basic_baremetal_tree/bin -c %s 2>&1 \
// RUN:   | FileCheck --check-prefix=NOSYSROOT %s
// NOSYSROOT: "-cc1"
// NOSYSROOT-NOT: "-nostdsysteminc"
// NOSYSROOT-NOT: "-isysroot"
// NOSYSROOT-NOT: "-internal-isystem"
// NOSYSROOT-NOT: "-internal-externc-isystem"
// NOSYSROOT-NOT: "-target-cpu"
// NOSYSROOT-NOT: "-fvisibility

// A --sysroot without include/<triple> is not a BPF sysroot but a root for the
// kernel's headers, which the frontend finds under it as it did before.

// RUN: %clang -### --target=bpf --sysroot=%S/Inputs/bpf_rootfs_tree -c %s 2>&1 \
// RUN:   | FileCheck --check-prefix=ROOTFS %s
// ROOTFS: "-cc1"
// ROOTFS-NOT: "-nostdsysteminc"
// ROOTFS-SAME: "-isysroot" "{{.*}}/Inputs/bpf_rootfs_tree"
// ROOTFS-NOT: "-internal-isystem"

// The sysroot next to the installation: the frontend adds no directory of the
// host's, and the sysroot's per-target headers come before its shared ones.

// RUN: %clang -### --target=bpfel -c %s 2>&1 \
// RUN:     -ccc-install-dir %S/Inputs/basic_bpf_sysroot_tree/bin \
// RUN:     -resource-dir %S/Inputs/basic_bpf_sysroot_tree/resource \
// RUN:   | FileCheck -DTRIPLE=bpfel-unknown-none --check-prefix=SYSROOT %s
// RUN: %clang -### --target=bpfeb -c %s 2>&1 \
// RUN:     -ccc-install-dir %S/Inputs/basic_bpf_sysroot_tree/bin \
// RUN:     -resource-dir %S/Inputs/basic_bpf_sysroot_tree/resource \
// RUN:   | FileCheck -DTRIPLE=bpfeb-unknown-none --check-prefix=SYSROOT %s
// SYSROOT: "-cc1"
// SYSROOT-SAME: "-nostdsysteminc"
// SYSROOT-SAME: "-resource-dir" "[[RESOURCE:[^"]+]]"
// SYSROOT-SAME: "-internal-isystem" "[[RESOURCE]]/include"
// SYSROOT-SAME: "-internal-isystem" "[[ROOT:[^"]+]]/share/bpf-sysroot/include/[[TRIPLE]]"
// SYSROOT-SAME: "-internal-isystem" "[[ROOT]]/share/bpf-sysroot/include"
// SYSROOT-NOT: "-internal-isystem"
// SYSROOT-NOT: "-include"

// --target=bpf is the host's byte order, and names the same directories.

// RUN: %clang -### --target=bpf -c %s 2>&1 \
// RUN:     -ccc-install-dir %S/Inputs/basic_bpf_sysroot_tree/bin \
// RUN:   | FileCheck --check-prefix=HOSTORDER %s
// HOSTORDER: "-internal-isystem" "{{.*}}/share/bpf-sysroot/include/bpf{{el|eb}}-unknown-none"

// --sysroot names the sysroot outright.

// RUN: %clang -### --target=bpfeb -c %s 2>&1 \
// RUN:     --sysroot=%S/Inputs/basic_bpf_sysroot_tree/share/bpf-sysroot \
// RUN:   | FileCheck --check-prefix=EXPLICIT %s
// EXPLICIT: "-cc1"
// EXPLICIT-SAME: "-nostdsysteminc"
// EXPLICIT-SAME: "-isysroot" "[[ROOT:[^"]+]]"
// EXPLICIT-SAME: "-internal-isystem" "{{[^"]+}}/include"
// EXPLICIT-SAME: "-internal-isystem" "[[ROOT]]/include/bpfeb-unknown-none"
// EXPLICIT-SAME: "-internal-isystem" "[[ROOT]]/include"
// EXPLICIT-NOT: "-internal-isystem"

// -nostdinc, -nostdlibinc and -nobuiltininc leave out what they do elsewhere.

// RUN: %clang -### --target=bpfel -nostdinc -c %s 2>&1 \
// RUN:     -ccc-install-dir %S/Inputs/basic_bpf_sysroot_tree/bin \
// RUN:     -resource-dir %S/Inputs/basic_bpf_sysroot_tree/resource \
// RUN:   | FileCheck --check-prefix=NOSTDINC %s
// NOSTDINC: "-cc1"
// NOSTDINC-NOT: "-internal-isystem"

// RUN: %clang -### --target=bpfel -nostdlibinc -c %s 2>&1 \
// RUN:     -ccc-install-dir %S/Inputs/basic_bpf_sysroot_tree/bin \
// RUN:     -resource-dir %S/Inputs/basic_bpf_sysroot_tree/resource \
// RUN:   | FileCheck --check-prefix=NOSTDLIBINC %s
// NOSTDLIBINC: "-cc1"
// NOSTDLIBINC-SAME: "-internal-isystem" "{{.*}}/resource/include"
// NOSTDLIBINC-NOT: "-internal-isystem"

// RUN: %clang -### --target=bpfel -nobuiltininc -c %s 2>&1 \
// RUN:     -ccc-install-dir %S/Inputs/basic_bpf_sysroot_tree/bin \
// RUN:     -resource-dir %S/Inputs/basic_bpf_sysroot_tree/resource \
// RUN:   | FileCheck --check-prefix=NOBUILTININC %s
// NOBUILTININC: "-cc1"
// NOBUILTININC-NOT: "{{.*}}/resource/include"
// NOBUILTININC-SAME: "-internal-isystem" "{{.*}}/share/bpf-sysroot/include/bpfel-unknown-none"
// NOBUILTININC-SAME: "-internal-isystem" "{{.*}}/share/bpf-sysroot/include"

// The CPU is the backend's default unless -mcpu says otherwise, sysroot or
// not.

// RUN: %clang -### --target=bpf -mcpu=v3 -c %s 2>&1 | FileCheck --check-prefix=MCPU %s
// RUN: %clang -### --target=bpfel -mcpu=v3 -c %s 2>&1 \
// RUN:     -ccc-install-dir %S/Inputs/basic_bpf_sysroot_tree/bin \
// RUN:   | FileCheck --check-prefix=MCPU %s
// MCPU: "-target-cpu" "v3"

// Linking is bpftool gen object with the inputs in their order. Without a
// sysroot nothing else is linked.

// RUN: %clang -### --target=bpf %s -o out.o 2>&1 | FileCheck --check-prefix=LINK %s
// LINK: "-cc1" {{.*}} "-o" "[[temp:[^"]*]]"
// LINK: bpftool{{(\.exe)?}}" "gen" "object" "out.o" "[[temp]]"{{$}}

// With one, compiler-rt's builtins from the resource directory, libc.a from
// the sysroot's lib/<triple> and the builtins again come after the inputs,
// GCC's libgcc, libc, libgcc, since each calls the other and libbpf links an
// archive against the references before it only; -nolibc leaves libc out,
// and the builtins' second copy with it, -nostdlib and -nodefaultlibs all.

// RUN: %clang -### --target=bpfel %s -o out.o 2>&1 \
// RUN:     -ccc-install-dir %S/Inputs/basic_bpf_sysroot_tree/bin \
// RUN:     -resource-dir %S/Inputs/basic_bpf_sysroot_tree/resource \
// RUN:   | FileCheck --check-prefix=LINK_SYSROOT %s
// LINK_SYSROOT: "-cc1" {{.*}} "-o" "[[temp:[^"]*]]"
// LINK_SYSROOT: bpftool{{(\.exe)?}}" "gen" "object" "out.o" "[[temp]]" "{{.*}}/resource/lib/bpfel-unknown-none/libclang_rt.builtins.a" "{{.*}}/share/bpf-sysroot/lib/bpfel-unknown-none/libc.a" "{{.*}}/resource/lib/bpfel-unknown-none/libclang_rt.builtins.a"{{$}}

// RUN: %clang -### --target=bpfel %s -o out.o -nolibc 2>&1 \
// RUN:     -ccc-install-dir %S/Inputs/basic_bpf_sysroot_tree/bin \
// RUN:     -resource-dir %S/Inputs/basic_bpf_sysroot_tree/resource \
// RUN:   | FileCheck --check-prefix=LINK_NOLIBC %s
// LINK_NOLIBC: "-cc1" {{.*}} "-o" "[[temp:[^"]*]]"
// LINK_NOLIBC: bpftool{{(\.exe)?}}" "gen" "object" "out.o" "[[temp]]" "{{.*}}/resource/lib/bpfel-unknown-none/libclang_rt.builtins.a"{{$}}

// RUN: %clang -### --target=bpfel %s -o out.o -nostdlib 2>&1 \
// RUN:     -ccc-install-dir %S/Inputs/basic_bpf_sysroot_tree/bin \
// RUN:     -resource-dir %S/Inputs/basic_bpf_sysroot_tree/resource \
// RUN:   | FileCheck --check-prefix=LINK %s
// RUN: %clang -### --target=bpfel %s -o out.o -nodefaultlibs 2>&1 \
// RUN:     -ccc-install-dir %S/Inputs/basic_bpf_sysroot_tree/bin \
// RUN:     -resource-dir %S/Inputs/basic_bpf_sysroot_tree/resource \
// RUN:   | FileCheck --check-prefix=LINK %s

// The builtins come from the sysroot's lib/<triple> when the resource
// directory has none for the target, as a runtimes build with the sysroot as
// its prefix installs them beside libc.a; -print-libgcc-file-name names the
// same file.

// RUN: %clang -### --target=bpfel %s -o out.o 2>&1 \
// RUN:     --sysroot=%S/Inputs/bpf_runtimes_sysroot_tree \
// RUN:     -resource-dir %S/Inputs/bpf_runtimes_sysroot_tree/resource \
// RUN:   | FileCheck --check-prefix=LINK_RUNTIMES %s
// LINK_RUNTIMES: "-cc1" {{.*}} "-o" "[[temp:[^"]*]]"
// LINK_RUNTIMES: bpftool{{(\.exe)?}}" "gen" "object" "out.o" "[[temp]]" "[[ROOT:[^"]+]]/Inputs/bpf_runtimes_sysroot_tree/lib/bpfel-unknown-none/libclang_rt.builtins.a" "[[ROOT]]/Inputs/bpf_runtimes_sysroot_tree/lib/bpfel-unknown-none/libc.a" "[[ROOT]]/Inputs/bpf_runtimes_sysroot_tree/lib/bpfel-unknown-none/libclang_rt.builtins.a"{{$}}

// RUN: %clang -print-libgcc-file-name --target=bpfel 2>&1 \
// RUN:     --sysroot=%S/Inputs/bpf_runtimes_sysroot_tree \
// RUN:     -resource-dir %S/Inputs/bpf_runtimes_sysroot_tree/resource \
// RUN:   | FileCheck --check-prefix=LIBGCC_RUNTIMES %s
// LIBGCC_RUNTIMES: {{.*}}/Inputs/bpf_runtimes_sysroot_tree/lib/bpfel-unknown-none/libclang_rt.builtins.a

// RUN: %clang -print-libgcc-file-name --target=bpfel 2>&1 \
// RUN:     -ccc-install-dir %S/Inputs/basic_bpf_sysroot_tree/bin \
// RUN:     -resource-dir %S/Inputs/basic_bpf_sysroot_tree/resource \
// RUN:   | FileCheck --check-prefix=LIBGCC_RESOURCE %s
// LIBGCC_RESOURCE: {{.*}}/resource/lib/bpfel-unknown-none/libclang_rt.builtins.a

// -l<name> is lib<name>.a from the -L directories, then the sysroot's
// lib/<triple>, in its place among the inputs; one found nowhere is passed by
// its file name for bpftool to report.

// RUN: %clang -### --target=bpfel -nodefaultlibs %s -o out.o 2>&1 \
// RUN:     -lfoo -L%S/Inputs/bpf_lib_tree -lbar -lnone \
// RUN:     -ccc-install-dir %S/Inputs/basic_bpf_sysroot_tree/bin \
// RUN:     -resource-dir %S/Inputs/basic_bpf_sysroot_tree/resource \
// RUN:   | FileCheck --check-prefix=LINK_LIBS %s
// LINK_LIBS: "-cc1" {{.*}} "-o" "[[temp:[^"]*]]"
// LINK_LIBS: bpftool{{(\.exe)?}}" "gen" "object" "out.o" "[[temp]]" "{{.*}}/share/bpf-sysroot/lib/bpfel-unknown-none/libfoo.a" "{{.*}}/Inputs/bpf_lib_tree/libbar.a" "libnone.a"{{$}}

// -Wl, and -Xlinker are bpftool's own options, which precede the command.

// RUN: %clang -### --target=bpf %s -o out.o -Wl,-d -Xlinker --debug 2>&1 \
// RUN:   | FileCheck --check-prefix=LINK_OPTIONS %s
// LINK_OPTIONS: bpftool{{(\.exe)?}}" "-d" "--debug" "gen" "object" "out.o"

// The linker is bpftool: -fuse-ld= may say so, or ld for the default, and
// --ld-path= may name another executable, but there is no other linker.

// RUN: %clang -### --target=bpf %s -o out.o -fuse-ld=bpftool 2>&1 | FileCheck --check-prefix=LINK %s
// RUN: %clang -### --target=bpf %s -o out.o -fuse-ld=ld 2>&1 | FileCheck --check-prefix=LINK %s
// RUN: not %clang -### --target=bpf %s -o out.o -fuse-ld=lld 2>&1 | FileCheck --check-prefix=FUSE_LD %s
// FUSE_LD: error: invalid linker name in argument '-fuse-ld=lld'
// RUN: not %clang -### --target=bpf %s -o out.o --ld-path=nonexistent-bpf-linker 2>&1 \
// RUN:   | FileCheck --check-prefix=LD_PATH %s
// LD_PATH: error: invalid linker name in argument '--ld-path=nonexistent-bpf-linker'

int f(void) { return 0; }
