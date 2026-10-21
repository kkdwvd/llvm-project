//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// The platform layer under the BPF sysroot's libc++abi, for its tests: what
// the sysroot's libc++.a, libc++abi.a, libc.a and builtins leave undefined.
//
// - The arena map, which std::bpf's Arena.bpf.o prepares and the test runner
//   maps (std::bpf's memory/Arena.h declares it);
// - malloc, free and aligned_alloc, a small single-threaded allocator over
//   bpf_arena_alloc_pages: power-of-two size classes from 16 bytes to 2 MB
//   carved from 2 MB regions, a free list per class, a 16-byte header before
//   each block and the block's address stored just before what is handed
//   out, so that an over-aligned request is served from a larger class. The
//   work is a global subprogram taking and returning numbers, checked once per
//   program; malloc and the others are hidden wrappers that cast.
// - The license.
//
// The kernel's bpf_throw and the CPU helper, which libc++abi declares itself,
// are the kernel's.
#include <vmlinux.h>

#include <bpf/bpf_helpers.h>

#include <cstddef>
#include <cstdint>

#include <std/bpf/memory/Arena.h>
#include <std/bpf/verifier/Loop.h>

extern "C" {
char _license[] SEC("license") = "GPL";
ArenaMap arena SEC(".maps");
}

namespace {

constexpr unsigned kClasses = 18; // 16 << 0 .. 16 << 17: 16 bytes to 2 MB
constexpr std::uint64_t kRegionPages = 512; // 2 MB
constexpr std::uint64_t kPageSize = 4096;

struct Block {
  std::uint64_t cls;
  std::uint64_t next; // on the free list; otherwise unused
};

struct Heap {
  std::uint64_t freeList[kClasses];
  std::uint64_t bump;
  std::uint64_t bumpEnd;
  std::uint64_t allocations;
  std::uint64_t frees;
  std::uint64_t regions;
  std::uint64_t failures;
};

Heap heap SEC(".addr_space.1");

} // namespace

extern "C" {

/// `size` bytes aligned to `align` (a power of two), or 0.
[[gnu::noinline]] std::uint64_t platform_malloc(std::uint64_t size, std::uint64_t align) noexcept {
  if (align < 16) {
    align = 16;
  }
  const std::uint64_t need = size + sizeof(Block) + (align > 16 ? align : 0);
  unsigned cls = std::bpf::opaqueZero();
  for (; cls < kClasses && can_loop; ++cls) {
    if ((std::uint64_t{16} << cls) >= need) {
      break;
    }
  }
  if (cls >= kClasses) {
    heap.failures += 1;
    return 0;
  }
  const std::uint64_t classSize = std::uint64_t{16} << cls;
  std::uint64_t block = heap.freeList[cls];
  if (block != 0) {
    heap.freeList[cls] = reinterpret_cast<Block*>(block)->next;
  } else {
    // Carve from the current region, at a multiple of the class size up to
    // a page, so that the block is aligned to it; a new region when it is
    // full.
    const std::uint64_t carveAlign = classSize < kPageSize ? classSize : kPageSize;
    std::uint64_t at = (heap.bump + carveAlign - 1) & ~(carveAlign - 1);
    if (at + classSize > heap.bumpEnd) {
      void* memory = std::bpf::arenaPointer(bpf_arena_alloc_pages(&arena, nullptr, kRegionPages, std::bpf::kNumaNoNode, 0));
      if (memory == nullptr) {
        heap.failures += 1;
        return 0;
      }
      heap.regions += 1;
      at = reinterpret_cast<std::uint64_t>(memory);
      heap.bumpEnd = at + kRegionPages * kPageSize;
    }
    heap.bump = at + classSize;
    block = at;
  }
  reinterpret_cast<Block*>(block)->cls = cls;
  std::uint64_t user = block + sizeof(Block);
  user = (user + align - 1) & ~(align - 1);
  reinterpret_cast<std::uint64_t*>(user)[-1] = block;
  heap.allocations += 1;
  return user;
}

[[gnu::noinline]] void platform_free(std::uint64_t address) noexcept {
  if (address == 0) {
    return;
  }
  const std::uint64_t block = reinterpret_cast<std::uint64_t*>(address)[-1];
  const std::uint64_t cls = reinterpret_cast<Block*>(block)->cls;
  if (cls >= kClasses) {
    return;
  }
  reinterpret_cast<Block*>(block)->next = heap.freeList[cls];
  heap.freeList[cls] = block;
  heap.frees += 1;
}

[[gnu::visibility("hidden")]] void* malloc(size_t size) noexcept {
  return reinterpret_cast<void*>(platform_malloc(size, 16));
}

[[gnu::visibility("hidden")]] void* aligned_alloc(size_t alignment, size_t size) noexcept {
  return reinterpret_cast<void*>(platform_malloc(size, alignment));
}

[[gnu::visibility("hidden")]] void free(void* pointer) noexcept {
  platform_free(reinterpret_cast<std::uint64_t>(pointer));
}

} // extern "C"
