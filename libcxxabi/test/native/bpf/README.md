# libc++abi's tests as BPF programs

The tests of libc++abi that exercise the ABI a program sees, ported to BPF
programs that run in a Linux kernel under the verifier: the catch tests
(`catch_class_01..04`, `catch_ptr`, `catch_ptr_02`, the nullptr and
member-pointer catches, `catch_array_01/02`, `catch_function_01..03`,
`catch_in_noexcept`, `inherited_exception`, `unwind_01`,
`uncaught_exception` and `uncaught_exceptions`), `dynamic_cast`,
`dynamic_cast3`, `dynamic_cast5`, `dynamic_cast14`, `cxa_bad_cast`,
`cxa_bad_typeid`, `test_aux_runtime` and `test_aux_runtime_op_array_new`,
`exception_object_alignment`, `exception_object_alignment.2`,
`test_exception_address_alignment`, `test_vector1..3` and
`cxa_vec_new_overflow_PR41395`, with `GuardTest`, the guards of
function-local statics on BPF (`guard_test_basic`'s ABI calls, a static
initialized once, an initializer that throws and is retried, and a
recursive initialization that terminates) and a catch of a null pointer to
a base reached by a virtual and a direct path: 101 programs in nine
objects. The
hierarchies, casts, throws and expectations are upstream's; what changed
in the port is said at the top of each file. Each upstream test is one
`BPF_TEST`, its `assert()` an `EXPECT_TRUE()`, its `main()` the body, and
an object a program cannot keep on its stack (one with a virtual base,
whose run-time check takes its address as a number) lives in the arena.
A program upstream expects to be terminated is in a suite named
`*Terminates`, and the runner reports it as ended through
`std::terminate()`.

They are not lit tests: a BPF program is loaded by a host process, verified
by the kernel and run through `bpf_prog_test_run()`, and a check inside it
can only record itself in memory the host reads afterwards. The harness is
the one of std::bpf, the C++ platform library for BPF programs
(`src/bpf/cpp` of the kkdwvd monorepo): its `BPF_TEST` framework
(`<std/bpf/testing/BpfTest.h>`), its `bpf_test_runner`, which loads an
object, runs the selected tests and prints each failed check with its
line and operands, and its `tools/vm-run`, which boots a vng guest on a
kernel and runs a command in it. `lit.local.cfg` keeps lit out of this
directory.

## What a run needs

- clang's BPF toolchain from this tree (`clang/docs/BPFSupport.md`), built
  with the BPF backend, and a BPF sysroot built from the same checkout
  with the runtimes cache `libc/cmake/caches/bpfel-unknown-none.cmake`:
  llvm-libc, libunwind's BPF glue, libc++, libc++abi's BPF port and
  compiler-rt's builtins. The cache's header says the configure line.
- A kernel from the kkdwvd/linux fork on branch `cpp-bpf`, which carries
  the `unwind_goto`/`unwind_exit` series, libbpf's C++ support (COMDAT
  groups, archives, read-only data in the arena, the function slots of
  arena data) and the work-in-progress callx of a number that a thrown
  object's destructor and every virtual call go through. Its libbpf and
  `bpftool` are the linker and the loader; the std::bpf build makes both
  from the tree it is pointed at.
- A build of std::bpf against that compiler, sysroot and kernel, which
  provides the objects the harness links into every test (the arena
  prepare program with the `.arena.data` marker, the opaque zero of the
  tests' loops and the results global the runner reads), the runner and
  the VM script. The tests link the sysroot's archives and
  `support/platform.bpf.cpp`, a stand-in for what the sysroot leaves to
  the platform (`malloc`, `free`, `aligned_alloc` and the licence), rather
  than std::bpf's allocator, so that what is under test is libc++abi on
  the sysroot alone.
- vng (virtme-ng) on the host.

## Running

```sh
export CLANGXX=/path/to/llvm/build/bin/clang++   # clang++ with the BPF backend
export SYSROOT=/path/to/bpf-sysroot              # include/bpfel-unknown-none/...
export BPFTOOL=/path/to/std-bpf/build/bpftool/bpftool
export STD_BPF_SOURCE=/path/to/kkd/src/bpf/cpp
export STD_BPF_BUILD=$STD_BPF_SOURCE/build
export KERNEL=/path/to/linux/.kdev/build/kernel  # a build directory or a bzImage

./build.sh            # compiles and links every test, build/build-results.txt
./run-vm.sh           # boots one guest, runs every program, build/run-results.txt
./run-one.sh VectorTest:Vector.Empty   # the verifier's full log of one program
```

`build.sh` takes test stems to build a subset; `run-vm.sh` runs every
linked object, or the stems given, one program per runner invocation so
that a program the verifier refuses hides none of the others, and writes
one line per program: `OK` with its checks and the verifier's instruction
count, `FAIL` with the first failed check, `LOAD` with the verifier's
reason. `BUILD` names another output directory, `VNG_ARGS` other vng
arguments (the default `--disable-microvm` is what the cpp-bpf kernel
needs), `CPUS` and `MEMORY` the guest's size.

## What the results mean

A run passes when every program is `OK` and the two `VectorTerminates`
programs are reported as ended through `std::terminate()`, as upstream's
`test_vector2` and `test_vector3` expect to be terminated: 101 programs in
all. Two of them need the kernel to verify a function whose address
`ld_imm64` takes whether or not the program calls through a number:
`Vector.Empty` and `Vector.Counted` throw `std::bad_array_new_length` on a
path the verifier proves dead, so the destructor whose address the throw
takes is reached by no call the verifier walks, and its slot in the vtable
would otherwise stay empty. What the port leaves open is in
`clang/docs/BPFSupport.md`, "Status and open work".

Two files of upstream's suite have no counterpart here on purpose:
`test_demangle` and the demangler, which the BPF build of libc++abi leaves
out (a recursive descent parser whose frames exceed a BPF stack), and
`cxa_thread_atexit_test`, since there are no threads. `catch_multi_level_pointer`,
`catch_pointer_reference`, `cxa_call_terminate`, `forced_unwind*`,
`guard_threaded_test`, `incomplete_type`, `noexception*`,
`thread_local_destruction_order`, `unwind_02..06` and `uncaught_exception`
on the ARM EHABI side are not ported yet, for want of time or of what they
test (forced unwinding and threads do not exist on BPF). What two CPUs do
to one guard, one exception's reference count or the fallback heap at once
is tested from the platform's side, by std::bpf's `ConcurrencyTest`, which
runs programs on two CPUs.
