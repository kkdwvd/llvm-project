//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// private_typeinfo.cpp for BPF: the type_info classes of the Itanium C++ ABI,
// the matching of a handler to a thrown type and the run-time check of
// dynamic_cast, written for the kernel's verifier. Adapted from std::bpf's
// rtti library.
//
// The other targets tell the kinds of type_info apart with virtual calls and
// search a class graph by recursing through them. A BPF program makes no
// virtual call in its runtime (a call through a pointer loaded from memory is
// checked against every function whose address the program stores, and such a
// callee may make no such call itself) and cannot recurse. So:
//
// - A type_info's kind is read as typeid reads it, from the type_info slot of
//   the vtable its first word points to, and compared with the type_info of
//   each ABI class, which this unit defines by defining each class's
//   destructor, its key function. Defining __fundamental_type_info's also
//   makes the compiler emit the type_infos of the fundamental types here, as
//   it does on every target.
//
// - A class graph is walked depth first on an explicit stack of frames, one
//   per class on the path from the root to the current subobject, driven one
//   step at a time from a single bounded loop (hierarchy_search). Every path
//   is walked, since the access of two paths to one virtual base can differ
//   and dynamic_cast needs to know which subobject of the target type each
//   path passes through. The walk has limits, 64 frames and 4096 subobjects
//   visited, past any real hierarchy (a chain 64 classes deep, a ladder of
//   eleven virtual diamonds); a walk that needs more, or meets a malformed
//   graph, or runs out of the kernel's loop budget, has no answer, and the
//   program terminates: a handler skipped or a cast refused for want of a
//   walk would be a wrong answer the program carried on with, where the
//   recursive search would catch or crash.
//
// - The search is 1,608 bytes, which no BPF stack frame holds (512 bytes), so
//   it is allocated for each walk from the library's heap, or from the
//   fallback heap when that has nothing (fallback_malloc.cpp), and freed after
//   it, and only for a walk: a clause the kinds, the names or the qualifiers
//   settle takes none. A walk with no memory for it ends the program, as a
//   throw with no memory for its object does.
//
// - What the verifier pays for a walk is its state: it knows every value
//   written to the stack as it was written, and states that differ in one do
//   not prune, so a walk on the stack of its caller cost it hundreds of
//   thousands of instructions. Memory a global subprogram reaches through a
//   number is memory the verifier does not track. So the walks run in global
//   subprograms that take the search's address as a number
//   (bpf_platform.h), checked once per program at about two thousand
//   instructions each, as does the comparison of type names; the visitor
//   keeps what it found in the search's memory, the frames are indexed
//   through a mask rather than a clamp, the depth and the steps count from a
//   zero the verifier cannot see, and a step returns what it did rather than
//   keeping it, so that nothing a register carries from one iteration to the
//   next is a value the verifier knows.
//
// Positions. With an object, a subobject's position is its address: a
// non-virtual base sits at the offset its base record gives from the class
// that names it, a virtual base at the offset that class's vtable holds in the
// slot the record names. Without an object (a thrown null pointer) no vtable
// can be read, and a position is the offset from the innermost virtual base on
// the path, its anchor, or from the root where the path has none: two paths
// reach the same subobject exactly when their anchors are the same type and
// their positions agree. This is what the other targets' vbase_cookie does.
//
// The matching is [except.handle]/3 as private_typeinfo.cpp has it, with the
// same departures from the standard: no pointer-to-member conversion
// ([conv.mem]) takes part, and the incomplete-class flags of a pointer's
// type_info are compared by name. Two type_infos are equal when they are one
// object or share their name, libc++'s unique comparison on this target, and
// by the string of their names where the other targets use strcmp, for a
// pointer to an incomplete class, whose type_info has a copy in every unit.
//
//===----------------------------------------------------------------------===//

#include "private_typeinfo.h"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <typeinfo>

#include "bpf_platform.h"
#include "fallback_malloc.h"

