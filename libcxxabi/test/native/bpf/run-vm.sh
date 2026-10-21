#!/usr/bin/env bash
#===----------------------------------------------------------------------===//
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
#===----------------------------------------------------------------------===//
#
# Boot one vng guest on the kernel and run every BPF_TEST of every linked
# test object (or of the stems named) one at a time, each with its own
# --filter so that a program the verifier refuses does not hide the others;
# then summarize in $BUILD/run-results.txt, with the verifier's instruction
# count of each program. README.md says what each variable names.
#
# A test source may carry a line `// RUN-TOGETHER: <glob on Suite.Name>`:
# the tests it matches share state from one program to the next and run in
# one load of the object, the glob being the runner's filter. A suite named
# *Terminates holds programs that must end through std::terminate(), as
# upstream's test expects to be terminated: the runner reports them as ended
# early, which the summary counts as OK.
#
# Usage: run-vm.sh [test-stem ...]
set -uo pipefail

here=$(cd "$(dirname "$0")" && pwd)
build=${BUILD:-$here/build}

: "${KERNEL:?set KERNEL to the kernel build directory or bzImage vng boots}"
: "${STD_BPF_BUILD:?set STD_BPF_BUILD to a build of the std::bpf tree}"
: "${STD_BPF_SOURCE:?set STD_BPF_SOURCE to the std::bpf tree, for tools/vm-run}"
VMRUN=$STD_BPF_SOURCE/tools/vm-run
RUNNER=$STD_BPF_BUILD/lib/testing/bpf_test_runner
VNG_ARGS=${VNG_ARGS:---disable-microvm}

if (($# > 0)); then stems=("$@"); else
  stems=(); for o in "$build"/*.bpf.o; do [[ -f "$o" ]] && stems+=("$(basename "$o" .bpf.o)"); done
fi

for stem in "${stems[@]}"; do rm -f "$build/$stem".*.run.log; done
guest="$build/guest-script.sh"
{
  echo '#!/usr/bin/env bash'
  for stem in "${stems[@]}"; do
    obj="$build/$stem.bpf.o"
    [[ -f "$obj" ]] || continue
    mapfile -t groups < <(grep -oE '^// RUN-TOGETHER: .*' "$here/$stem.bpf.cpp" | sed -E 's|^// RUN-TOGETHER: ||')
    for group in "${groups[@]}"; do
      echo "'$RUNNER' '$obj' --filter='$group' --verifier-stats > '$build/$stem.$group.run.log' 2>&1; echo \"exit=\$?\" >> '$build/$stem.$group.run.log'"
    done
    while read -r name; do
      grouped=0
      for group in "${groups[@]}"; do [[ "$name" == $group ]] && grouped=1; done
      ((grouped)) && continue
      echo "'$RUNNER' '$obj' --filter='$name' --verifier-stats > '$build/$stem.$name.run.log' 2>&1; echo \"exit=\$?\" >> '$build/$stem.$name.run.log'"
    done < <(grep -oE '^BPF_TEST\([A-Za-z0-9]+, [A-Za-z0-9]+\)' "$here/$stem.bpf.cpp" | sed -E 's/BPF_TEST\(([A-Za-z0-9]+), ([A-Za-z0-9]+)\)/\1.\2/')
  done
} > "$guest"
chmod +x "$guest"

vng_args=()
for arg in $VNG_ARGS; do vng_args+=(--vng-arg "$arg"); done
"$VMRUN" --kernel "$KERNEL" --cwd "$build" --cpus "${CPUS:-2}" --memory "${MEMORY:-4G}" "${vng_args[@]}" -- "$guest" > "$build/vm.log" 2>&1
echo "vm-run exit $?"

printf '%-26s %-40s %-6s %-9s %s\n' object test result insns detail | tee "$build/run-results.txt"
for stem in "${stems[@]}"; do
  for log in "$build/$stem".*.run.log; do
    [[ -f "$log" ]] || continue
    name=$(basename "$log" .run.log); name=${name#$stem.}
    # The verifier's statistics line of every program loaded, the test's by
    # far the largest: the largest count is the test's.
    insns=$(grep -oE "processed [0-9]+ insns" "$log" | grep -oE '[0-9]+' | sort -n | tail -1)
    if grep -qE '\[ +OK +\]' "$log" && ! grep -qE 'failed:|FAILED|ended early|cannot load' "$log"; then
      detail=$(grep -oE '[0-9]+ checks' "$log" | tr '\n' ' '); result=OK
    elif [[ "$name" == *Terminates.* ]] && grep -qE 'ended early: std::terminate\(\) ended it' "$log"; then
      detail='ended through std::terminate(), as expected'; result=OK
    elif grep -qE 'failed:|FAILED|ended early' "$log"; then
      detail=$(grep -m1 -E 'failed:|FAILED|ended early' "$log" | cut -c1-100); result=FAIL
    elif grep -qE 'cannot load|load failed' "$log"; then
      detail=$(grep -E "^[0-9]+: |invalid|unreachable|R[0-9] |not found|too complex|limit|Unreadable|same insn|recursive|callx" "$log" | grep -v "processed\|verification" | tail -1 | cut -c1-100); result=LOAD
    else
      detail=$(tail -1 "$log" | cut -c1-100); result=OTHER
    fi
    printf '%-26s %-40s %-6s %-9s %s\n' "$stem" "$name" "$result" "${insns:--}" "$detail" | tee -a "$build/run-results.txt"
  done
done
