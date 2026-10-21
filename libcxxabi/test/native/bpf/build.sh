#!/usr/bin/env bash
#===----------------------------------------------------------------------===//
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
#===----------------------------------------------------------------------===//
#
# Compile every *.bpf.cpp of this directory (or the stems named) against a BPF
# sysroot with clang's BPF toolchain, link each with bpftool against the
# stand-in platform layer (support/platform.bpf.cpp) and the std::bpf test
# framework's objects, and record the compile and link results in
# $BUILD/build-results.txt. README.md says what each variable names.
#
# Usage: build.sh [test-stem ...]
set -uo pipefail

here=$(cd "$(dirname "$0")" && pwd)
build=${BUILD:-$here/build}
mkdir -p "$build"

: "${SYSROOT:?set SYSROOT to a BPF sysroot (see README.md)}"
: "${BPFTOOL:?set BPFTOOL to the static linker of libbpf from the cpp-bpf kernel tree}"
: "${STD_BPF_BUILD:?set STD_BPF_BUILD to a build of the std::bpf tree}"
CLANGXX=${CLANGXX:-clang++}
TRIPLE=${TRIPLE:-bpfel-unknown-none}

# The std::bpf objects the harness keeps: the arena prepare program with the
# .arena.data marker, the opaque zero of the tests' loops, and the results
# global the runner reads. Not the allocator, exceptions, unwinding or RTTI:
# those are the sysroot's and the platform layer's.
runtime=("$STD_BPF_BUILD/lib/testing/std_bpf_testing_bpf.dir/BpfTest.bpf.o"
         "$STD_BPF_BUILD/lib/std_bpf.dir/Loop.bpf.o"
         "$STD_BPF_BUILD/lib/std_bpf.dir/Arena.bpf.o")
for o in "${runtime[@]}"; do
  [[ -f "$o" ]] || { echo "missing $o: build the std::bpf tree first" >&2; exit 1; }
done

# The flags the std::bpf tree compiles its objects with, beyond the toolchain's
# defaults for C++ on BPF: the sysroot's runtimes are built for -mcpu=v4, the
# include root holds <std/bpf/...> and vmlinux.h, libbpf's BPF-side headers
# come from the same build.
flags=(--target="$TRIPLE" --sysroot="$SYSROOT" -mcpu=v4 -std=c++26 -O2 -g
  "-I$STD_BPF_BUILD/include" "-I$STD_BPF_BUILD/bpftool/libbpf/include"
  -Wall -Wextra -Wno-extern-c-compat -Wno-unused-parameter)
# The link is the toolchain's, which runs bpftool on the inputs and names the
# sysroot's libc++.a, libc++abi.a, libc.a and the builtins after them.
link=(--target="$TRIPLE" --sysroot="$SYSROOT" "--ld-path=$BPFTOOL")

if (($# > 0)); then
  sources=()
  for s in "$@"; do sources+=("$here/$s.bpf.cpp"); done
else
  sources=("$here"/*.bpf.cpp)
fi

"$CLANGXX" "${flags[@]}" -c "$here/support/platform.bpf.cpp" -o "$build/platform.o" \
  2> "$build/platform.compile.log" || { echo "support/platform: compile FAILED"; cat "$build/platform.compile.log"; exit 1; }

printf '%-28s %-8s %s\n' test compile link | tee "$build/build-results.txt"
for src in "${sources[@]}"; do
  stem=$(basename "$src" .bpf.cpp)
  obj="$build/$stem.o"
  if ! "$CLANGXX" "${flags[@]}" -c "$src" -o "$obj" 2> "$build/$stem.compile.log"; then
    printf '%-28s %-8s %s\n' "$stem" FAIL "$(grep -m1 -E 'error:' "$build/$stem.compile.log" | cut -c1-110)" | tee -a "$build/build-results.txt"
    rm -f "$build/$stem.bpf.o"
    continue
  fi
  if "$CLANGXX" "${link[@]}" "$obj" "$build/platform.o" "${runtime[@]}" -o "$build/$stem.bpf.o" 2> "$build/$stem.link.log"; then
    # bpftool leaves an unresolved extern in place for the loader to find in
    # the kernel's BTF; an extern that is no kfunc or helper is a missing
    # definition, reported here as a link failure.
    externs=$("$BPFTOOL" btf dump file "$build/$stem.bpf.o" 2>/dev/null | grep -oE "(FUNC|VAR) '[^']+' type_id=[0-9]+,? ?linkage=extern" | sed -E "s/(FUNC|VAR) '([^']+)'.*/\2/" | grep -vE '^(bpf_|__bpf_trap)' | tr '\n' ' ')
    if [[ -n "$externs" ]]; then link_result="UNRES $externs"; else link_result=ok; fi
  else
    link_result="FAIL  $(grep -m1 -iE 'error|fail|unresolved|undefined' "$build/$stem.link.log" | cut -c1-110)"
    rm -f "$build/$stem.bpf.o"
  fi
  printf '%-28s %-8s %s\n' "$stem" ok "$link_result" | tee -a "$build/build-results.txt"
done
