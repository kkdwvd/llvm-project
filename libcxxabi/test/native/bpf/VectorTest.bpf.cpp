//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// libc++abi's test_vector1 (test_empty, test_counted,
// test_exception_in_constructor, test_exception_in_destructor) and
// cxa_vec_new_overflow_PR41395, as BPF programs: the array construction
// and destruction API, with constructors, destructors and allocators the
// test passes as function pointers, which the kernel's call through a
// number reaches. test_vector2 and test_vector3 are the programs that must
// end through std::terminate(): the runner reports them as ended early.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cxxabi.h>
#include <exception>
#include <new>

#include <std/bpf/testing/BpfTest.h>

namespace vec {
//  Wrapper routines
static void* my_alloc2(size_t sz) { return std::malloc(sz); }
static void my_dealloc2(void* p) { std::free(p); }
static void my_dealloc3(void* p, size_t) { std::free(p); }
static void my_construct(void*) {}
static void my_destruct(void*) {}

static int gCounter;
static void count_construct(void*) { ++gCounter; }
static void count_destruct(void*) { --gCounter; }

static int gConstructorCounter;
static int gConstructorThrowTarget;
static int gDestructorCounter;
static int gDestructorThrowTarget;
static void throw_construct(void*) {
  if (gConstructorCounter == gConstructorThrowTarget)
    throw 1;
  ++gConstructorCounter;
}
static void throw_destruct(void*) {
  if (++gDestructorCounter == gDestructorThrowTarget)
    throw 2;
}

struct vec_on_stack {
  void* storage;
  vec_on_stack() : storage(__cxxabiv1::__cxa_vec_new(10, 40, 8, throw_construct, throw_destruct)) {}
  ~vec_on_stack() noexcept(false) { __cxxabiv1::__cxa_vec_delete(storage, 40, 8, throw_destruct); }
};
} // namespace vec

BPF_TEST(Vector, Empty) {
  using namespace vec;
  void *one, *two, *three;

  //  Try with no padding and no con/destructors
  one = __cxxabiv1::__cxa_vec_new(10, 40, 0, NULL, NULL);
  two = __cxxabiv1::__cxa_vec_new2(10, 40, 0, NULL, NULL, my_alloc2, my_dealloc2);
  three = __cxxabiv1::__cxa_vec_new3(10, 40, 0, NULL, NULL, my_alloc2, my_dealloc3);
  EXPECT_NE(one, nullptr);
  EXPECT_NE(two, nullptr);
  EXPECT_NE(three, nullptr);
  __cxxabiv1::__cxa_vec_delete(one, 40, 0, NULL);
  __cxxabiv1::__cxa_vec_delete2(two, 40, 0, NULL, my_dealloc2);
  __cxxabiv1::__cxa_vec_delete3(three, 40, 0, NULL, my_dealloc3);

  //  Try with no padding
  one = __cxxabiv1::__cxa_vec_new(10, 40, 0, my_construct, my_destruct);
  two = __cxxabiv1::__cxa_vec_new2(10, 40, 0, my_construct, my_destruct, my_alloc2, my_dealloc2);
  three = __cxxabiv1::__cxa_vec_new3(10, 40, 0, my_construct, my_destruct, my_alloc2, my_dealloc3);
  __cxxabiv1::__cxa_vec_delete(one, 40, 0, my_destruct);
  __cxxabiv1::__cxa_vec_delete2(two, 40, 0, my_destruct, my_dealloc2);
  __cxxabiv1::__cxa_vec_delete3(three, 40, 0, my_destruct, my_dealloc3);

  //  Padding and no con/destructors
  one = __cxxabiv1::__cxa_vec_new(10, 40, 8, NULL, NULL);
  two = __cxxabiv1::__cxa_vec_new2(10, 40, 8, NULL, NULL, my_alloc2, my_dealloc2);
  three = __cxxabiv1::__cxa_vec_new3(10, 40, 8, NULL, NULL, my_alloc2, my_dealloc3);
  __cxxabiv1::__cxa_vec_delete(one, 40, 8, NULL);
  __cxxabiv1::__cxa_vec_delete2(two, 40, 8, NULL, my_dealloc2);
  __cxxabiv1::__cxa_vec_delete3(three, 40, 8, NULL, my_dealloc3);

  //  Padding with con/destructors
  one = __cxxabiv1::__cxa_vec_new(10, 40, 8, my_construct, my_destruct);
  two = __cxxabiv1::__cxa_vec_new2(10, 40, 8, my_construct, my_destruct, my_alloc2, my_dealloc2);
  three = __cxxabiv1::__cxa_vec_new3(10, 40, 8, my_construct, my_destruct, my_alloc2, my_dealloc3);
  __cxxabiv1::__cxa_vec_delete(one, 40, 8, my_destruct);
  __cxxabiv1::__cxa_vec_delete2(two, 40, 8, my_destruct, my_dealloc2);
  __cxxabiv1::__cxa_vec_delete3(three, 40, 8, my_destruct, my_dealloc3);
}

