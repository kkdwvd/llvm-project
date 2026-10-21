#!/usr/bin/env bash
#===----------------------------------------------------------------------===//
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
#===----------------------------------------------------------------------===//
#
# Run named tests (stem:Suite.Name ...) in one guest without
# --verifier-stats, so that libbpf prints the verifier's full log of a
# refused program, into $BUILD/<stem>.<Suite.Name>.full.log. README.md says
# what each variable names.
#
# Usage: run-one.sh stem:Suite.Name...
set -uo pipefail

here=$(cd "$(dirname "$0")" && pwd)
build=${BUILD:-$here/build}

: "${KERNEL:?set KERNEL to the kernel build directory or bzImage vng boots}"
: "${STD_BPF_BUILD:?set STD_BPF_BUILD to a build of the std::bpf tree}"
: "${STD_BPF_SOURCE:?set STD_BPF_SOURCE to the std::bpf tree, for tools/vm-run}"
VMRUN=$STD_BPF_SOURCE/tools/vm-run
RUNNER=$STD_BPF_BUILD/lib/testing/bpf_test_runner
VNG_ARGS=${VNG_ARGS:---disable-microvm}

guest="$build/guest-one.sh"
{
  echo '#!/usr/bin/env bash'
  for spec in "$@"; do
    stem=${spec%%:*}; name=${spec#*:}
    echo "'$RUNNER' '$build/$stem.bpf.o' --filter='$name' > '$build/$stem.$name.full.log' 2>&1; echo \"exit=\$?\" >> '$build/$stem.$name.full.log'"
  done
} > "$guest"
chmod +x "$guest"

vng_args=()
for arg in $VNG_ARGS; do vng_args+=(--vng-arg "$arg"); done
"$VMRUN" --kernel "$KERNEL" --cwd "$build" --cpus "${CPUS:-2}" --memory "${MEMORY:-4G}" "${vng_args[@]}" -- "$guest" > "$build/vm-one.log" 2>&1
echo "vm-run exit $?"
for spec in "$@"; do
  stem=${spec%%:*}; name=${spec#*:}
  echo "== $build/$stem.$name.full.log"
  tail -n 3 "$build/$stem.$name.full.log"
done
