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
                                       alone, and the C++ ABI's cxxabi.h
<sysroot>/include/c++/v1/              libc++'s headers, if shared between
                                       the targets
<sysroot>/include/                     C headers shared between the targets
<sysroot>/lib/<triple>/                libc.a, libc++.a, libc++abi.a,
                                       libunwind.a and the archives -l
                                       names, and compiler-rt's
                                       libclang_rt.builtins.a when the
                                       runtimes were built into the sysroot
<resource>/lib/<triple>/               compiler-rt's libclang_rt.builtins.a,
                                       in the compiler's resource directory,
                                       which comes first
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

### cxxabi.h in every C++ unit

libbpf links a call of a function in another object through BTF, which clang
derives from the declaration the unit has of the function. The calls clang
makes on its own, for a throw, a catch, a `dynamic_cast` or a vtable's pure
virtual slot, have BTF only if the unit declares the runtime function, and a
unit that throws need not include any header. With a sysroot that has it, the
driver includes `include/<triple>/c++/v1/cxxabi.h`, the C++ ABI's header that
libc++abi installs with libc++'s headers, in every C++ unit, as the CUDA, HIP
and OpenMP toolchains include their runtimes' declarations. Its declarations
must have the types, and the `noexcept`, that the runtime is built with:
libbpf compares the BTF of the call with the BTF of the definition.

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

`bpftool` is looked for as the linker of any toolchain is: in the `-B`
directories first, then next to clang, then on `PATH`;
`-print-prog-name=bpftool` prints the one a link would run. `-fuse-ld=bpftool`
or `-fuse-ld=ld` name the default; `--ld-path=` names another executable;
there is no other linker. `-Wl,` and `-Xlinker` pass bpftool's own options,
which precede the command.

An object links through its BTF, which the compiler builds from debug info.
A source compiled in the same invocation without `-g` cannot be linked, and
the driver says so before bpftool would refuse the object (`failed to find BTF
info for object`); an object given as an input is bpftool's to check.

bpftool takes files, so `-l<name>` is `lib<name>.a` from the `-L` directories,
then the sysroot's `lib/<triple>`, then the resource directory's, passed by
its path in its place among the inputs; one found nowhere is passed as
`lib<name>.a` for bpftool to report.

With a sysroot the default libraries follow the inputs, in the order they
depend on each other, since libbpf links a member of an archive for the
references before the archive only, as ld does: `libc++.a`, `libc++abi.a` and
`libunwind.a` from the sysroot's `lib/<triple>` when the driver is `clang++`,
then compiler-rt's builtins, `libclang_rt.builtins.a` from the resource
directory's `lib/<triple>` or, when that has none, the sysroot's, `libc.a`
from the sysroot's, and the builtins once more, since the two call each
other: libc the 128-bit division, the builtins `malloc` and `memcpy` for
emulated TLS. That is GCC's link sequence, libgcc, libc, libgcc, and the GNU
driver's, which lists the runtime libraries around `-lc`. `-nostdlib++`
leaves out the first three, `-nolibc` libc and the builtins' second copy,
`-nostdlib` and `-nodefaultlibs` all of them. Without a sysroot nothing is
linked but what was named. `-print-libgcc-file-name` prints the builtins the
link would take.

## Building the sysroot

The runtimes build produces the layout. For each target, configure
compiler-rt's builtins, a libc, libunwind, libc++ and libc++abi with
`-DLLVM_DEFAULT_TARGET_TRIPLE=bpfel-unknown-none` (or `bpfeb`),
`-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=ON` and
`-DCMAKE_INSTALL_PREFIX=<sysroot>`: libc++'s headers then install to
`include/c++/v1` with their `__config_site` in `include/<triple>/c++/v1`, or
all of them to the latter with `LIBCXX_INSTALL_INCLUDE_DIR`, and the archives,
the builtins' `libclang_rt.builtins.a` among them, to `lib/<triple>`, where
the driver finds the builtins when the compiler's own resource directory has
none for the target, so that a sysroot built apart from the compiler carries
everything a link needs. When the prefix is the compiler's own installation,
`-DCOMPILER_RT_INSTALL_PATH=<prefix>/lib/clang/<major>` puts the builtins in
its resource directory instead, `lib/<triple>/libclang_rt.builtins.a` under
it, as the runtimes of every target are installed there; the driver takes
those first. The target's C headers, the kernel's UAPI headers among them if
the sysroot is to replace the host's `<linux/*.h>`, go to `include/<triple>`.

