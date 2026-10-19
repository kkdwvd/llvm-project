// RUN: %clang_cc1 -triple bpfel -emit-llvm -debug-info-kind=constructor \
// RUN:   -fcxx-exceptions -fexceptions %s -o - | FileCheck %s
// RUN: %clang_cc1 -triple bpfel -emit-llvm -debug-info-kind=constructor \
// RUN:   -fcxx-exceptions -fexceptions -DSAME_TU %s -o - \
// RUN:   | FileCheck --check-prefix=SAME %s
// RUN: %clang_cc1 -triple bpfel -emit-llvm -debug-info-kind=constructor \
// RUN:   -fcxx-exceptions -fexceptions -DNO_DECLS %s -o - \
// RUN:   | FileCheck --check-prefix=NODECL %s
// RUN: %clang_cc1 -triple x86_64-linux-gnu -emit-llvm \
// RUN:   -debug-info-kind=constructor -fcxx-exceptions -fexceptions %s -o - \
// RUN:   | FileCheck --check-prefix=NODECL %s

// BPF links a call to a function in another object through BTF, which comes
// from debug info. CodeGen calls the C++ runtime on its own, with nothing in
// the source that refers to it: the runtime functions get the debug info of
// the declarations the translation unit has of them, the ones the runtime is
// built with, so that the two agree.

// CHECK-DAG: declare !dbg ![[ALLOC:[0-9]+]] ptr @__cxa_allocate_exception(i64)
// CHECK-DAG: declare !dbg ![[FREE:[0-9]+]] void @__cxa_free_exception(ptr)
// CHECK-DAG: declare !dbg ![[THROW:[0-9]+]] void @__cxa_throw(ptr, ptr, ptr)
// CHECK-DAG: declare !dbg ![[RETHROW:[0-9]+]] void @__cxa_rethrow()
// CHECK-DAG: declare !dbg ![[BEGIN:[0-9]+]] ptr @__cxa_begin_catch(ptr)
// CHECK-DAG: declare !dbg ![[END:[0-9]+]] void @__cxa_end_catch()
// CHECK-DAG: declare !dbg ![[GET:[0-9]+]] ptr @__cxa_get_exception_ptr(ptr)
// CHECK-DAG: declare !dbg ![[TERMINATE:[0-9]+]] void @_ZSt9terminatev()
// CHECK-DAG: declare !dbg ![[DYNCAST:[0-9]+]] ptr @__dynamic_cast(ptr, ptr, ptr, i64)
// CHECK-DAG: declare !dbg ![[BADCAST:[0-9]+]] void @__cxa_bad_cast()
// CHECK-DAG: declare !dbg ![[BADTYPEID:[0-9]+]] void @__cxa_bad_typeid()
// CHECK-DAG: declare !dbg ![[PURE:[0-9]+]] void @__cxa_pure_virtual()
// CHECK-DAG: declare !dbg ![[DELETED:[0-9]+]] void @__cxa_deleted_virtual()

// CHECK-DAG: ![[ALLOC]] = !DISubprogram(name: "__cxa_allocate_exception",
// CHECK-DAG: ![[FREE]] = !DISubprogram(name: "__cxa_free_exception",
// CHECK-DAG: ![[RETHROW]] = !DISubprogram(name: "__cxa_rethrow",
// CHECK-DAG: ![[BEGIN]] = !DISubprogram(name: "__cxa_begin_catch",
// CHECK-DAG: ![[END]] = !DISubprogram(name: "__cxa_end_catch",
// CHECK-DAG: ![[GET]] = !DISubprogram(name: "__cxa_get_exception_ptr",
// CHECK-DAG: ![[TERMINATE]] = !DISubprogram(name: "terminate", linkageName: "_ZSt9terminatev",
// CHECK-DAG: ![[DYNCAST]] = !DISubprogram(name: "__dynamic_cast",
// CHECK-DAG: ![[BADCAST]] = !DISubprogram(name: "__cxa_bad_cast",
// CHECK-DAG: ![[BADTYPEID]] = !DISubprogram(name: "__cxa_bad_typeid",
// CHECK-DAG: ![[PURE]] = !DISubprogram(name: "__cxa_pure_virtual",
// CHECK-DAG: ![[DELETED]] = !DISubprogram(name: "__cxa_deleted_virtual",