namespace __cxxabiv1 {

// The key functions. Each class's vtable and type_info are emitted here.

__shim_type_info::~__shim_type_info() {}
__fundamental_type_info::~__fundamental_type_info() {}
__array_type_info::~__array_type_info() {}
__function_type_info::~__function_type_info() {}
__enum_type_info::~__enum_type_info() {}
__class_type_info::~__class_type_info() {}
__si_class_type_info::~__si_class_type_info() {}
__vmi_class_type_info::~__vmi_class_type_info() {}
__pbase_type_info::~__pbase_type_info() {}
__pointer_type_info::~__pointer_type_info() {}
__pointer_to_member_type_info::~__pointer_to_member_type_info() {}

namespace {

//===----------------------------------------------------------------------===//
// Kinds and equality
//===----------------------------------------------------------------------===//

// The kind of a type_info, read as typeid reads a polymorphic object: its
// vtable's type_info slot, compared with the type_info of the ABI class, which
// this unit defines once. A reference, so that the compiler inserts no null
// check with its throw of std::bad_typeid.
template <class _Tp>
inline bool is_a(const __shim_type_info* ti) {
  const std::type_info& dynamic = *ti;
  return &typeid(dynamic) == &typeid(_Tp);
}

inline bool is_class(const __shim_type_info* ti) {
  return is_a<__class_type_info>(ti) || is_a<__si_class_type_info>(ti) || is_a<__vmi_class_type_info>(ti);
}

// The longest name compared by its string: a longer pair, or a comparison the
// loop budget cuts short, is unequal, so a match can be missed but never made
// wrongly.
constexpr std::uint32_t kMaxTypeNameLength = 4096;

} // namespace

// Whether two names are one string, in a global subprogram the verifier
// checks once rather than at every comparison inside a walk.
_LIBCXXABI_BPF_GLOBAL_SUBPROGRAM unsigned long long
__libcxxabi_bpf_type_names_equal(unsigned long long a, unsigned long long b) noexcept {
  const char* x = reinterpret_cast<const char*>(a);
  const char* y = reinterpret_cast<const char*>(b);
  for (std::uint32_t i = _LIBCXXABI_OPAQUE_ZERO; i < kMaxTypeNameLength && _LIBCXXABI_CAN_LOOP; ++i) {
    const char c = x[i];
    if (c != y[i])
      return 0;
    if (c == '\0')
      return 1;
  }
  return 0;
}

namespace {

// Equality as the other targets have it: libc++'s type_info comparison, by
// the identity of the name, unless asked to compare the strings, which the
// matching asks for a pointer to an incomplete class.
inline bool is_equal(const std::type_info* x, const std::type_info* y, bool use_strcmp) {
  if (x == y || x->name() == y->name())
    return true;
  if (!use_strcmp)
    return false;
  return __libcxxabi_bpf_type_names_equal(
             reinterpret_cast<unsigned long long>(x->name()), reinterpret_cast<unsigned long long>(y->name())) != 0;
}

// The two fundamental types the matching rules name, told by their mangled
// names, which no compound type shares.
inline bool is_void(const __shim_type_info* ti) {
  const char* name = ti->name();
  return is_a<__fundamental_type_info>(ti) && name[0] == 'v' && name[1] == '\0';
}

inline bool is_nullptr_t(const __shim_type_info* ti) {
  const char* name = ti->name();
  return is_a<__fundamental_type_info>(ti) && name[0] == 'D' && name[1] == 'n' && name[2] == '\0';
}

//===----------------------------------------------------------------------===//
// Vtables (ABI 2.5.2)
//===----------------------------------------------------------------------===//

// A polymorphic object starts with its vptr, the address point of its vtable;
// the two slots before it hold the offset from the object to the most derived
// object and that object's type_info, and a class with virtual bases keeps
// their offsets in slots before those.

inline const std::intptr_t* vtable_of(const void* object) { return *static_cast<const std::intptr_t* const*>(object); }

inline std::intptr_t offset_to_top(const std::intptr_t* vtable) { return vtable[-2]; }

inline const __shim_type_info* most_derived_type(const std::intptr_t* vtable) {
  return reinterpret_cast<const __shim_type_info*>(vtable[-1]);
}

inline std::intptr_t virtual_base_offset(const std::intptr_t* vtable, long slot) {
  return *reinterpret_cast<const std::intptr_t*>(reinterpret_cast<const char*>(vtable) + slot);
}

//===----------------------------------------------------------------------===//
// The walk
//===----------------------------------------------------------------------===//

// What a step of a hierarchy_search did.
enum class search_step : std::uint8_t {
  ended,    // the walk is over: complete, failed, or cut short
  arrived,  // down to a base of the subobject, or to the root at the first step
  returned, // back up to the subobject the last one is a base of
};

// Subobjects of one type that a walk arrived at: how many distinct ones, up to
// two (more count as two), and of the first its position, its anchor, and
// whether some path to it was public.
struct found_subobjects {
  std::uint64_t position;
  const __shim_type_info* anchor;
  std::uint32_t count;
  std::uint32_t is_public;

  void add(std::uint64_t at, const __shim_type_info* at_anchor, bool public_path) {
    if (count == 0) {
      count     = 1;
      position  = at;
      anchor    = at_anchor;
      is_public = public_path;
    } else if (
        at == position &&
        (at_anchor == anchor || (at_anchor != nullptr && anchor != nullptr && is_equal(at_anchor, anchor, false)))) {
      is_public = is_public || public_path;
    } else {
      count = 2;
    }
  }
};

// What a walk's visitor keeps from one subobject to the next, in the search's
// memory rather than on the caller's stack (see the file comment).
struct walk_notes {
  found_subobjects found[2];
  std::uint64_t flag;
};

class hierarchy_search {
public:
  // The longest path from the class searched, counting it, and the most
  // subobjects a search visits, counting every path.
  static constexpr std::uint32_t kMaxDepth = 64;
  static constexpr std::uint32_t kMaxSteps = 4096;
  // What marked_level() returns when nothing on the path is marked.
  static constexpr std::uint32_t kNoLevel = 255;

