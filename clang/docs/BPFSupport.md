# BPF Support

Clang compiles C and C++ for BPF, the kernel's virtual machine. The driver's
BPF toolchain compiles against a sysroot when there is one, laid out as
wasi-sdk lays out its own, and links with libbpf's static linker, `bpftool`.
A C compilation without a sysroot is what it was before the toolchain
existed: nothing a BPF program that is built today with `--target=bpf` and the
host's or the kernel's headers sees has changed.

## Target triples

The targets are `bpfel-unknown-none` and `bpfeb-unknown-none`. `--target=bpf`
is the host's byte order, and `bpfel`, `bpfeb`, `bpf-none` and the rest are
the same two targets spelled shorter. The directories that hold a target's
files, in the sysroot and in the resource directory, are named by the full
triple whatever the spelling, so that `include/bpfel-unknown-none` serves
`--target=bpf` and `--target=bpfel` alike.

## The sysroot

The sysroot is the first of these that is given:

1. `--sysroot=<path>` on the command line;
2. `DEFAULT_BPF_SYSROOT`, a CMake setting of the compiler's build, like
   `DEFAULT_SYSROOT` but for the BPF targets alone, so that a compiler that
   builds for its host too can carry one;
3. `share/bpf-sysroot` next to the compiler's installation (`bin/..`), as
   wasi-sdk keeps `share/wasi-sysroot`.

It is a BPF sysroot only if it has `include/<triple>` for the target. A
`--sysroot` without that is not one: C BPF programs have long been built with
`--sysroot` naming a Linux root to take their `<linux/*.h>` from under it, and
they still are, since the toolchain then does what it did before.

The layout, with `<triple>` one of the two above:

```
<sysroot>/include/<triple>/            the target's C headers
<sysroot>/include/<triple>/c++/v1/     libc++'s headers, or its __config_site
                                       alone
<sysroot>/include/c++/v1/              libc++'s headers, if shared between
                                       the targets
<sysroot>/include/                     C headers shared between the targets
<sysroot>/lib/<triple>/                libc++.a, libc++abi.a and the archives
                                       -l names
<resource>/lib/<triple>/               compiler-rt's libclang_rt.builtins.a,
                                       in the compiler's resource directory
```

With a sysroot the compiler takes no header from the host: the frontend's own
system directories (`/usr/local/include`, `/usr/include`) are off, and the
search path is, in order, libc++'s per-target directory, its shared one, the
compiler's builtin headers in the resource directory, the sysroot's per-target
C headers and its shared ones. `-nostdinc++`, `-nostdlibinc`, `-nobuiltininc`
and `-nostdinc` leave out what they do on every target.

Without a sysroot the driver adds no directory, and the frontend adds the
host's as it does for any target without an OS, under `--sysroot` if there is
one; there are no C++ headers.

## Defaults for C and C++

C keeps every default it had. The CPU is the backend's unless `-mcpu` says
otherwise, as it was: the sysroot's libraries are built for one level, and a
program that needs it passes the same `-mcpu`, as it would pass `-march` on
another target. Nothing is freestanding by default, and a function that is not
`static` is a global subprogram, which the verifier checks once with typed
arguments.

C++ on BPF, with or without a sysroot:

- C++ exceptions and RTTI are on, as on hosted targets. The backend lowers
  unwinding to the instructions the kernel verifies, BTF describes the
  type_info objects and vtables, and the runtime matches types for a catch
  and a `dynamic_cast`. `-fno-exceptions` and `-fno-rtti` turn them off.
- Unwind tables are on whenever exceptions are, freestanding or not, since the
  backend unwinds through a function only if it has them; `-fno-unwind-tables`
  wins.
- `-fvisibility-inlines-hidden` and
  `-fvisibility-global-new-delete=force-hidden`. libbpf makes a hidden
  function a static subprogram in BTF, which the verifier checks at each call
  with what the caller passes, and a default one a global subprogram. The
  functions an author writes keep C's rule. The inline member functions of
  classes and class templates would otherwise be weak global subprograms
  with untyped pointer parameters, which verify nowhere, and the global
  allocation functions take and return the void pointers a global subprogram
  may not. Nothing else is hidden: a BPF object exports nothing, so the
  reason WebAssembly hides everything by default does not apply, and the
  visibility of a function is what chooses how the verifier checks it. An
  inline free function and the instance of a free function template keep
  default visibility, as the flag's name says, and are weak global
  subprograms, verified once with typed arguments like any global function;
  one whose parameters a global function may not take is made `static` or
  hidden, or the unit is built with `-fvisibility=hidden`.
- `-g` is standalone (`-fstandalone-debug`). BTF is built from the debug info
  of each object alone, and libbpf loads no map or global whose type the
  debug info leaves declared, as limited debug info does with a class whose
  constructor another unit defines.
- libc++ is the C++ standard library, and the only one.

On both, constructors and destructors are never emitted as aliases, since an
alias has no BTF.

## Linking

A link runs libbpf's static linker:

```
clang --target=bpf a.o b.o -o prog.bpf.o
  bpftool gen object prog.bpf.o a.o b.o
```

`bpftool` is looked for next to clang, in `-B` directories and on `PATH`.
`-fuse-ld=bpftool` or `-fuse-ld=ld` name the default; `--ld-path=` names
another executable; there is no other linker. `-Wl,` and `-Xlinker` pass
bpftool's own options, which precede the command.

bpftool takes files, so `-l<name>` is `lib<name>.a` from the `-L` directories,
then the sysroot's `lib/<triple>`, then the resource directory's, passed by
its path in its place among the inputs; one found nowhere is passed as
`lib<name>.a` for bpftool to report.

With a sysroot the default libraries follow the inputs: `libc++.a` and
`libc++abi.a` from the sysroot when the driver is `clang++`, then
compiler-rt's builtins from the resource directory. `-nostdlib++` leaves the
first two out, `-nostdlib` and `-nodefaultlibs` all of them. Without a sysroot
nothing is linked but what was named.

## Building the sysroot

The runtimes build produces the layout. For each target, configure
compiler-rt's builtins, libc++ and libc++abi with
`-DLLVM_DEFAULT_TARGET_TRIPLE=bpfel-unknown-none` (or `bpfeb`),
`-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=ON` and
`-DCMAKE_INSTALL_PREFIX=<sysroot>`: libc++'s headers then install to
`include/c++/v1` with their `__config_site` in `include/<triple>/c++/v1`, or
all of them to the latter with `LIBCXX_INSTALL_INCLUDE_DIR`, and the archives
to `lib/<triple>`. The builtins install to the compiler's resource directory,
`lib/<triple>/libclang_rt.builtins.a` under it, which is where the driver
looks (`-print-libgcc-file-name` prints the path). The target's C headers,
the kernel's UAPI headers among them if the sysroot is to replace the host's
`<linux/*.h>`, go to `include/<triple>`.

One thing the runtimes must get right for libbpf, which links by BTF and
visibility rather than by symbol name alone: libc++'s `__config_site` should
define `_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS` (`LIBCXX_EXTRA_SITE_DEFINES`),
since a declaration with default visibility in a program does not link against
the hidden definition in the library.