BPF_TEST(Vector, Counted) {
  using namespace vec;
  void *one, *two, *three;

  //  Try with no padding
  gCounter = 0;
  one = __cxxabiv1::__cxa_vec_new(10, 40, 0, count_construct, count_destruct);
  two = __cxxabiv1::__cxa_vec_new2(10, 40, 0, count_construct, count_destruct, my_alloc2, my_dealloc2);
  three = __cxxabiv1::__cxa_vec_new3(10, 40, 0, count_construct, count_destruct, my_alloc2, my_dealloc3);
  __cxxabiv1::__cxa_vec_delete(one, 40, 0, count_destruct);
  __cxxabiv1::__cxa_vec_delete2(two, 40, 0, count_destruct, my_dealloc2);
  __cxxabiv1::__cxa_vec_delete3(three, 40, 0, count_destruct, my_dealloc3);
  //  Since there was no padding, the # of elements in the array are not
  //  stored and the destructors are not called.
  EXPECT_EQ(gCounter, 30);

  gCounter = 0;
  one = __cxxabiv1::__cxa_vec_new(10, 40, 8, count_construct, count_destruct);
  two = __cxxabiv1::__cxa_vec_new2(10, 40, 8, count_construct, count_destruct, my_alloc2, my_dealloc2);
  three = __cxxabiv1::__cxa_vec_new3(10, 40, 8, count_construct, count_destruct, my_alloc2, my_dealloc3);
  __cxxabiv1::__cxa_vec_delete(one, 40, 8, count_destruct);
  __cxxabiv1::__cxa_vec_delete2(two, 40, 8, count_destruct, my_dealloc2);
  __cxxabiv1::__cxa_vec_delete3(three, 40, 8, count_destruct, my_dealloc3);
  EXPECT_EQ(gCounter, 0);
}

BPF_TEST(Vector, ExceptionInConstructor) {
  using namespace vec;
  void *one, *two, *three;

  //  Try with no padding
  gConstructorCounter = gDestructorCounter = 0;
  gConstructorThrowTarget = 15;
  gDestructorThrowTarget = -1;
  try {
    one = two = three = NULL;
    one = __cxxabiv1::__cxa_vec_new(10, 40, 0, throw_construct, throw_destruct);
    two = __cxxabiv1::__cxa_vec_new2(10, 40, 0, throw_construct, throw_destruct, my_alloc2, my_dealloc2);
    three = __cxxabiv1::__cxa_vec_new3(10, 40, 0, throw_construct, throw_destruct, my_alloc2, my_dealloc3);
  } catch (int i) {
  }
  __cxxabiv1::__cxa_vec_delete(one, 40, 0, throw_destruct);
  __cxxabiv1::__cxa_vec_delete2(two, 40, 0, throw_destruct, my_dealloc2);
  __cxxabiv1::__cxa_vec_delete3(three, 40, 0, throw_destruct, my_dealloc3);
  //  Since there was no padding, the # of elements in the array are not
  //  stored and the destructors are not called. Since we threw after 15
  //  calls to the constructor, we should see 5 calls to the destructor from
  //  the partially constructed array.
  EXPECT_EQ(gConstructorCounter - gDestructorCounter, 10);

  gConstructorCounter = gDestructorCounter = 0;
  gConstructorThrowTarget = 15;
  gDestructorThrowTarget = -1;
  try {
    one = two = three = NULL;
    one = __cxxabiv1::__cxa_vec_new(10, 40, 8, throw_construct, throw_destruct);
    two = __cxxabiv1::__cxa_vec_new2(10, 40, 8, throw_construct, throw_destruct, my_alloc2, my_dealloc2);
    three = __cxxabiv1::__cxa_vec_new3(10, 40, 8, throw_construct, throw_destruct, my_alloc2, my_dealloc3);
  } catch (int i) {
  }
  __cxxabiv1::__cxa_vec_delete(one, 40, 8, throw_destruct);
  __cxxabiv1::__cxa_vec_delete2(two, 40, 8, throw_destruct, my_dealloc2);
  __cxxabiv1::__cxa_vec_delete3(three, 40, 8, throw_destruct, my_dealloc3);
  EXPECT_EQ(gConstructorCounter, gDestructorCounter);
}

BPF_TEST(Vector, ExceptionInDestructor) {
  using namespace vec;
  void *one, *two;

  //  Throw from within a destructor
  gConstructorCounter = gDestructorCounter = 0;
  gConstructorThrowTarget = -1;
  gDestructorThrowTarget = 15;
  try {
    one = two = NULL;
    one = __cxxabiv1::__cxa_vec_new(10, 40, 8, throw_construct, throw_destruct);
    two = __cxxabiv1::__cxa_vec_new2(10, 40, 8, throw_construct, throw_destruct, my_alloc2, my_dealloc2);
  } catch (int i) {
  }
  int got = 0;
  try {
    __cxxabiv1::__cxa_vec_delete(one, 40, 8, throw_destruct);
    __cxxabiv1::__cxa_vec_delete2(two, 40, 8, throw_destruct, my_dealloc2);
    got = 1;
  } catch (int i) {
    got = i;
  }
  //  We should have thrown in the middle of cleaning up "two", which means
  //  that there should be 20 calls to the destructor and the try block
  //  should exit before the end.
  EXPECT_EQ(got, 2);
  EXPECT_EQ(gConstructorCounter, 20);
  EXPECT_EQ(gDestructorCounter, 20);

  //  Try throwing from a destructor - should be fine.
  gConstructorCounter = gDestructorCounter = 0;
  gConstructorThrowTarget = -1;
  gDestructorThrowTarget = 5;
  got = 0;
  try {
    vec_on_stack v;
  } catch (int i) {
    got = i;
  }
  EXPECT_EQ(got, 2);
  EXPECT_EQ(gConstructorCounter, gDestructorCounter);
}