  // Starts a walk of the subobjects of root, a class type, at object, or with
  // no object if it is null, with its notes zeroed. The first step arrives at
  // the root. A root that is not a class fails the walk at once.
  void start(const __shim_type_info* root, const void* object);

  // Moves down to the next base of the current subobject (arrived), or back
  // up to the subobject it is a base of when it has none left (returned);
  // ended once the walk has come back up past the root (complete()) or
  // failed, and at every step after.
  search_step step();

  bool complete() const { return state_ == state::complete; }

  // Whether a walk could not be finished: it ran out of frames, steps or loop
  // budget, or met a malformed graph. Not the same as no match.
  bool failed() const { return state_ == state::failed; }

  // Marks a walk that did not complete as failed: what a caller does when its
  // loop ended without the walk ending.
  void fail() {
    state_ = state::failed;
    depth_ = 0;
  }

  // Forgets any walk: what memory nothing constructed needs before one.
  void clear() {
    state_ = state::idle;
    depth_ = 0;
  }

  walk_notes& notes() { return notes_; }

  // The current subobject: the one the last step arrived at or returned to.

  const __shim_type_info* type() const { return top().type; }

  // Its address, or without an object its offset from anchor().
  std::uintptr_t position() const { return top().position; }

  // Without an object, the innermost virtual base on its path, null for none;
  // with one, always null, as positions are addresses.
  const __shim_type_info* anchor() const {
    if (has_object_)
      return nullptr;
    const std::uint32_t level = top().last_virtual;
    return level == 0 ? nullptr : frame_at(level).type;
  }

  // Whether every base specifier on its path from the root is public.
  bool public_path() const { return top().last_private == 0; }

  // Leaves its bases unvisited.
  void skip_bases() {
    frame& f    = top();
    f.next_base = f.base_count;
  }

  // Marks it, for the subobjects below it on this path.
  void mark() { top().marked_level = static_cast<std::uint8_t>(depth_ - 1); }

  // The level of the marked subobject on its path (the root is 0, the current
  // subobject depth() - 1), kNoLevel for none.
  std::uint32_t marked_level() const { return top().marked_level; }

  std::uintptr_t position_at(std::uint32_t level) const { return frame_at(level).position; }

  // Whether the path from the subobject at level down to it is public.
  bool public_from(std::uint32_t level) const { return top().last_private <= level; }

private:
  // idle is zero, so that zeroed memory is a search that neither completed
  // nor failed.
  enum class state : std::uint8_t { idle, walking, complete, failed };

  enum class kind : std::uint8_t { none, class_, si_class, vmi_class };

  struct frame {
    const __shim_type_info* type;
    std::uintptr_t position;
    std::uint16_t next_base;
    std::uint16_t base_count;
    kind kind_;
    // The level of the deepest subobject on the path reached through a
    // non-public base specifier, 0 for none (the root has no specifier).
    std::uint8_t last_private;
    // The same for a virtual one: the anchor's level.
    std::uint8_t last_virtual;
    std::uint8_t marked_level;
  };
  static_assert(sizeof(frame) == 24, "");

  // A power of two of frames, so that a level masked to an index is in bounds
  // without a branch, which would double the verifier's paths at every access.
  static constexpr std::uint32_t kFrameSlots = 64;
  static_assert(kMaxDepth <= kFrameSlots && (kFrameSlots & (kFrameSlots - 1)) == 0, "");

  frame& frame_at(std::uint32_t level) { return frames_[__libcxxabi_bpf_checked(level) & (kFrameSlots - 1)]; }
  const frame& frame_at(std::uint32_t level) const {
    return frames_[__libcxxabi_bpf_checked(level) & (kFrameSlots - 1)];
  }
  frame& top() { return frame_at(depth_ - 1); }
  const frame& top() const { return frame_at(depth_ - 1); }

  // The next_base of a root not yet arrived at.
  static constexpr std::uint16_t kRootPending = 0xffff;

  // Reads the kind of a class type and how many direct bases it has; false if
  // it is not a class or has more than a frame counts.
  static bool class_of(const __shim_type_info* type, kind* k, std::uint16_t* count);

  // Pushes the next base of the current subobject; false if the walk cannot
  // go on.
  bool descend();