// The declaration's types, std::type_info as declared:
// CHECK-DAG: ![[THROW]] = !DISubprogram(name: "__cxa_throw", {{.*}}type: ![[THROW_TY:[0-9]+]]
// CHECK-DAG: ![[THROW_TY]] = !DISubroutineType(types: ![[THROW_TYS:[0-9]+]])
// CHECK-DAG: ![[THROW_TYS]] = !{null, !{{[0-9]+}}, ![[TI_PTR:[0-9]+]], !{{[0-9]+}}}
// CHECK-DAG: ![[TI_PTR]] = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: ![[TI:[0-9]+]]
// CHECK-DAG: ![[TI]] = !DICompositeType(tag: DW_TAG_class_type, name: "type_info"

// A runtime function the translation unit defines has the debug info of its
// definition, and only that, whether it comes before or after the calls.
// SAME: define {{.*}}ptr @__cxa_begin_catch(ptr {{.*}}) {{.*}}!dbg ![[DEF:[0-9]+]]
// SAME-NOT: declare {{.*}}@__cxa_begin_catch
// SAME: ![[DEF]] = distinct !DISubprogram(name: "__cxa_begin_catch", {{.*}}spFlags: DISPFlagDefinition
// SAME-NOT: !DISubprogram(name: "__cxa_begin_catch"

// Without a declaration in the translation unit, or for a target that does
// not ask for debug info of external references, there is none.
// NODECL: declare ptr @__cxa_begin_catch(ptr)

#ifndef NO_DECLS
namespace std {
class type_info;
[[noreturn]] void terminate() noexcept;
}

namespace __cxxabiv1 {
class __class_type_info;

extern "C" {
void *__cxa_allocate_exception(unsigned long thrown_size) noexcept;
void __cxa_free_exception(void *thrown_exception) noexcept;
[[noreturn]] void __cxa_throw(void *thrown_exception, std::type_info *tinfo,
                              void (*dest)(void *));
[[noreturn]] void __cxa_rethrow();
void *__cxa_begin_catch(void *exception_object) noexcept;
void __cxa_end_catch();
void *__cxa_get_exception_ptr(void *exception_object) noexcept;
void *__dynamic_cast(const void *sub, const __class_type_info *src,
                     const __class_type_info *dst, long src2dst);
[[noreturn]] void __cxa_bad_cast();
[[noreturn]] void __cxa_bad_typeid();
[[noreturn]] void __cxa_pure_virtual();
[[noreturn]] void __cxa_deleted_virtual();
}
} // namespace __cxxabiv1
#endif

namespace std {
class type_info {
public:
  virtual ~type_info();
  const char *name;
};
}

struct Copy {
  Copy();
  Copy(const Copy &);
  int v;
};

struct Base {
  virtual int f() = 0;
  virtual int g() = delete;
};

struct Derived : Base {
  int f() override;
};

void may_throw(int v);

int catches(int v)
{
  try {
    may_throw(v);
  } catch (Copy c) {
    return c.v;
  } catch (int) {
    throw;
  }
  return 0;
}

void throws(int v)
{
  throw v;
}

void throws_copy()
{
  throw Copy();
}

void cannot_throw(int v) noexcept
{
  may_throw(v);
}

int casts(Base &b, Base *p)
{
  return dynamic_cast<Derived &>(b).f() + (typeid(*p).name != nullptr);
}

int Derived::f()
{
  return 1;
}

int constructs()
{
  Derived d;

  return d.f();
}

#ifdef SAME_TU
extern "C" void *__cxa_begin_catch(void *exception_object) noexcept
{
  return exception_object;
}
#endif
