# The C and C++ runtimes of a BPF program: compiler-rt's builtins, llvm-libc,
# libunwind, libc++abi and libc++, built for one BPF triple into a sysroot, as
# the runtimes of an embedded or a WebAssembly target are. Expects the triple
# to be passed as `RUNTIMES_TARGET_TRIPLE`; bpfel-unknown-none.cmake and
# bpfeb-unknown-none.cmake pass it.
#
#   cmake -S runtimes -B build-bpf -G Ninja \
#     -C libc/cmake/caches/bpfel-unknown-none.cmake \
#     -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
#     -DCMAKE_INSTALL_PREFIX=sysroot
#   ninja -C build-bpf install
#
# installs, with LLVM_ENABLE_PER_TARGET_RUNTIME_DIR, the C headers and
# libunwind's to include/<triple>/, the libc++ headers with their
# __config_site and libc++abi's cxxabi.h to include/<triple>/c++/v1/, and
# libc.a, libunwind.a, libc++.a, libc++abi.a and the builtins'
# libclang_rt.builtins.a to lib/<triple>/, which clang's BPF driver
# searches; -DCOMPILER_RT_INSTALL_PATH=lib/clang/<major> puts the builtins in
# the resource directory's layout instead, where every driver looks for them
# when the prefix is the compiler's own.
#
# BPF programs run in the kernel, under its verifier: no floating point, no
# variadic definitions, no recursion, loops the verifier can bound, 512-byte
# stacks, no static constructors, and a library function is one static
# subprogram per call site when its symbol is hidden, which is what these
# runtimes make every symbol.

set(CMAKE_SYSTEM_PROCESSOR bpf CACHE STRING "")
set(LLVM_ENABLE_RUNTIMES "compiler-rt;libc;libunwind;libcxxabi;libcxx" CACHE STRING "")
set(LLVM_ENABLE_PER_TARGET_RUNTIME_DIR ON CACHE BOOL "")

# -O2 -g: BTF, which libbpf links objects by, comes from the debug info, and
# with -fstandalone-debug it describes every type in full, which libbpf's
# linker wants when it matches a declaration against a definition. The ISA
# level is the kernel's current one; a consumer may lower it.
set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "")
foreach(lang C;CXX;ASM)
  set(CMAKE_${lang}_FLAGS "-mcpu=v4 -fstandalone-debug" CACHE STRING "")
endforeach()

# compiler-rt: the builtins alone, and of those what BPF calls
# (compiler-rt/lib/builtins/CMakeLists.txt). The atomic library is a lock
# table, which no BPF program runs.
set(COMPILER_RT_EXCLUDE_ATOMIC_BUILTIN ON CACHE BOOL "")
set(COMPILER_RT_BUILD_CRT OFF CACHE BOOL "")

# llvm-libc: the bare-metal configuration (libc/config/baremetal) with the
# headers, entrypoints and configuration of libc/config/baremetal/bpf, whose
# errno is thread_local, as a hosted libc's is. The headers declare every
# function of the standard, not only the entrypoints: what a program can
# define for itself, printf and clock_gettime say, is declared and left
# undefined, as libc++'s <cstdio> and <chrono> expect of a C library.
set(LLVM_LIBC_ALL_HEADERS ON CACHE BOOL "")
set(RUNTIMES_USE_LIBC "llvm-libc" CACHE STRING "")

include(${CMAKE_CURRENT_LIST_DIR}/baremetal_common.cmake)

# libunwind: the kernel unwinds, so the library is the landing pad context
# that a landing pad fills in before it calls the personality routine and the
# _Unwind_* accessors the personality reads it through, as it is for
# WebAssembly (libunwind/src/Unwind-bpf.c). A static library with every
# symbol hidden, no threads, and no assertions, whose logging a BPF program
# has no stdio for. Its headers go to the target's directory with the C
# headers; libc++abi is built against them and linked with the library.
set(LIBUNWIND_ENABLE_SHARED OFF CACHE BOOL "")
set(LIBUNWIND_ENABLE_STATIC ON CACHE BOOL "")
set(LIBUNWIND_HIDE_SYMBOLS ON CACHE BOOL "")
set(LIBUNWIND_ENABLE_THREADS OFF CACHE BOOL "")
set(LIBUNWIND_ENABLE_ASSERTIONS OFF CACHE BOOL "")
set(LIBUNWIND_IS_BAREMETAL ON CACHE BOOL "")
set(LIBUNWIND_USE_COMPILER_RT ON CACHE BOOL "")
set(LIBUNWIND_ENABLE_CROSS_UNWINDING OFF CACHE BOOL "")
set(LIBUNWIND_INSTALL_INCLUDE_DIR "include/${RUNTIMES_TARGET_TRIPLE}" CACHE STRING "")