Two things the runtimes must get right for libbpf, which links by BTF and
visibility rather than by symbol name alone: libc++'s `__config_site` should
define `_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS` (`LIBCXX_EXTRA_SITE_DEFINES`),
since a declaration with default visibility in a program does not link against
the hidden definition in the library, and `cxxabi.h` must be in
`include/<triple>/c++/v1` with the declarations the runtime is built with, as
above.

The runtimes caches `libc/cmake/caches/bpfel-unknown-none.cmake` and
`bpfeb-unknown-none.cmake` configure all five runtimes this way, libc++abi's
BPF port included: on BPF `cxxabi.h` also declares `__dynamic_cast`, and
`__cxa_end_catch` is `noexcept`.

### The personality routine a landing pad calls

The kernel unwinds, and runs no personality routine between the frames, as
the VM runs none in WebAssembly: every landing pad is entered, with the
exception object in r0. A pad with catch clauses for types has the
personality run where WebAssembly has it run, in the pad: it stores its
index and the address of its function's exception table into the landing
pad context that libunwind keeps (`_Unwind_GetBPFLPadContext()`,
`libunwind/src/Unwind-bpf.c`), calls the personality with the exception
object, and reads the selector back; the dispatch on the selector is
clang's, as on every target. The personality is `__gxx_bpf_personality_v0`,
which clang selects for C++ on BPF and declares as it declares
`__gxx_wasm_personality_v0`, taking the exception object and returning
`int`, and which libc++abi defines on its common code, single phase, the
call sites keyed by index: it reads the table through libunwind's
`_Unwind_GetLanguageSpecificData()` and `_Unwind_GetIP()`, and sets the
selector through `_Unwind_SetGR()`. The backend declares the personality
and the context accessor it calls with the debug info of their C
prototypes, as it declares its other libcalls, so that libbpf links the
calls through BTF.

The table is the standard LSDA, emitted as for WebAssembly: a call-site
table keyed by landing pad index rather than by address, the action table
and the type table with absolute pointers to the type_info objects, in
`.data.rel.ro` as the type_info objects and vtables are, where libbpf loads
it with the rest of the data and relocates it; no `.gcc_except_table`
section, and no CFI naming the personality or the table. A pad with
cleanups or a catch-all alone makes no call and has no entry, and a
function whose pads need no entry has no table. An exception filter with
types, as of a dynamic exception specification, is refused.

### The exception object a runtime unwinds with

The backend lowers unwinding to `unwind_goto` after a call that may throw
and `unwind_exit` out of a frame, with the exception object in r0 at every
landing pad and at every `unwind_exit`. The kernel checks r0 at
`unwind_exit` as it checks a return value. A static function hands r0 to
its caller as it is, and a callback that a helper calls may unwind with r0
of any type: the pad after the helper's call gets r0 with the verifier
state the callback left it in, and only a pointer into the callback's own
stack is refused there. A global function is verified on its own and must
leave a scalar or an arena pointer at `unwind_exit` as at `exit`, so an
exception object that leaves a global function has to be one of those,
which under `BPF_F_ARENA_SCALAR` means it lives in the arena. That is the
ABI rule for every language runtime that unwinds on BPF, not only C++, and
global functions are where it comes from. libc++abi's port complies: its
objects come from `malloc`, which the platform serves from the arena, and
from its fallback heap, which is arena data. A pointer to a static buffer
or to a map value does not comply as soon as the throw leaves a global
function.