namespace overflow {
static void dummy_ctor(void*) {}
static void dummy_dtor(void*) {}
static void* dummy_alloc(size_t) { return nullptr; }
static void dummy_dealloc(void*) {}
static void dummy_dealloc_sized(void*, size_t) {}

static bool check_mul_overflows(size_t x, size_t y) {
  size_t tmp = x * y;
  return tmp / x != y;
}

static bool check_add_overflows(size_t x, size_t y) {
  size_t tmp = x + y;
  return tmp < x;
}

template <class F>
static int catches_bad_array_new_length(F f) {
  try {
    f();
    return 1;
  } catch (std::bad_array_new_length const&) {
    return 2;
  } catch (...) {
    return 3;
  }
}
} // namespace overflow

BPF_TEST(Vector, NewOverflowInMultiplication) {
  using namespace overflow;
  const size_t elem_count = std::size_t(1) << (sizeof(std::size_t) * 8 - 2);
  const size_t elem_size = 8;
  const size_t padding = 0;
  EXPECT_TRUE(check_mul_overflows(elem_count, elem_size));
  EXPECT_EQ(catches_bad_array_new_length([&] {
              __cxxabiv1::__cxa_vec_new(elem_count, elem_size, padding, dummy_ctor, dummy_dtor);
            }),
            2);
  EXPECT_EQ(catches_bad_array_new_length([&] {
              __cxxabiv1::__cxa_vec_new2(elem_count, elem_size, padding, dummy_ctor, dummy_dtor, &dummy_alloc,
                                         &dummy_dealloc);
            }),
            2);
  EXPECT_EQ(catches_bad_array_new_length([&] {
              __cxxabiv1::__cxa_vec_new3(elem_count, elem_size, padding, dummy_ctor, dummy_dtor, &dummy_alloc,
                                         &dummy_dealloc_sized);
            }),
            2);
}

BPF_TEST(Vector, NewOverflowInAddition) {
  using namespace overflow;
  const size_t elem_size = 4;
  const size_t elem_count = static_cast<size_t>(-1) / 4u;
  const size_t padding = sizeof(std::size_t);
  EXPECT_TRUE(!check_mul_overflows(elem_count, elem_size));
  EXPECT_TRUE(check_add_overflows(elem_count * elem_size, padding));
  EXPECT_EQ(catches_bad_array_new_length([&] {
              __cxxabiv1::__cxa_vec_new(elem_count, elem_size, padding, dummy_ctor, dummy_dtor);
            }),
            2);
  EXPECT_EQ(catches_bad_array_new_length([&] {
              __cxxabiv1::__cxa_vec_new2(elem_count, elem_size, padding, dummy_ctor, dummy_dtor, &dummy_alloc,
                                         &dummy_dealloc);
            }),
            2);
  EXPECT_EQ(catches_bad_array_new_length([&] {
              __cxxabiv1::__cxa_vec_new3(elem_count, elem_size, padding, dummy_ctor, dummy_dtor, &dummy_alloc,
                                         &dummy_dealloc_sized);
            }),
            2);
}

// test_vector2: a destructor that throws while unwinding ends the program
// through std::terminate(). The runner reports the test as ended early,
// through bpf_throw()'s 0, which is the expected outcome.
BPF_TEST(VectorTerminates, ThrowWhileUnwinding) {
  using namespace vec;
  gConstructorCounter = gDestructorCounter = 0;
  gConstructorThrowTarget = -1;
  gDestructorThrowTarget = 5;
  try {
    vec_on_stack v;
    throw 3;
  } catch (int i) {
  }
  EXPECT_TRUE(false); // should never get here
}

namespace vec3 {
static int ran;
struct B {
  ~B() { ran = 10; }
};
struct A {
  ~A() noexcept(false) {
    B b;
    throw 0;
  }
};
template <class T>
static void destroy(void* v) {
  T* t = static_cast<T*>(v);
  t->~T();
}
} // namespace vec3

// test_vector3: a destructor that throws during __cxa_vec_dtor, with no
// handler, ends the program through std::terminate(): ended early.
BPF_TEST(VectorTerminates, ThrowFromVecDtor) {
  using namespace vec3;
  typedef A Array[10];
  Array* a = new Array[10];
  __cxxabiv1::__cxa_vec_dtor(a, 10, sizeof(A), destroy<A>);
  EXPECT_TRUE(false);
}