# libc++abi: the C++ ABI library, a static library with every symbol hidden,
# exceptions and RTTI on, no threads, and the unwinder above: the backend
# lowers unwinding to the instructions the kernel verifies, every landing
# pad is entered, and one with catch clauses for types calls the personality
# routine itself, as a WebAssembly catch does, which reads the function's
# exception table through libunwind's landing pad context. libc++abi's BPF
# sources (libcxxabi/src/*_bpf.cpp) replace the recursive type_info searches
# with bounded walks. The hermetic static library drops the visibility
# annotations and hides the global operator new and delete, so that every
# function is a static subprogram to libbpf. Terminate is silent and the
# assertions are off: a BPF program prints nothing, and has nothing to report
# a failed assertion with. The demangler is left out. The headers go to
# the target's C++ directory, where clang's BPF driver looks for cxxabi.h,
# which it includes in every C++ unit: libbpf links the calls the compiler
# makes on its own for a throw, a catch or a dynamic_cast through the types
# of that declaration.
set(LIBCXXABI_ENABLE_SHARED OFF CACHE BOOL "")
set(LIBCXXABI_ENABLE_STATIC ON CACHE BOOL "")
set(LIBCXXABI_HERMETIC_STATIC_LIBRARY ON CACHE BOOL "")
set(LIBCXXABI_ENABLE_EXCEPTIONS ON CACHE BOOL "")
set(LIBCXXABI_ENABLE_THREADS OFF CACHE BOOL "")
set(LIBCXXABI_USE_LLVM_UNWINDER ON CACHE BOOL "")
set(LIBCXXABI_USE_COMPILER_RT ON CACHE BOOL "")
set(LIBCXXABI_SILENT_TERMINATE ON CACHE BOOL "")
set(LIBCXXABI_ENABLE_ASSERTIONS OFF CACHE BOOL "")
set(LIBCXXABI_ADDITIONAL_COMPILE_FLAGS "-fexceptions;-frtti;-fno-threadsafe-statics;-include;cxxabi.h" CACHE STRING "")
set(LIBCXXABI_INCLUDE_TESTS OFF CACHE BOOL "")
set(LIBCXXABI_INSTALL_INCLUDE_DIR "include/${RUNTIMES_TARGET_TRIPLE}/c++/v1" CACHE STRING "")

# libc++: a static library with every symbol hidden, exceptions and RTTI on,
# and nothing that needs an operating system. LIBCXX_HERMETIC_STATIC_LIBRARY
# strips the visibility annotations, so that every function of the library
# is a static subprogram to libbpf. The compiler's defaults for C++ on BPF
# are exceptions and RTTI on, as on hosted targets; the library says so
# itself, as a runtimes build does, so that a consumer's flags cannot turn
# them off under it. A unit that throws calls the ABI's
# __cxa_allocate_exception and __cxa_throw, which clang declares itself, and
# clang takes their BTF from the declaration the unit has: libc++abi's
# cxxabi.h, included in every unit, as the BPF driver includes it in every
# unit of a program. With no threads a function-local static needs no guard.
set(LIBCXX_ENABLE_SHARED OFF CACHE BOOL "")
set(LIBCXX_ENABLE_STATIC ON CACHE BOOL "")
set(LIBCXX_HERMETIC_STATIC_LIBRARY ON CACHE BOOL "")
# The headers a program sees drop the annotations too: an operator new a
# program defines, hidden as every function of a BPF object is, must not
# meet a declaration that says default.
set(LIBCXX_EXTRA_SITE_DEFINES "_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS" CACHE STRING "")
set(LIBCXX_ENABLE_EXCEPTIONS ON CACHE BOOL "")
set(LIBCXX_ENABLE_RTTI ON CACHE BOOL "")
set(LIBCXX_ADDITIONAL_COMPILE_FLAGS "-fexceptions;-frtti;-fno-threadsafe-statics;-include;cxxabi.h" CACHE STRING "")
set(LIBCXX_ENABLE_THREADS OFF CACHE BOOL "")
set(LIBCXX_ENABLE_LOCALIZATION OFF CACHE BOOL "")
set(LIBCXX_ENABLE_FILESYSTEM OFF CACHE BOOL "")
set(LIBCXX_ENABLE_RANDOM_DEVICE OFF CACHE BOOL "")
set(LIBCXX_ENABLE_MONOTONIC_CLOCK OFF CACHE BOOL "")
set(LIBCXX_ENABLE_TIME_ZONE_DATABASE OFF CACHE BOOL "")
set(LIBCXX_ENABLE_UNICODE OFF CACHE BOOL "")
set(LIBCXX_ENABLE_WIDE_CHARACTERS OFF CACHE BOOL "")
set(LIBCXX_HARDENING_MODE "none" CACHE STRING "")
set(LIBCXX_USE_COMPILER_RT ON CACHE BOOL "")
set(LIBCXX_INCLUDE_BENCHMARKS OFF CACHE BOOL "")
set(LIBCXX_INCLUDE_TESTS OFF CACHE BOOL "")

# The ABI library is the libc++abi built here: <exception>, <typeinfo> and
# the exception_ptr are the ABI's, and operator new and delete are defined
# there, over the platform's malloc and free.
set(LIBCXX_CXX_ABI "libcxxabi" CACHE STRING "")
set(LIBCXX_ENABLE_NEW_DELETE_DEFINITIONS OFF CACHE BOOL "")

# Every header under the triple, as wasi-sdk lays a multiarch sysroot out:
# the target directory holds __config_site anyway.
set(LIBCXX_INSTALL_INCLUDE_DIR "include/${RUNTIMES_TARGET_TRIPLE}/c++/v1" CACHE STRING "")
