// bpftool is looked for as the linker of any toolchain is: in the -B
// directories first, then next to clang, then on PATH, and -print-prog-name
// prints the one a link would run.
//
// UNSUPPORTED: system-windows
// RUN: %clang -### --target=bpf -g %s -o out.o -B%S/Inputs/bpf_tool_tree 2>&1 \
// RUN:   | FileCheck --check-prefix=LINK_B %s
// LINK_B: {{.*}}/Inputs/bpf_tool_tree/bpftool" "gen" "object" "out.o"
// RUN: %clang --target=bpf -B%S/Inputs/bpf_tool_tree -print-prog-name=bpftool \
// RUN:   | FileCheck --check-prefix=PROG_B %s
// PROG_B: {{.*}}/Inputs/bpf_tool_tree/bpftool

int f(void) { return 0; }