  frame frames_[kFrameSlots];
  std::uint32_t depth_;
  std::uint32_t steps_;
  walk_notes notes_;
  state state_;
  bool has_object_;
};

static_assert(sizeof(hierarchy_search) == 1608, "");

bool hierarchy_search::class_of(const __shim_type_info* type, kind* k, std::uint16_t* count) {
  if (is_a<__si_class_type_info>(type)) {
    *k     = kind::si_class;
    *count = 1;
    return true;
  }
  if (is_a<__vmi_class_type_info>(type)) {
    const unsigned bases = static_cast<const __vmi_class_type_info*>(type)->__base_count;
    if (bases >= kRootPending)
      return false;
    *k     = kind::vmi_class;
    *count = static_cast<std::uint16_t>(bases);
    return true;
  }
  if (is_a<__class_type_info>(type)) {
    *k     = kind::class_;
    *count = 0;
    return true;
  }
  return false;
}

void hierarchy_search::start(const __shim_type_info* root, const void* object) {
  // The depth and the steps count from a zero the verifier cannot see, so
  // that the frames sit at offsets it only bounds.
  const std::uint32_t zero = _LIBCXXABI_OPAQUE_ZERO;
  depth_                   = zero;
  steps_                   = zero;
  notes_                   = walk_notes{};
  state_                   = state::failed;
  has_object_              = object != nullptr;
  if (root == nullptr)
    return;
  kind k                   = kind::none;
  std::uint16_t base_count = 0;
  if (!class_of(root, &k, &base_count))
    return;
  frame& f       = frame_at(zero);
  f.type         = root;
  f.position     = reinterpret_cast<std::uintptr_t>(object);
  f.next_base    = kRootPending;
  f.base_count   = base_count;
  f.kind_        = k;
  f.last_private = 0;
  f.last_virtual = 0;
  f.marked_level = kNoLevel;
  depth_         = zero + 1;
  steps_         = zero + 1;
  state_         = state::walking;
}

search_step hierarchy_search::step() {
  if (depth_ == 0)
    return search_step::ended;
  frame& f = top();
  if (f.next_base == kRootPending) {
    f.next_base = 0;
    return search_step::arrived;
  }
  if (f.next_base < f.base_count) {
    if (!descend()) {
      state_ = state::failed;
      depth_ = 0;
      return search_step::ended;
    }
    return search_step::arrived;
  }
  depth_ -= 1;
  if (depth_ == 0) {
    state_ = state::complete;
    return search_step::ended;
  }
  return search_step::returned;
}

bool hierarchy_search::descend() {
  if (depth_ >= kMaxDepth || steps_ >= kMaxSteps)
    return false;
  frame& parent             = top();
  const std::uint32_t index = parent.next_base;
  parent.next_base += 1;

  const __shim_type_info* base;
  long offset_flags;
  if (parent.kind_ == kind::si_class) {
    base         = static_cast<const __si_class_type_info*>(parent.type)->__base_type;
    offset_flags = __base_class_type_info::__public_mask;
  } else {
    const __base_class_type_info& info = static_cast<const __vmi_class_type_info*>(parent.type)->__base_info[index];
    base                               = info.__base_type;
    offset_flags                       = info.__offset_flags;
  }
  if (base == nullptr)
    return false;
  kind k                   = kind::none;
  std::uint16_t base_count = 0;
  if (!class_of(base, &k, &base_count))
    return false;

  const long offset     = offset_flags >> __base_class_type_info::__offset_shift;
  const bool is_virtual = (offset_flags & __base_class_type_info::__virtual_mask) != 0;
  const bool is_public  = (offset_flags & __base_class_type_info::__public_mask) != 0;
  std::uintptr_t position;
  if (!is_virtual) {
    position = parent.position + static_cast<std::uintptr_t>(offset);
  } else if (has_object_) {
    // The class that names the virtual base is dynamic, so its subobject
    // starts with a vptr, and the record's offset is that of the slot in the
    // vtable holding the base's offset from the subobject.
    const std::intptr_t base_offset =
        virtual_base_offset(vtable_of(reinterpret_cast<const void*>(parent.position)), offset);
    position = parent.position + static_cast<std::uintptr_t>(base_offset);
  } else {
    position = 0;
  }

  const std::uint8_t level = static_cast<std::uint8_t>(depth_);
  frame& child             = frame_at(depth_);
  child.type               = base;
  child.position           = position;
  child.next_base          = 0;
  child.base_count         = base_count;
  child.kind_              = k;
  child.last_private       = is_public ? parent.last_private : level;
  child.last_virtual       = is_virtual ? level : parent.last_virtual;
  child.marked_level       = parent.marked_level;
  depth_ += 1;
  steps_ += 1;
  return true;
}

// Whether base is an unambiguous public base of derived, or derived itself,
// searched from object (null for none). On success *position is the base
// subobject's address, or without an object its offset. False with
// search.failed() set when the walk could not finish.
bool find_public_base(hierarchy_search& search,
                      const __shim_type_info* derived,
                      const void* object,
                      const __shim_type_info* base,
                      std::uintptr_t* position) {
  found_subobjects& bases = search.notes().found[0];
  for (search.start(derived, object); _LIBCXXABI_CAN_LOOP;) {
    const search_step step = search.step();
    if (step == search_step::ended)
      break;
    if (step == search_step::returned || !is_equal(search.type(), base, false))
      continue;
    // A class is never its own base: nothing below is another subobject.
    // Another path to one already found adds its access, the most accessible
    // counting; another subobject makes the base ambiguous.
    search.skip_bases();
    bases.add(search.position(), search.anchor(), search.public_path());
    if (bases.count > 1)
      return false;
  }
  if (!search.complete()) {
    search.fail();
    return false;
  }
  if (bases.count != 1 || !bases.is_public)
    return false;
  *position = bases.position;
  return true;
}

// The search at memory, which nothing constructed, ready for a walk.
hierarchy_search& search_at(unsigned long long memory) {
  hierarchy_search& search = *reinterpret_cast<hierarchy_search*>(memory);
  search.clear();
  return search;
}

// Memory for a search, or the end of the program (see the file comment).
unsigned long long allocate_search() {
  void* memory = __aligned_malloc_with_fallback(sizeof(hierarchy_search));
  if (memory == nullptr)
    std::terminate();
  return reinterpret_cast<unsigned long long>(memory);
}

void free_search(unsigned long long memory) { __aligned_free_with_fallback(reinterpret_cast<void*>(memory)); }

} // namespace

// The walks, over the search's memory as a number.

// 0 when base is not an unambiguous public base of derived, else the base
// subobject's address, or 1 when object is 0. Ends the program when the walk
// cannot finish.
_LIBCXXABI_BPF_GLOBAL_SUBPROGRAM unsigned long long __libcxxabi_bpf_find_public_base(
    unsigned long long derived,
    unsigned long long object,
    unsigned long long base,
    unsigned long long memory) noexcept {
  hierarchy_search& search = search_at(memory);
  std::uintptr_t position  = 0;
  if (!find_public_base(
          search,
          reinterpret_cast<const __shim_type_info*>(derived),
          reinterpret_cast<const void*>(object),
          reinterpret_cast<const __shim_type_info*>(base),
          &position)) {
    if (search.failed())
      std::terminate();
    return 0;
  }
  return object == 0 ? 1 : position;
}

namespace {

// find_public_base, in the global subprogram above, over a search allocated
// for the walk and freed after it (allocate_search).
bool search_public_base(const __shim_type_info* derived,
                        const void* object,
                        const __shim_type_info* base,
                        std::uintptr_t* position) {
  const unsigned long long search = allocate_search();
  const unsigned long long found = __libcxxabi_bpf_find_public_base(
      reinterpret_cast<unsigned long long>(derived),
      reinterpret_cast<unsigned long long>(object),
      reinterpret_cast<unsigned long long>(base),
      search);
  free_search(search);
  if (found == 0)
    return false;
  *position = object == nullptr ? 0 : found;
  return true;
}

//===----------------------------------------------------------------------===//
// can_catch
//===----------------------------------------------------------------------===//

// A handler is a match for an exception object of type E if
//   1. The handler is of type cv T or cv T& and E and T are the same type
//      (ignoring the top-level cv-qualifiers), or
//   2. the handler is of type cv T or cv T& and T is an unambiguous public
//       base class of E, or
//   3. the handler is of type cv1 T* cv2 and E is a pointer type that can be
//      converted to the type of the handler by either or both of
//      A. a standard pointer conversion (4.10) not involving conversions to
//         pointers to private or protected or ambiguous classes
//      B. a qualification conversion
//   4. the handler is a pointer or pointer to member type and E is
//      std::nullptr_t.
//
// adjustedPtr is what the handler gets, as on the other targets:
//   catch (A& a) : adjustedPtr == &a
//   catch (A* a) : adjustedPtr == a
//   catch (D2& d2) : adjustedPtr == &d2  (d2 is base class of thrown object)
//   catch (D2* d2) : adjustedPtr == d2
//   catch (...) : adjustedPtr == & of the exception
// and a std::nullptr_t caught as a pointer to member points to a
// statically-allocated null pointer representation of that kind.

// The ABI's pointer to member function: the function, or a vtable offset plus
// one, and the adjustment of this.
struct member_function_pointer {
  std::uintptr_t function;
  std::ptrdiff_t adjustment;
};

constexpr std::ptrdiff_t null_data_member_pointer              = -1;
constexpr member_function_pointer null_member_function_pointer = {0, 0};

// The deepest chain of pointers and pointers to member compared level by
// level, below the top.
constexpr std::uint32_t kMaxPointerLevels = 64;

// Whether the qualifiers of one level of a thrown pointer convert to the
// handler's: none removed, no function qualifier added, and below the top
// none removed either.
inline bool qualifiers_convert(unsigned handler_flags, unsigned thrown_flags, bool top_level) {
  if (thrown_flags & ~handler_flags & __pbase_type_info::__no_remove_flags_mask)
    return false;
  if (handler_flags & ~thrown_flags & __pbase_type_info::__no_add_flags_mask)
    return false;
  return top_level || (thrown_flags & ~handler_flags & __pbase_type_info::__no_add_flags_mask) == 0;
}

// Whether the incomplete-class flags ask for the names to be compared.
inline bool compares_names(const __pbase_type_info* ti) {
  return (ti->__flags & (__pbase_type_info::__incomplete_class_mask | __pbase_type_info::__incomplete_mask)) != 0;
}

// The levels below the top of a qualification conversion: whether the thrown
// pointee converts to the handler's, both pointers or pointers to member whose
// pointees differ. The handler's top level is const, or the caller would not
// have come down.
bool nested_qualifiers_convert(const __shim_type_info* handler, const __shim_type_info* thrown) {
  for (std::uint32_t level = _LIBCXXABI_OPAQUE_ZERO; level < kMaxPointerLevels && _LIBCXXABI_CAN_LOOP; ++level) {
    const bool pointer = is_a<__pointer_type_info>(handler);
    if (pointer ? !is_a<__pointer_type_info>(thrown)
                : (!is_a<__pointer_to_member_type_info>(handler) || !is_a<__pointer_to_member_type_info>(thrown)))
      return false;
    if (!pointer && !is_equal(static_cast<const __pointer_to_member_type_info*>(handler)->__context,
                              static_cast<const __pointer_to_member_type_info*>(thrown)->__context,
                              false))
      return false;
    const __pbase_type_info* handler_pointer = static_cast<const __pbase_type_info*>(handler);
    const __pbase_type_info* thrown_pointer  = static_cast<const __pbase_type_info*>(thrown);
    if (!qualifiers_convert(handler_pointer->__flags, thrown_pointer->__flags, false))
      return false;
    if (is_equal(handler_pointer->__pointee,
                 thrown_pointer->__pointee,
                 compares_names(handler_pointer) || compares_names(thrown_pointer)))
      return true;
    if ((handler_pointer->__flags & __pbase_type_info::__const_mask) == 0)
      return false;
    handler = handler_pointer->__pointee;
    thrown  = thrown_pointer->__pointee;
  }
  return false;
}

// The value of a thrown pointer, from the exception object holding it.
inline void* pointer_value(void* thrown_object) {
  return thrown_object == nullptr ? nullptr : *static_cast<void* const*>(thrown_object);
}

// Rules 3 and 4 for a pointer handler.
bool catch_pointer(const __shim_type_info* handler,
                   const __shim_type_info* thrown,
                   void* thrown_object,
                   void*& adjustedPtr) {
  if (is_nullptr_t(thrown)) {
    adjustedPtr = nullptr;
    return true;
  }
  if (!is_a<__pointer_type_info>(thrown))
    return false;
  const __pbase_type_info* handler_pointer = static_cast<const __pbase_type_info*>(handler);
  const __pbase_type_info* thrown_pointer  = static_cast<const __pbase_type_info*>(thrown);
  if (!qualifiers_convert(handler_pointer->__flags, thrown_pointer->__flags, true))
    return false;
  void* value                             = pointer_value(thrown_object);
  const __shim_type_info* handler_pointee = handler_pointer->__pointee;
  const __shim_type_info* thrown_pointee  = thrown_pointer->__pointee;
  if (is_equal(handler_pointee, thrown_pointee, compares_names(handler_pointer) || compares_names(thrown_pointer))) {
    adjustedPtr = value;
    return true;
  }
  if (is_void(handler_pointee)) {
    // Any object pointer converts to void*; a function pointer does not.
    if (is_a<__function_type_info>(thrown_pointee))
      return false;
    adjustedPtr = value;
    return true;
  }
  if (is_class(handler_pointee) && is_class(thrown_pointee)) {
    std::uintptr_t position = 0;
    if (!search_public_base(thrown_pointee, value, handler_pointee, &position))
      return false;
    // A null pointer converts to a null pointer, whatever the offset.
    adjustedPtr = value == nullptr ? nullptr : reinterpret_cast<void*>(position);
    return true;
  }
  if ((handler_pointer->__flags & __pbase_type_info::__const_mask) == 0 ||
      !nested_qualifiers_convert(handler_pointee, thrown_pointee))
    return false;
  adjustedPtr = value;
  return true;
}

// Rules 3 and 4 for a pointer-to-member handler.
bool catch_member_pointer(
    const __shim_type_info* handler, const __shim_type_info* thrown, void* thrown_object, void*& adjustedPtr) {
  const __pointer_to_member_type_info* handler_member = static_cast<const __pointer_to_member_type_info*>(handler);
  if (is_nullptr_t(thrown)) {
    const void* null =
        is_a<__function_type_info>(handler_member->__pointee)
            ? static_cast<const void*>(&null_member_function_pointer)
            : static_cast<const void*>(&null_data_member_pointer);
    adjustedPtr = const_cast<void*>(null);
    return true;
  }
  if (!is_a<__pointer_to_member_type_info>(thrown))
    return false;
  const __pointer_to_member_type_info* thrown_member = static_cast<const __pointer_to_member_type_info*>(thrown);
  // [except.handle] does not allow the pointer-to-member conversions of
  // [conv.mem]: the member must be of the same class.
  if (!is_equal(handler_member->__context, thrown_member->__context, false))
    return false;
  if (!qualifiers_convert(handler_member->__flags, thrown_member->__flags, true))
    return false;
  if (!is_equal(handler_member->__pointee,
                thrown_member->__pointee,
                compares_names(handler_member) || compares_names(thrown_member)) &&
      ((handler_member->__flags & __pbase_type_info::__const_mask) == 0 ||
       !nested_qualifiers_convert(handler_member->__pointee, thrown_member->__pointee)))
    return false;
  // Qualification and function pointer conversions keep the representation.
  adjustedPtr = thrown_object;
  return true;
}

} // namespace

bool __shim_type_info::can_catch(const __shim_type_info* thrown_type, void*& adjustedPtr) const noexcept {
  if (thrown_type == nullptr)
    return false;
  // Rule 1. A type_info has no top-level qualifiers to ignore.
  if (is_equal(this, thrown_type, false)) {
    if (is_a<__pointer_type_info>(thrown_type))
      adjustedPtr = pointer_value(adjustedPtr);
    return true;
  }
  if (is_a<__pointer_to_member_type_info>(this))
    return catch_member_pointer(this, thrown_type, adjustedPtr, adjustedPtr);
  const bool handler_is_pointer = is_a<__pointer_type_info>(this);
  if (!handler_is_pointer && !is_class(this))
    // Fundamental, enumeration, array and function types match only
    // themselves.
    return false;
  // Rules 2 and 3 may walk a class graph, in a global subprogram over a
  // search allocated for the walk alone (see the file comment): a clause
  // that the kinds or the qualifiers settle costs no memory, which matters
  // when a throw is served by the fallback heap.
  if (handler_is_pointer)
    return catch_pointer(this, thrown_type, adjustedPtr, adjustedPtr);
  std::uintptr_t position = 0;
  if (!is_class(thrown_type) || !search_public_base(thrown_type, adjustedPtr, this, &position))
    return false;
  adjustedPtr = adjustedPtr == nullptr ? nullptr : reinterpret_cast<void*>(position);
  return true;
}

//===----------------------------------------------------------------------===//
// __dynamic_cast
//===----------------------------------------------------------------------===//

// dynamic_cast<C*>(v), where v points to a subobject of static type S of a
// polymorphic object and C is not a base of S, reads the most derived object's
// address and type from v's vtable and then ([expr.dynamic.cast]/9):
//
//   9.1  if v is a public base subobject of a C object, and only one C object
//        is derived from v's subobject, the result is that C;
//   9.2  otherwise, if v is a public base subobject of the most derived
//        object, and C is an unambiguous public base of it, the result is that
//        C subobject;
//   9.3  otherwise the cast fails and the result is null.
//
// The whole answer comes from one walk of the most derived object's
// subobjects, which marks every C subobject it arrives at, so that arriving at
// v's subobject tells which C, if any, lies above it on the path and whether
// the path from it is public; it also counts the distinct C subobjects and
// whether each path to v and to C is public. When the most derived type is C
// itself, the walk only looks for a public path to v.
//
// The hint is the ABI's src2dst_offset: -1 for none, -2 when S is not a public
// base of C, -3 when S is a public base of C by several paths, never through a
// virtual base, else the offset of S in C when S is a unique public
// non-virtual base of C. It decides a cast to the most derived type without a
// walk; every other case walks.

namespace {

constexpr std::ptrdiff_t kNoHint                   = -1;
constexpr std::ptrdiff_t kSourceNotPublicBase      = -2;
constexpr std::ptrdiff_t kSourceMultiplePublicBase = -3;

// The cast when the most derived type is the target: 9.1 with the whole
// object as the only C, which needs v public in it, as 9.2 would.
void* cast_to_complete(
    hierarchy_search& search,
    const __shim_type_info* whole,
    std::uintptr_t whole_address,
    const __shim_type_info* source,
    std::uintptr_t source_address,
    std::ptrdiff_t hint) {
  void* result = reinterpret_cast<void*>(whole_address);
  if (hint >= 0) {
    // S is a unique public non-virtual base of C at that offset; any other S
    // subobject of C is not public.
    return whole_address + static_cast<std::uintptr_t>(hint) == source_address ? result : nullptr;
  }
  if (hint == kSourceNotPublicBase)
    return nullptr;
  (void)kNoHint;
  (void)kSourceMultiplePublicBase;
  for (search.start(whole, result); _LIBCXXABI_CAN_LOOP;) {
    const search_step step = search.step();
    if (step == search_step::ended)
      break;
    if (step == search_step::returned || search.position() != source_address || !is_equal(search.type(), source, false))
      continue;
    if (search.public_path())
      return result;
    search.skip_bases();
  }
  if (!search.complete())
    search.fail();
  return nullptr;
}

// The cast when the target is a proper base of the most derived type.
void* cast_within(hierarchy_search& search,
                  const __shim_type_info* whole,
                  std::uintptr_t whole_address,
                  const __shim_type_info* source,
                  std::uintptr_t source_address,
                  const __shim_type_info* target) {
  // In the search's notes, zeroed at the start: the C subobjects of the whole
  // object, and whether the path from the whole object to the first is
  // public; the C subobjects v's subobject is a base of, and whether the path
  // from the first down to v is public; whether v is a public base subobject
  // of the whole object.
  walk_notes& notes            = search.notes();
  found_subobjects& targets    = notes.found[0];
  found_subobjects& leading    = notes.found[1];
  std::uint64_t& source_public = notes.flag;
  for (search.start(whole, reinterpret_cast<const void*>(whole_address)); _LIBCXXABI_CAN_LOOP;) {
    const search_step step = search.step();
    if (step == search_step::ended)
      break;
    if (step == search_step::returned)
      continue;
    const std::uintptr_t position = search.position();
    if (is_equal(search.type(), target, false)) {
      search.mark();
      targets.add(position, nullptr, search.public_path());
    }
    if (position != source_address || !is_equal(search.type(), source, false))
      continue;
    source_public             = source_public || search.public_path();
    const std::uint32_t level = search.marked_level();
    if (level != hierarchy_search::kNoLevel) {
      leading.add(search.position_at(level), nullptr, search.public_from(level));
      if (leading.count > 1)
        // Two C objects are derived from v, so C is ambiguous too.
        return nullptr;
    }
  }
  if (!search.complete()) {
    search.fail();
    return nullptr;
  }
  if (leading.count == 1 && leading.is_public)
    return reinterpret_cast<void*>(leading.position); // 9.1
  if (source_public && targets.count == 1 && targets.is_public)
    return reinterpret_cast<void*>(targets.position); // 9.2
  return nullptr;
}

// The cast where the search is. Null with search.failed() set when the walk
// could not finish.
void* dynamic_cast_walk(hierarchy_search& search,
                        const void* static_ptr,
                        const __shim_type_info* static_type,
                        const __shim_type_info* dst_type,
                        std::ptrdiff_t src2dst_offset) {
  if (static_ptr == nullptr || static_type == nullptr || dst_type == nullptr)
    return nullptr;
  const std::intptr_t* vtable   = vtable_of(static_ptr);
  const __shim_type_info* whole = most_derived_type(vtable);
  if (whole == nullptr || !is_class(whole))
    return nullptr;
  const std::uintptr_t source_address = reinterpret_cast<std::uintptr_t>(static_ptr);
  const std::uintptr_t whole_address  = source_address + static_cast<std::uintptr_t>(offset_to_top(vtable));
  if (is_equal(whole, dst_type, false))
    return cast_to_complete(search, whole, whole_address, static_type, source_address, src2dst_offset);
  return cast_within(search, whole, whole_address, static_type, source_address, dst_type);
}

} // namespace

// The walk, over the search's memory as a number. Ends the program when the
// walk cannot finish.
_LIBCXXABI_BPF_GLOBAL_SUBPROGRAM unsigned long long __libcxxabi_bpf_dynamic_cast_search(
    unsigned long long static_ptr,
    unsigned long long static_type,
    unsigned long long dst_type,
    long long src2dst_offset,
    unsigned long long memory) noexcept {
  hierarchy_search& search = search_at(memory);
  void* result             = dynamic_cast_walk(
      search,
      reinterpret_cast<const void*>(static_ptr),
      reinterpret_cast<const __shim_type_info*>(static_type),
      reinterpret_cast<const __shim_type_info*>(dst_type),
      src2dst_offset);
  if (result == nullptr && search.failed())
    std::terminate();
  return reinterpret_cast<unsigned long long>(result);
}

// The run-time check, checked once per program: it allocates the search and
// frees it after the walk above.
_LIBCXXABI_BPF_GLOBAL_SUBPROGRAM unsigned long long __libcxxabi_bpf_dynamic_cast(
    unsigned long long static_ptr,
    unsigned long long static_type,
    unsigned long long dst_type,
    long long src2dst_offset) noexcept {
  const unsigned long long search = allocate_search();
  const unsigned long long result =
      __libcxxabi_bpf_dynamic_cast_search(static_ptr, static_type, dst_type, src2dst_offset, search);
  free_search(search);
  return result;
}

extern "C" _LIBCXXABI_FUNC_VIS const void*
__dynamic_cast(const void* static_ptr,
               const __class_type_info* static_type,
               const __class_type_info* dst_type,
               std::ptrdiff_t src2dst_offset) {
  return reinterpret_cast<const void*>(__libcxxabi_bpf_dynamic_cast(
      reinterpret_cast<unsigned long long>(static_ptr),
      reinterpret_cast<unsigned long long>(static_type),
      reinterpret_cast<unsigned long long>(dst_type),
      src2dst_offset));
}

} // namespace __cxxabiv1