Out of a program's entry, `unwind_exit` is exit, and the program returns the
exception object, which the verifier's return checks judge. An entry written
`noexcept` ends such an exception through `std::terminate()` instead, after
the frames inside the program unwound, as std::bpf's `BPF_CPP_PROGRAM`
declares every entry.

## Status and open work

C++ on BPF is the freestanding C++26 library, with the hosted parts a
program reaches, on the kernel's virtual machine, with no convention of its
own where another platform's does: the toolchain is clang with a sysroot as
wasi-sdk is, the runtimes are LLVM's built the runtimes way, exceptions and
RTTI are the Itanium ABI's through libc++abi, and the platform under them,
`malloc`, `free`, `aligned_alloc` and the arena they serve from, is a
program's own (std::bpf, `src/bpf/cpp` of the kkdwvd monorepo, is one).
This section records where that stands: what works, what is work in
progress and what retires it, and what is open, each with the commit, file
or test that holds the details, so that a reader of the trees alone finds
what review left open here and in the messages of the commits it belongs
to. It is written against the kernel of the kkdwvd/linux fork on branch
`fermata`, which carries `cpp-bpf`, and against std::bpf on its
`bpf-sysroot` branch.

### What works

- The toolchain: the sysroot layout, the search paths, the C++ defaults, the
  link step with bpftool and the refusal of a link without debug info,
  above ("[clang][BPF] Add a toolchain that links with bpftool" and the
  commits after it).
