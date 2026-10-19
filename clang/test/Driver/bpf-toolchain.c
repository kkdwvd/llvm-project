// The BPF toolchain compiles as the generic ELF toolchain did: the driver adds
// no include directory and the frontend adds the host's, as it does for any
// target without an OS. The CPU is the backend's default unless -mcpu says
// otherwise.

// RUN: %clang -### --target=bpf -c %s 2>&1 | FileCheck --check-prefix=CC1 %s
// RUN: %clang -### --target=bpfel -c %s 2>&1 | FileCheck --check-prefix=CC1 %s
// RUN: %clang -### --target=bpfeb -c %s 2>&1 | FileCheck --check-prefix=CC1 %s
// RUN: %clang -### --target=bpfel-unknown-none -c %s 2>&1 | FileCheck --check-prefix=CC1 %s
// RUN: %clang -### --target=bpf --sysroot=%S/Inputs/bpf_rootfs_tree -c %s 2>&1 \
// RUN:   | FileCheck --check-prefixes=CC1,ROOTFS %s
// CC1: "-cc1"
// CC1-NOT: "-nostdsysteminc"
// ROOTFS-SAME: "-isysroot" "{{.*}}/Inputs/bpf_rootfs_tree"
// CC1-NOT: "-internal-isystem"
// CC1-NOT: "-internal-externc-isystem"
// CC1-NOT: "-target-cpu"
// CC1-NOT: "-fvisibility

// RUN: %clang -### --target=bpf -mcpu=v3 -c %s 2>&1 | FileCheck --check-prefix=MCPU %s
// MCPU: "-target-cpu" "v3"

// Linking is bpftool gen object with the inputs in their order, and nothing
// else.

// RUN: %clang -### --target=bpf %s -o out.o 2>&1 | FileCheck --check-prefix=LINK %s
// LINK: "-cc1" {{.*}} "-o" "[[temp:[^"]*]]"
// LINK: bpftool{{(\.exe)?}}" "gen" "object" "out.o" "[[temp]]"{{$}}

// -l<name> is lib<name>.a from the -L directories, in its place among the
// inputs; one found nowhere is passed by its file name for bpftool to report.

// RUN: %clang -### --target=bpfel %s -o out.o -L%S/Inputs/bpf_lib_tree -lbar -lnone 2>&1 \
// RUN:   | FileCheck --check-prefix=LINK_LIBS %s
// LINK_LIBS: "-cc1" {{.*}} "-o" "[[temp:[^"]*]]"
// LINK_LIBS: bpftool{{(\.exe)?}}" "gen" "object" "out.o" "[[temp]]" "{{.*}}/Inputs/bpf_lib_tree/libbar.a" "libnone.a"{{$}}

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