- BTF for C++: a function named by its symbol, with its linkage; thunks by
  the type of their vtable slot; references as pointers, and the reference
  parameters and `this` of a global function tagged `arg:nonnull`, so that
  the kernel takes them for what C++ says they are ("[BPF] Tag the reference
  parameters of a global function nonnull in BTF"); pointers to members and
  `std::nullptr_t` as integers; the aliases of functions, the libcalls the
  backend emits and the runtime functions clang calls on its own; the
  type_info objects and vtables as the bytes they are; the source return
  type of a function that returns through an sret pointer, at both ends of
  a link; and the parameters of a function built without optimization.
- Thread-local storage, emulated, as on the targets that default to
  emulated TLS (Android before 29, OpenBSD, Cygwin): `thread_local` and
  `_Thread_local` compile, the driver passes `-femulated-tls`, LowerEmuTLS
  turns each variable into its `__emutls_v.` control block and
  `__emutls_t.` template, globals described in BTF as the bytes they are in
  `.data` and `.rodata`, and every access is a call of
  `__emutls_get_address()`, declared for BTF with the prototype compiler-rt
  defines it with. compiler-rt's `emutls.c` has a BPF backend: per CPU,
  through the kernel's CPU helper, the objects from the platform's
  `malloc`, the work a global subprogram taking and returning numbers that
  the verifier checks once per program behind the hidden entry point, so
  that an access costs its caller a few instructions and a call. Per CPU
  has the limit of the per-invocation state below, and the fix is recorded
  beside the code. A variable with a dynamic initializer is a static
  constructor, which BPF has none of; `constinit` says so. What a hosted
  runtime keeps per thread is `thread_local` here as there: libc++abi's
  exception state and llvm-libc's errno.
- The runtimes, from one cache: compiler-rt's 128-bit integer builtins with
  a division loop the verifier bounds (`compiler-rt/lib/builtins/bpf/`);
  llvm-libc in its bare-metal configuration with the kernel's error numbers,
  loops bounded with `may_goto` and a `thread_local` errno
  (`libc/config/baremetal/bpf/`); libunwind as the landing pad context and
  the accessors of `libunwind/src/Unwind-bpf.c` alone; libc++ as a static library with what BPF
  cannot compile left out (`libcxx/src/CMakeLists.txt`: floating point,
  variadic definitions, a static object with a destructor, a `std::string`
  passed by value) and `operator new` allocating once without the new
  handler (`libcxx/src/support/new.ipp`); libc++abi ported
  (`libcxxabi/src/*_bpf.cpp`, `bpf_platform.h`, "[libc++abi][BPF] Port
  libc++abi to BPF").
- Exceptions: unwinding lowered to `unwind_goto` after a call that may throw
  and `unwind_exit` out of a frame, the exception object in r0 at every
  landing pad and at every `unwind_exit`, an arena object whenever it
  leaves a global function (the ABI rule for runtimes, above), the
  personality routine called from a pad with catch clauses for
  types as WebAssembly calls it, reading the standard tables keyed by
  landing pad index (above), a `noexcept`
  violation ended through `__clang_call_terminate` as on every target, which
  begins catching the exception before `std::terminate` so that a terminate
  handler sees it, and has BTF from the artificial debug info clang gives
  the functions it makes up; `std::exception_ptr`, nested
  exceptions of non-polymorphic types, `std::uncaught_exceptions()`, the
  standard exception classes, `std::bad_alloc` from `new`. RTTI: `typeid`,
  `dynamic_cast` with pointers and references, `std::bad_cast` and
  `std::bad_typeid`, through walks of the class graph bounded for the
  verifier (`libcxxabi/src/private_typeinfo_bpf.cpp`).
- Of the freestanding headers, checked against this tree, a sysroot built
  from it and that kernel: `<algorithm>` and `<numeric>` over ranges
  the verifier can count, `<atomic>` at 32 and 64 bits with every memory
  order and fences, `<bit>`, `<charconv>` for integers in base 10 (with
  `to_chars` through the 128-bit builtins), `<coroutine>`, `<cstdlib>`,
  `<cerrno>`, `<cstring>` with constant lengths, `<exception>`, `<expected>`,
  `<functional>`'s freestanding parts, `<memory>` (`unique_ptr`,
  `allocator`, the uninitialized algorithms, `out_ptr`), `<new>`,
  `<optional>`, `<random>`'s engines, `<ranges>` with constant bounds,
  `<span>`, `<array>`, `<mdspan>`, `<string_view>` but for `find` of a
  string, `<tuple>`, `<utility>`, `<compare>`, `<ratio>`, `<type_traits>`,
  `<typeinfo>`, `<variant>` with `std::visit`, `<version>`, `<debugging>`,
  `<stdckdint.h>`; and of the hosted parts `<vector>`, `<string>` in the
  arena, `std::shared_ptr`, `std::function` with a small functor and the
  throwing accessors of the sum types. The tests are std::bpf's
  `toolchain/test/freestanding/` (one object per group of headers, and
  `Refused.bpf.cpp` with `RefusedTest.cpp` and `compile/` for what is
  refused, below), `toolchain/test/` for the exception runtime and the
  other runtimes, and here `libcxxabi/test/native/bpf/` for libc++abi's
  own tests as BPF programs, all passing on that kernel.

### Work in progress

Each `[WIP]` commit exists for one reason and goes with it.

- "[WIP] [clang][BPF] Keep the unwind edge of a call through a pointer"
  (`clang/lib/CodeGen/CGCall.cpp`): the kernel's callx of a number merges
  what it knows of every function whose address the program takes, so a
  call through a pointer needs its `unwind_goto` whatever the pointee's
  exception specification. Retired by a callx design in which the verifier
  knows the callees of a call through a pointer, verified function
  pointers, or BTF that says which callees throw.
- "[WIP] [libc++abi][BPF] Defer the destructors of thrown objects"
  (`libcxxabi/src/cxa_exception.cpp`, the queue in `cxa_exception.h`): a
  thrown object's destructor is called through the address the throw
  stored, and the kernel refuses a function whose address a program takes
  that may make such a call itself, as recursion, which dropping the last
  reference to an exception from a destructor would be. The queue has
  costs of its own, in its message: an `exception_ptr` released outside a
  handler is destroyed at the next handler's end on that CPU, after a delay
  nothing bounds; a program in IRQ context can interleave with the two
  stores that push onto the queue; and the callx in `__cxa_end_catch`
  makes every function whose address is taken that contains a handler
  refused as recursion, not only a thrown object's destructor. Retired by
  the same design; `__cxa_decrement_exception_refcount` then destroys at
  once again.
- "[WIP] [BPF] Select an or that is an add as an add"
  (`llvm/lib/Target/BPF/BPFInstrInfo.td`, `or-disjoint.ll`): noticed in the
  std::atomic test programs, as `r2 |= r1` on a stack pointer the verifier
  refuses ("bitwise operator |= on pointer prohibited") and a kernel that
  makes any other operation turn a pointer into a number misroutes to the
  arena. Not seen in std::bpf's or libc++'s code. To reconsider: whether the
  backend is the place, or the transform that forms the or should not apply
  to BPF pointers, or the verifier should treat a disjoint or on a pointer
  as an add; and whether to send it upstream on its own, since C programs
  hit the same refusal.
- "[WIP] [BPF] Give narrow atomics the alignment of their address"
  (`llvm/lib/Target/BPF/BPFAlignAtomics.cpp`, `atomic-partword-align.ll`,
  `and-add-aligned.ll`): noticed in the same test programs, an 8-bit
  `fetch_or` on a `std::atomic<uint8_t>` on the stack whose masked address
  went to the arena and read 0. The pass raises a stack object or a global
  to the word's alignment, a global with an explicit `aligned(1)` or
  `aligned(2)` attribute included, and a DAG combine folds the mask of an
  atomic at an odd offset into the offset; what stays masked is an address
  whose alignment is not known and cannot be chosen, a pointer a function
  is passed, one a map lookup returns, an extern global or one in a
  section of its own. One limit of the partword expansion stays whatever
  the pass does: the 32-bit word around a narrow atomic on a global must
  lie inside the global's data map, and a 2-byte global alone in `.bss`,
  `struct flags gflags;`, gives a map of `value_size=2` and the refusal
  "invalid access to map value, value_size=2 off=0 size=4"; libbpf
  rounding a data section up to 8 bytes would let it load. Matters only
  while 8- and 16-bit atomics are built from masked operations on the word
  around them; retired by BPF atomics of 8 and 16 bits, with which the
  pass goes.

On the kernel side the counterparts are "[WIP] bpf: Let callx call a
function address held in a scalar", "[WIP] bpf: Let callx of a number call
functions only data refers to", "[WIP] libbpf: Put JITed addresses of
functions into arena data", "[WIP] libbpf: Take addresses of functions as
copies for callx of a number", "[WIP] libbpf: Leave the slots of functions
that only trap NULL", "[WIP] bpf: Let bpf_throw leave with resources held
under a prog flag" with its selftests, "[WIP] bpf: Verify functions whose
address ld_imm64 takes without callx" and "[WIP] libbpf: Fill the slots of
functions whether or not a program has callx". Each message lists what is
not right about it.

### Known limits and open items

Calls through a number. The kernel's callx of a number verifies every
function whose address a program takes on its own, with numbers for its
arguments, and hands it whatever the registers hold. Everything below
follows from that and is retired by a proper design with verified function
pointers: libbpf's copies of a function for its address and for arena data,
so that one function is not verified with a stack pointer at a direct call
and an arena number at a call through its address ("same insn cannot be
used with different pointers"); the fill-in of the function slots of arena
data after load, the first program loaded winning, so that in an object of
several programs what `std::get_terminate()` returns depends on which
program loaded first (std::bpf's `toolchain/test/HandlersTest.bpf.cpp` says
how its tests are ordered for that); trap-only slots left NULL; the unwind
edge every call through a pointer keeps; the deferral of thrown objects'
destructors; and the handlers libc++abi and libc++ never call
(`libcxxabi/src/cxa_handlers.cpp`, `libcxx/src/support/new.ipp`: a call
through the stored pointer inside every destructor or allocating function
would be refused as recursion). A callee that takes a stack address is
wrong rather than refused: it reads the arena at the address's low bits.
`std::visit` over two variants, a table of function pointers, returns 0 for
operands on the stack and the right answer for operands in the arena
(std::bpf's `freestanding/SumTypesTest.bpf.cpp`); `std::function_ref`'s
thunk is refused on the trap in its copy or on "same insn"; a `std::function`
with a functor on the heap is refused as recursion through its vtable's
deallocation (`freestanding/Refused.bpf.cpp`). A WebAssembly-style shadow
stack for the locals such calls take was considered and deferred.

The kernel's ABI for a struct returned or passed by value. The compiler's
BTF describes the source prototype at both ends of a link: a function that
returns a struct larger than 16 bytes has a FUNC_PROTO that returns the
struct, while the ABI passes a hidden pointer to it in r1 ("[BPF] Keep the
source return type in the BTF of a function with an sret argument").
`btf_prepare_func_args()` does not model that pointer, so a global function
returning such a struct is refused ("Global function mk() has unsupported
return type"), and a static one whose sret pointer pushes its fifth source
parameter onto the stack is refused too ("the arguments of mk5() are not
known from its BTF, which the stack args need"). The same question applies
to a parameter larger than 16 bytes passed by value, which the ABI passes
as a pointer to a copy. Whether BTF is the source prototype, which the
verifier maps onto registers as the ABI does, or the ABI's register list,
is not decided on the kernel side; the compiler says the source, which the
kernel's own BTF says and from which the other reading could be derived.

A context passed by reference to a global function. A reference parameter
of a global function is tagged `arg:nonnull`, and the kernel refuses the
tag beside a context type ("invalid combination of tags"), so `void
f(struct xdp_md &ctx)` as a global function, not a program, is refused
where a `struct xdp_md *` parameter is typed as the context; a program's
own parameter gets no tag. The kernel could accept nonnull on a context,
which is never null either.

`bpf_throw` and held resources. `std::terminate()` ends a program through
`bpf_throw()`, which releases nothing a frame holds, so a throw under an IRQ
or lock guard did not verify; `BPF_F_THROW_NO_RELEASE` lets it through with
the resources held (`libcxxabi/src/abort_message.cpp`; std::bpf's
`host/Object.h` loads every program with the flag). Retired by `bpf_throw()`
releasing what the frames it discards hold.

Per-invocation state kept per CPU. Emulated TLS keeps a `thread_local` per
CPU (`compiler-rt/lib/builtins/emutls.c`), and libc++abi's
`__cxa_eh_globals`, llvm-libc's errno and libunwind's landing pad context
are `thread_local`. An invocation preempted or sleeping mid-exception can
see another's state on its CPU, and an exception that escapes a program
leaves its object allocated. The planned fix is 8 bytes in `struct
bpf_run_ctx` holding the address of the invocation's state, for the program
types that run with a run context, with a kfunc returning it; emutls's
comment has the design, and nothing in the runtimes changes with it.

Every CPU at once, without threads. The runtimes are built without thread
support, and what libc++ and libc++abi guard only under it is shared
between the CPUs a program runs on all the same: the reference count of an
exception object, which `std::exception_ptr` copies on two CPUs raise and
drop, and `std::shared_ptr`'s, are the compiler's atomic builtins on BPF
whatever the thread support (`libcxx/src/include/atomic_support.h`,
`libcxx/include/__memory/shared_count.h`); the fallback heap is under a
spin lock bounded by the loop budget (`libcxxabi/src/fallback_malloc.cpp`);
and the guard of a function-local static is a compare-and-exchange of the
whole 64-bit guard, the CPU that holds it recorded where the futex
implementation keeps its thread id, so that a second arrival on the same
CPU, a recursion or a program that interrupted the initializer, ends the
program rather than waits for itself (`libcxxabi/src/cxa_guard_impl.h`,
`InitByteBpf`). A waiter spins within the loop budget, since there is
nothing to sleep on; one behind a holder that is an interrupted program on
its own CPU burns the whole budget, 250 ms of iterations, and then fails
safely, its allocation failing, its block leaking or its program ending,
with the spent budget reported on the program's stderr stream. The three
hang on `__BPF__`, since the configuration has no finer word:
`_LIBCPP_HAS_THREADS=0` means single-threaded to libc++ and libc++abi,
which wasi and the bare-metal targets are and BPF is not, and the knob
that would say "atomic without threads" does not exist yet; its cleaner
shape upstream is one macro that the three places test. What needs threads
outright stays unusable: `std::atomic_load` and `std::atomic_store` of a
`shared_ptr` and everything else behind `__sp_mut`, the mutex table. A
handler inside a function whose address a program stores, a virtual
function say, is refused as recursion along with a thrown object's
destructor that catches: `__cxa_end_catch`, which ends every handler, calls
the destructors of thrown objects through the addresses their throws stored
(std::bpf's `toolchain/test/UnwindNegative`).

One instruction used with a stack and an arena pointer. The verifier rewrites
each instruction once, so a static subprogram reached with a stack object on
one path and an arena object on another is refused ("same insn cannot be
used with different pointers"): a class thrown in one place and kept on the
stack in another (the libbpf copies above work around it for destructors),
`~exception_ptr` on a `std::exception_ptr` kept on the heap and one passed
by value, libc's string functions on a stack buffer and an arena one,
`dynamic_cast` and virtual calls, whose objects must be in the arena. A
verifier-side answer, keying the rewrite by the edge through which a state
entered its basic block and duplicating the block prefix at fixup time, is
designed in the monorepo's note `notes/bpf/verifier-same-insn-rewrites.md`,
with what it covers and what it leaves to the compiler.

Atomics. A read-modify-write that needs a compare-and-swap loop, an exchange
or an add of 8 or 16 bits, `std::atomic<bool>`, `std::atomic_flag`,
`fetch_max` and `fetch_min`, a strong `compare_exchange` on a type narrower
than 32 bits, is refused by the verifier as an infinite loop
(`freestanding/Refused.bpf.cpp`); the fix is 8- and 16-bit and max/min
atomics in the ISA, or a `may_goto` bound on the loop. A narrow atomic on
a global whose 32-bit word extends past the global's data map is refused
as an invalid access to the map value, above. One caveat of the mapping
(`BPFISelLowering.cpp`, "How C++ atomics map onto BPF"): a seq_cst load is
a load-acquire, which on powerpc is RCpc and weaker than seq_cst asks; a
failed cmpxchg is ordered as its failure order asks by an expansion of its
own ("[BPF] Order a failed cmpxchg as its failure order asks"). Wait and
notify need a clock and threads and do not link
(`freestanding/compile/AtomicWait.bpf.cpp`).

libc++ algorithms without `can_loop`. A loop of libc++'s carries no bound
the verifier can use: `std::sort` is the recursive introsort ("recursive
call"); `std::gcd`, `std::ranges::partition`, a filtered view's distance and
`std::uniform_int_distribution`'s rejection loop are refused as infinite;
`std::stable_sort`, `std::partial_sort`, `std::nth_element`,
`std::string_view::find` of a string (libc++'s search loop with `memcmp`
inside, walked at every state), libc++.a's `__next_prime` and a range for
over a span of unknown size run past the budget; `std::from_chars` reads a
table on its stack by the count of digits parsed ("invalid read from
stack"), as does a `std::bitset` of more than one word, whose algorithms take
`__bit_iterator`s by value; a `std::string` on the stack indexes itself by a
size the verifier cannot bound; libc's `memcpy` returns its destination,
which a subprogram may not when it is a stack pointer, and so does
`__itoa::__base_10_u64` (`libcxx/include/__charconv/to_chars_base_10.h`),
which returns a pointer into the caller's stack buffer, so that
`std::to_string` links and does not verify ("cannot return stack pointer
to the caller"). `std::pmr` needs `.init_array` (`memory_resource.cpp` is
left out), `std::system_error`'s messages a `std::string` by value
(`system_error.cpp` is left out), and the demangler a deeper stack
(`cxa_demangle.cpp` is left out). No floating point, no variadic
definitions, no recursion, no unbounded loops: the ISA's and the verifier's,
not the library's. Each has an expectation test in std::bpf's
`freestanding/`, which asserts the refusal so that a change in either
direction is noticed.

libc's loops under the budget. A loop of llvm-libc's BPF configuration
leaves through `may_goto` when the budget is spent, and the routine returns
what it has: `memcmp`, `bcmp` and `strcmp` report the prefix they compared
as equal, and `memcpy` has copied a prefix. The kernel reports the cut on
the program's stderr stream, so this is a policy rather than a silent
failure, but a comparison cut short would be safer reporting unequal, so
that no caller takes two buffers for the same on a budget it did not know
it had; open ("[libc][BPF] Configure llvm-libc for BPF"). And a loop that
returns a pointer, `strchr`, `strcpy` or `memchr`, called on a base plus an
offset the verifier does not know, or on a pointer from `new`, with its
result compared against a pointer computed from the same offset, is walked
one iteration per state into the limit of 8,193 jumps and refused; `strlen`
on the same operands converges.

Library gaps found by std::bpf's audit of the freestanding headers, each
pinned by an expectation test there. `std::from_chars` in a base other
than 10 divides by a table of float logarithms (`__from_chars_log2f_lut` in
`libcxx/include/__charconv/from_chars_integral.h`), a floating-point
operation in a function P1642 marks freestanding; the bound can be computed
in integers (`freestanding/compile/FromCharsBase16.bpf.cpp`). libc++
defines none of the freestanding feature-test macros (P2198, P2407), pinned
in `freestanding/BitTest.bpf.cpp`. `vmlinux.h` has types named as the C
library names its own (`struct tm`, `union sigval`, `struct sigevent`,
`struct sched_param`, `timer_t`), which C++ refuses a second definition of;
a bpftool C++ rule is being added, and std::bpf's `tools/vmlinux-h` filters
the dump meanwhile.

Accepted. Every object with C++ runtime data carries the `.arena.data`
marker, which keeps `.data`, `.bss`, `.rodata` and the compiler's string
literals in the arena so that every pointer a program loads is one it can
follow; the price is what the verifier knows of read-only data, a load from
the arena being unknown where a load from a frozen map is a constant, which
costs std::bpf's allocator up to two and a half times the instructions to
verify its tables. A throw under an IRQ or lock guard is refused without
the flag above, and documented as such. An exception object lives in the
arena whenever it leaves a global function, since the kernel checks what a
global function unwinds with as it checks its return value and wants a
scalar or an arena pointer there: the rule above for every runtime, which
libc++abi's port meets through `malloc` and its fallback heap, and which a
runtime that threw a pointer to a static buffer or a map value would break.

### Upstreaming candidates

On the kernel side: "bpf: Let only fentry attach to a subprog that might
throw" (the crash of `bpf_throw` under an fexit trampoline), "bpf: Don't
continue after a call of a global subprog that only unwinds", "libbpf: Move
the jump tables of the code that the linker moves", nonnull beside the
context's tag in `btf_prepare_func_args()`, and from this tree the
disjoint-or selection when it is reconsidered. "libbpf: Link only COMDAT data
named after its symbol into base sections" and the COMDAT folding around it
need discussion with libbpf's maintainers first: whether the linker should
fold a compiler-named subsection into its base section at all. On the
compiler side, the `TargetTransformInfo` hook that keeps the arguments of a
call for DeadArgumentElimination is written to be sent as it is, and the
condition of `shouldEmitFunction` that refers to an inline member of an
explicit instantiation declaration as a declaration is keyed on
`allowDebugInfoForExternalRef()`, the BTF switch, where a reviewer may want
the property named for what it is, a target that links through debug info.
