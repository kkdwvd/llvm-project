; RUN: llc -mtriple=bpfel -mcpu=v4 -stop-after=bpf-unwind-prepare -o - %s | FileCheck --check-prefix=IR %s
; RUN: llc -mtriple=bpfel -mcpu=v4 -verify-machineinstrs -o - %s | FileCheck %s
; RUN: llc -mtriple=bpfel -mcpu=v4 -enable-new-pm -verify-machineinstrs -o - %s | FileCheck %s
; RUN: llc -mtriple=bpfel -mcpu=v4 -filetype=obj -o %t %s
; RUN: llvm-readelf -S -r %t | FileCheck --check-prefix=OBJ %s

; Catch clauses for a type. The kernel unwinds and runs no personality routine
; between the frames: every pad is entered, and one with a catch clause for a
; type calls the personality routine itself, as a Wasm catch does. It stores
; its index and the address of the function's exception table into the
; landing pad context of libunwind, calls the personality with the exception
; object, and reads the selector back; clang's dispatch blocks compare the
; selector with llvm.eh.typeid.for() as on any other target. The table is the
; standard LSDA with a call-site table keyed by landing pad index, as Wasm's,
; in .data.rel.ro beside the type_info objects it points to. The IR is
; clang's, from
;
;   clang --target=bpf -mcpu=v4 -O2 -fexceptions -funwind-tables -fno-rtti
;
; of the C++ below, without metadata and with fewer attributes.
;   extern "C" void may_throw(int);
;   struct Base { int x; };
;   struct Guard { ~Guard(); };
;
;   extern "C" int one_typed(int v)
;   {
;           try {
;                   may_throw(v);
;           } catch (int e) {
;                   return e;
;           }
;           return 0;
;   }
;
;   extern "C" int two_typed(int v)
;   {
;           try {
;                   may_throw(v);
;           } catch (int e) {
;                   return e;
;           } catch (Base &b) {
;                   return b.x;
;           } catch (...) {
;                   return -1;
;           }
;           return 0;
;   }
;
;   extern "C" int nested(int v)
;   {
;           try {
;                   try {
;                           may_throw(v);
;                   } catch (int e) {
;                           may_throw(e);
;                           return e;
;                   }
;           } catch (long l) {
;                   return (int)l;
;           }
;           return 0;
;   }
;
;   static __attribute__((noinline)) int with_cleanup(int v)
;   {
;           Guard g;
;
;           if (v > 100)
;                   throw v;
;           return v;
;   }
;
;   extern "C" int caller(int v)
;   {
;           try {
;                   return with_cleanup(v);
;           } catch (int e) {
;                   return e;
;           }
;   }
;
;   extern "C" __attribute__((noinline)) int rethrows(int v)
;   {
;           try {
;                   may_throw(v);
;           } catch (int) {
;                   throw;
;           }
;           return 0;
;   }
;
;   extern "C" int catches_rethrown(int v)
;   {
;           try {
;                   return rethrows(v);
;           } catch (int e) {
;                   return e;
;           }
;   }


%struct.Guard = type { i8 }

$_ZTI4Base = comdat any

$_ZTS4Base = comdat any

@_ZTIi = external constant ptr
@_ZTI4Base = linkonce_odr constant { ptr, ptr } { ptr getelementptr inbounds (i8, ptr @_ZTVN10__cxxabiv117__class_type_infoE, i64 16), ptr @_ZTS4Base }, comdat, align 8
@_ZTVN10__cxxabiv117__class_type_infoE = external global [0 x ptr]
@_ZTS4Base = linkonce_odr constant [6 x i8] c"4Base\00", comdat, align 1
@_ZTIl = external constant ptr

define i32 @one_typed(i32 %v) uwtable personality ptr @__gxx_bpf_personality_v0 {
entry:
  invoke void @may_throw(i32 %v)
          to label %return unwind label %lpad

lpad:
  %0 = landingpad { ptr, i32 }
          catch ptr @_ZTIi
  %1 = extractvalue { ptr, i32 } %0, 1
  %2 = tail call i32 @llvm.eh.typeid.for.p0(ptr nonnull @_ZTIi) nounwind
  %matches = icmp eq i32 %1, %2
  br i1 %matches, label %catch, label %eh.resume

catch:
  %3 = extractvalue { ptr, i32 } %0, 0
  %4 = tail call ptr @__cxa_begin_catch(ptr %3) nounwind
  %5 = load i32, ptr %4, align 4
  tail call void @__cxa_end_catch() nounwind
  br label %return

return:
  %retval.0 = phi i32 [ %5, %catch ], [ 0, %entry ]
  ret i32 %retval.0

eh.resume:
  resume { ptr, i32 } %0
}

; One clause: the pad, the first with a clause for a type, is entry 0 of the
; table, whose one action is the catch of int. The pad gets the exception
; object in r0, and nothing reads the selector from the landingpad, whose
; register is r0 too: it reads the one the personality routine set.
; IR-LABEL: define i32 @one_typed(
; IR:       lpad:
; IR-NEXT:    %0 = landingpad { ptr, i32 }
; IR-NEXT:            cleanup
; IR-NEXT:            catch ptr @_ZTIi
; IR-NEXT:    %exn = extractvalue { ptr, i32 } %0, 0
; IR-NEXT:    call void @llvm.bpf.landingpad.index(i32 0)
; IR-NEXT:    %lpad.context = call ptr @_Unwind_GetBPFLPadContext() #[[NOUNWIND:[0-9]+]]
; IR-NEXT:    store i64 0, ptr %lpad.context, align 8
; IR-NEXT:    %lsda = call ptr @llvm.bpf.lsda()
; IR-NEXT:    %lsda.slot = getelementptr inbounds { i64, ptr, i64 }, ptr %lpad.context, i32 0, i32 1
; IR-NEXT:    store ptr %lsda, ptr %lsda.slot, align 8
; IR-NEXT:    call i32 @__gxx_bpf_personality_v0(ptr %exn) #[[NOUNWIND]]
; IR-NEXT:    %selector.slot = getelementptr inbounds { i64, ptr, i64 }, ptr %lpad.context, i32 0, i32 2
; IR-NEXT:    %selector = load i64, ptr %selector.slot, align 8
; IR-NEXT:    [[SEL:%.*]] = trunc i64 %selector to i32
; IR-NEXT:    [[VAL0:%.*]] = insertvalue { ptr, i32 } poison, ptr %exn, 0
; IR-NEXT:    [[VAL:%.*]] = insertvalue { ptr, i32 } [[VAL0]], i32 [[SEL]], 1
; IR-NEXT:    [[GOT:%.*]] = extractvalue { ptr, i32 } [[VAL]], 1
; IR-NEXT:    [[WANT:%.*]] = tail call i32 @llvm.eh.typeid.for.p0(ptr nonnull @_ZTIi)
; IR-NEXT:    %matches = icmp eq i32 [[GOT]], [[WANT]]
; IR-NOT:     extractvalue { ptr, i32 } %0
; IR:       eh.resume:
; IR-NEXT:    resume { ptr, i32 } [[VAL]]

; CHECK-LABEL: one_typed:
; CHECK:         call may_throw
; CHECK-NEXT:    unwind_goto [[PAD:.LBB[0-9_]+]]
; CHECK:       [[PAD]]:
; CHECK:         [[EXN:r[6-9]]] = r0
; CHECK-NEXT:    call _Unwind_GetBPFLPadContext
; CHECK-NEXT:    [[CTX:r[6-9]]] = r0
; CHECK-NEXT:    r1 = GCC_except_table0 ll
; CHECK-NEXT:    *(u64 *)([[CTX]] + 8) = r1
; CHECK-NEXT:    *(u64 *)([[CTX]] + 0) = 0
; CHECK-NEXT:    r1 = [[EXN]]
; CHECK-NEXT:    call __gxx_bpf_personality_v0
; CHECK-NEXT:    r1 = *(u64 *)([[CTX]] + 16)
; CHECK-NEXT:    if w1 != 1 goto
; CHECK:         r1 = [[EXN]]
; CHECK-NEXT:    call __cxa_begin_catch
; CHECK:         r0 = [[EXN]]
; CHECK-NEXT:    unwind_exit

; The table of one_typed: index-keyed call sites, as Wasm's, with the action
; chain of the pad, a cleanup since every pad is one and the catch of int,
; and the type table of absolute pointers.
; CHECK:         .section .data.rel.ro,"aw",@progbits
; CHECK:       GCC_except_table0:
; CHECK:         .byte 255 {{.*}} @LPStart Encoding = omit
; CHECK-NEXT:    .byte 0 {{.*}} @TType Encoding = absptr
; CHECK-NEXT:    .uleb128 .Lttbase0-.Lttbaseref0
; CHECK-NEXT:  .Lttbaseref0:
; CHECK-NEXT:    .byte 1 {{.*}} Call site Encoding = uleb128
; CHECK-NEXT:    .uleb128 .Lcst_end0-.Lcst_begin0
; CHECK-NEXT:  .Lcst_begin0:
; CHECK-NEXT:    .byte 0 {{.*}} >> Call Site 0 <<
; CHECK:         .byte 3 {{.*}} Action: 2
; CHECK-NEXT:  .Lcst_end0:
; CHECK-NEXT:    .byte 0 {{.*}} >> Action Record 1 <<
; CHECK:         .byte 0 {{.*}} No further actions
; CHECK-NEXT:    .byte 1 {{.*}} >> Action Record 2 <<
; CHECK:         .byte 125 {{.*}} Continue to action 1
; CHECK-NEXT:    .p2align 2
; CHECK:         .quad _ZTIi {{.*}} TypeInfo 1
; CHECK-NEXT:  .Lttbase0:

; No .gcc_except_table and no .eh_frame: the tables are data, with their
; relocations against the type_info objects.
; OBJ:     .data.rel.ro
; OBJ-NOT: .gcc_except_table
; OBJ-NOT: .eh_frame
; OBJ:     Relocation section '.rel.data.rel.ro'
; OBJ:     R_BPF_64_ABS64 {{.*}} _ZTIi

declare void @may_throw(i32 noundef)

declare i32 @__gxx_bpf_personality_v0(ptr)

declare i32 @llvm.eh.typeid.for.p0(ptr)

declare ptr @__cxa_begin_catch(ptr)

declare void @__cxa_end_catch()

define i32 @two_typed(i32 %v) uwtable personality ptr @__gxx_bpf_personality_v0 {
entry:
  invoke void @may_throw(i32 %v)
          to label %return unwind label %lpad

lpad:
  %0 = landingpad { ptr, i32 }
          catch ptr @_ZTIi
          catch ptr @_ZTI4Base
          catch ptr null
  %1 = extractvalue { ptr, i32 } %0, 0
  %2 = extractvalue { ptr, i32 } %0, 1
  %3 = tail call i32 @llvm.eh.typeid.for.p0(ptr nonnull @_ZTIi) nounwind
  %matches = icmp eq i32 %2, %3
  br i1 %matches, label %catch4, label %catch.fallthrough

catch4:
  %4 = tail call ptr @__cxa_begin_catch(ptr %1) nounwind
  br label %return.sink.split.sink.split

catch.fallthrough:
  %5 = tail call i32 @llvm.eh.typeid.for.p0(ptr nonnull @_ZTI4Base) nounwind
  %matches1 = icmp eq i32 %2, %5
  %6 = tail call ptr @__cxa_begin_catch(ptr %1) nounwind
  br i1 %matches1, label %return.sink.split.sink.split, label %return.sink.split

return.sink.split.sink.split:
  %.sink = phi ptr [ %4, %catch4 ], [ %6, %catch.fallthrough ]
  %7 = load i32, ptr %.sink, align 4
  br label %return.sink.split

return.sink.split:
  %retval.0.ph = phi i32 [ -1, %catch.fallthrough ], [ %7, %return.sink.split.sink.split ]
  tail call void @__cxa_end_catch()
  br label %return

return:
  %retval.0 = phi i32 [ 0, %entry ], [ %retval.0.ph, %return.sink.split ]
  ret i32 %retval.0
}

; Three clauses, one call: the clauses are the action chain of the table, which
; the personality walks in order, the catch-all last, and it records the
; object for the clause it matched, for __cxa_begin_catch() to return.
; IR-LABEL: define i32 @two_typed(
; IR:       lpad:
; IR-NEXT:    %0 = landingpad { ptr, i32 }
; IR-NEXT:            cleanup
; IR-NEXT:            catch ptr @_ZTIi
; IR-NEXT:            catch ptr @_ZTI4Base
; IR-NEXT:            catch ptr null
; IR-NEXT:    %exn = extractvalue { ptr, i32 } %0, 0
; IR-NEXT:    call void @llvm.bpf.landingpad.index(i32 0)
; IR:         call i32 @__gxx_bpf_personality_v0(ptr %exn)
; IR-NOT:     __gxx_bpf_personality_v0
; IR:       {{^ *}}}

; CHECK-LABEL: two_typed:
; CHECK:         unwind_goto [[PAD:.LBB[0-9_]+]]
; CHECK:       [[PAD]]:
; CHECK:         [[EXN:r[6-9]]] = r0
; CHECK:         {{r[0-9]}} = GCC_except_table1 ll
; CHECK:         call __gxx_bpf_personality_v0
; CHECK-NOT:     call __gxx_bpf_personality_v0
; CHECK:       .Lfunc_end

; CHECK:       GCC_except_table1:
; CHECK:         .byte 0 {{.*}} >> Call Site 0 <<
; CHECK:         .byte 7 {{.*}} Action: 4
; CHECK-NEXT:  .Lcst_end1:
; CHECK-NEXT:    .byte 0 {{.*}} >> Action Record 1 <<
; CHECK:         .byte 0 {{.*}} No further actions
; CHECK-NEXT:    .byte 1 {{.*}} >> Action Record 2 <<
; CHECK:         .byte 125 {{.*}} Continue to action 1
; CHECK-NEXT:    .byte 2 {{.*}} >> Action Record 3 <<
; CHECK:         .byte 125 {{.*}} Continue to action 2
; CHECK-NEXT:    .byte 3 {{.*}} >> Action Record 4 <<
; CHECK:         .byte 125 {{.*}} Continue to action 3
; CHECK-NEXT:    .p2align 2
; CHECK:         .quad _ZTIi {{.*}} TypeInfo 3
; CHECK-NEXT:    .quad _ZTI4Base {{.*}} TypeInfo 2
; CHECK-NEXT:    .quad 0 {{.*}} TypeInfo 1
; CHECK-NEXT:  .Lttbase1:

define i32 @nested(i32 %v) uwtable personality ptr @__gxx_bpf_personality_v0 {
entry:
  invoke void @may_throw(i32 %v)
          to label %return unwind label %lpad

lpad:
  %0 = landingpad { ptr, i32 }
          catch ptr @_ZTIi
          catch ptr @_ZTIl
  %1 = extractvalue { ptr, i32 } %0, 0
  %2 = extractvalue { ptr, i32 } %0, 1
  %3 = tail call i32 @llvm.eh.typeid.for.p0(ptr nonnull @_ZTIi) nounwind
  %matches = icmp eq i32 %2, %3
  br i1 %matches, label %catch, label %catch.dispatch3

catch:
  %4 = tail call ptr @__cxa_begin_catch(ptr %1) nounwind
  %5 = load i32, ptr %4, align 4
  invoke void @may_throw(i32 %5)
          to label %return.sink.split unwind label %lpad1

lpad1:
  %6 = landingpad { ptr, i32 }
          cleanup
          catch ptr @_ZTIl
  %7 = extractvalue { ptr, i32 } %6, 0
  %8 = extractvalue { ptr, i32 } %6, 1
  tail call void @__cxa_end_catch() nounwind
  br label %catch.dispatch3

catch.dispatch3:
  %exn.slot.0 = phi ptr [ %7, %lpad1 ], [ %1, %lpad ]
  %ehselector.slot.0 = phi i32 [ %8, %lpad1 ], [ %2, %lpad ]
  %lpad.val11.merged = phi { ptr, i32 } [ %6, %lpad1 ], [ %0, %lpad ]
  %9 = tail call i32 @llvm.eh.typeid.for.p0(ptr nonnull @_ZTIl) nounwind
  %matches5 = icmp eq i32 %ehselector.slot.0, %9
  br i1 %matches5, label %catch6, label %eh.resume

catch6:
  %10 = tail call ptr @__cxa_begin_catch(ptr %exn.slot.0) nounwind
  %11 = load i64, ptr %10, align 8
  %conv = trunc i64 %11 to i32
  br label %return.sink.split

return.sink.split:
  %retval.0.ph = phi i32 [ %conv, %catch6 ], [ %5, %catch ]
  tail call void @__cxa_end_catch() nounwind
  br label %return

return:
  %retval.0 = phi i32 [ 0, %entry ], [ %retval.0.ph, %return.sink.split ]
  ret i32 %retval.0

eh.resume:
  resume { ptr, i32 } %lpad.val11.merged
}

; The pad of the inner try lists the clause of the outer one after its own, and
; the pad of the call in the inner handler lists the outer one's alone: two
; pads, two entries of the table, the second the pad in the handler. The
; outer dispatch compares the selector of either with long's type id.
; IR-LABEL: define i32 @nested(
; IR:       lpad:
; IR:         call void @llvm.bpf.landingpad.index(i32 0)
; IR:         store i64 0, ptr %lpad.context, align 8
; IR:         call i32 @__gxx_bpf_personality_v0(ptr %exn)
; IR:         [[SEL32:%.*]] = trunc i64 %selector to i32
; IR-NEXT:    [[VAL0:%.*]] = insertvalue { ptr, i32 } poison, ptr %exn, 0
; IR-NEXT:    [[VAL:%.*]] = insertvalue { ptr, i32 } [[VAL0]], i32 [[SEL32]], 1
; IR-NEXT:    [[EXN:%.*]] = extractvalue { ptr, i32 } [[VAL]], 0
; IR-NEXT:    [[SEL:%.*]] = extractvalue { ptr, i32 } [[VAL]], 1
; IR:       lpad1:
; IR-NEXT:    [[LP1:%.*]] = landingpad { ptr, i32 }
; IR-NEXT:            cleanup
; IR-NEXT:            catch ptr @_ZTIl
; IR-NEXT:    %exn1 = extractvalue { ptr, i32 } [[LP1]], 0
; IR-NEXT:    call void @llvm.bpf.landingpad.index(i32 1)
; IR-NEXT:    [[CTX1:%lpad.context[0-9]+]] = call ptr @_Unwind_GetBPFLPadContext()
; IR-NEXT:    store i64 1, ptr [[CTX1]], align 8
; IR:         call i32 @__gxx_bpf_personality_v0(ptr %exn1)
; IR:         [[SEL321:%.*]] = trunc i64 {{%selector[0-9]+}} to i32
; IR-NEXT:    [[VAL10:%.*]] = insertvalue { ptr, i32 } poison, ptr %exn1, 0
; IR-NEXT:    [[VAL1:%.*]] = insertvalue { ptr, i32 } [[VAL10]], i32 [[SEL321]], 1
; IR-NEXT:    [[EXN1:%.*]] = extractvalue { ptr, i32 } [[VAL1]], 0
; IR-NEXT:    [[SEL1:%.*]] = extractvalue { ptr, i32 } [[VAL1]], 1
; IR:       catch.dispatch3:
; IR-NEXT:    %exn.slot.0 = phi ptr [ [[EXN1]], %lpad1 ], [ [[EXN]], %lpad ]
; IR-NEXT:    %ehselector.slot.0 = phi i32 [ [[SEL1]], %lpad1 ], [ [[SEL]], %lpad ]
; IR-NEXT:    %lpad.val11.merged = phi { ptr, i32 } [ [[VAL1]], %lpad1 ], [ [[VAL]], %lpad ]

; CHECK-LABEL: nested:
; CHECK:         call __gxx_bpf_personality_v0
; CHECK:         call may_throw
; CHECK-NEXT:    unwind_goto [[PAD:.LBB[0-9_]+]]
; CHECK:       [[PAD]]:
; CHECK:         [[EXN:r[6-9]]] = r0
; CHECK:         call _Unwind_GetBPFLPadContext
; CHECK:         r1 = [[EXN]]
; CHECK-NEXT:    call __gxx_bpf_personality_v0
; CHECK:         call __cxa_end_catch

; CHECK:       GCC_except_table2:
; CHECK:         .byte 0 {{.*}} >> Call Site 0 <<
; CHECK:         .byte 5 {{.*}} Action: 3
; CHECK-NEXT:    .byte 1 {{.*}} >> Call Site 1 <<
; CHECK:         .byte 3 {{.*}} Action: 2
; CHECK-NEXT:  .Lcst_end2:

define i32 @caller(i32 %v) uwtable personality ptr @__gxx_bpf_personality_v0 {
entry:
  %call = invoke fastcc i32 @_ZL12with_cleanupi(i32 %v)
          to label %return unwind label %lpad

lpad:
  %0 = landingpad { ptr, i32 }
          catch ptr @_ZTIi
  %1 = extractvalue { ptr, i32 } %0, 1
  %2 = tail call i32 @llvm.eh.typeid.for.p0(ptr nonnull @_ZTIi) nounwind
  %matches = icmp eq i32 %1, %2
  br i1 %matches, label %catch, label %eh.resume

catch:
  %3 = extractvalue { ptr, i32 } %0, 0
  %4 = tail call ptr @__cxa_begin_catch(ptr %3) nounwind
  %5 = load i32, ptr %4, align 4
  tail call void @__cxa_end_catch() nounwind
  br label %return

return:
  %retval.0 = phi i32 [ %5, %catch ], [ %v, %entry ]
  ret i32 %retval.0

eh.resume:
  resume { ptr, i32 } %0
}

; The caller of a function with a cleanup: the cleanup passes the exception
; object on in r0, and the caller's pad asks the personality about it.
; IR-LABEL: define i32 @caller(
; IR:         call i32 @__gxx_bpf_personality_v0(ptr %exn)

; CHECK-LABEL: caller:
; CHECK:         call _ZL12with_cleanupi
; CHECK-NEXT:    unwind_goto [[PAD:.LBB[0-9_]+]]
; CHECK:       [[PAD]]:
; CHECK:         [[EXN:r[6-9]]] = r0
; CHECK:         r1 = [[EXN]]
; CHECK-NEXT:    call __gxx_bpf_personality_v0

define internal fastcc range(i32 -2147483648, 101) i32 @_ZL12with_cleanupi(i32 returned %v) uwtable personality ptr @__gxx_bpf_personality_v0 {
entry:
  %g = alloca %struct.Guard, align 1
  %cmp = icmp sgt i32 %v, 100
  br i1 %cmp, label %if.then, label %if.end

if.then:
  %exception = tail call ptr @__cxa_allocate_exception(i64 4) nounwind
  store i32 %v, ptr %exception, align 16
  invoke void @__cxa_throw(ptr nonnull %exception, ptr nonnull @_ZTIi, ptr null) noreturn
          to label %unreachable unwind label %lpad

lpad:
  %0 = landingpad { ptr, i32 }
          cleanup
  call void @_ZN5GuardD1Ev(ptr nonnull align 1 dereferenceable(1) %g) nounwind
  resume { ptr, i32 } %0

if.end:
  call void @_ZN5GuardD1Ev(ptr nonnull align 1 dereferenceable(1) %g) nounwind
  ret i32 %v

unreachable:
  unreachable
}

; A cleanup has no clause to ask about: its selector is 0, with no call and
; no entry in a table.
; IR-LABEL: define internal fastcc {{.*}} @_ZL12with_cleanupi(
; IR:       lpad:
; IR-NEXT:    %0 = landingpad { ptr, i32 }
; IR-NEXT:            cleanup
; IR-NEXT:    %exn = extractvalue { ptr, i32 } %0, 0
; IR-NEXT:    [[VAL0:%.*]] = insertvalue { ptr, i32 } poison, ptr %exn, 0
; IR-NEXT:    [[VAL:%.*]] = insertvalue { ptr, i32 } [[VAL0]], i32 0, 1
; IR-NEXT:    call void @_ZN5GuardD1Ev
; IR-NEXT:    resume { ptr, i32 } [[VAL]]

; CHECK-LABEL: _ZL12with_cleanupi:
; CHECK:         call __cxa_throw
; CHECK-NEXT:    unwind_goto [[PAD:.LBB[0-9_]+]]
; CHECK:       [[PAD]]:
; CHECK-NEXT:    [[EXN:r[6-9]]] = r0
; CHECK:         call _ZN5GuardD1Ev
; CHECK-NEXT:    r0 = [[EXN]]
; CHECK-NEXT:    unwind_exit
; CHECK-NOT:   GCC_except_table4

define i32 @rethrows(i32 %v) uwtable personality ptr @__gxx_bpf_personality_v0 {
entry:
  invoke void @may_throw(i32 %v)
          to label %try.cont unwind label %lpad

lpad:
  %0 = landingpad { ptr, i32 }
          catch ptr @_ZTIi
  %1 = extractvalue { ptr, i32 } %0, 1
  %2 = tail call i32 @llvm.eh.typeid.for.p0(ptr nonnull @_ZTIi) nounwind
  %matches = icmp eq i32 %1, %2
  br i1 %matches, label %catch, label %eh.resume

catch:
  %3 = extractvalue { ptr, i32 } %0, 0
  %4 = tail call ptr @__cxa_begin_catch(ptr %3) nounwind
  invoke void @__cxa_rethrow() noreturn
          to label %unreachable unwind label %lpad1

lpad1:
  %5 = landingpad { ptr, i32 }
          cleanup
  tail call void @__cxa_end_catch() nounwind
  br label %eh.resume

try.cont:
  ret i32 0

eh.resume:
  %lpad.val4.merged = phi { ptr, i32 } [ %5, %lpad1 ], [ %0, %lpad ]
  resume { ptr, i32 } %lpad.val4.merged

unreachable:
  unreachable
}

; 'throw;' calls __cxa_rethrow(), which raises the caught exception again with
; _Unwind_RaiseException() in the runtime. The cleanup that ends the catch
; passes it on, and the caller's pad asks about it as about any other.
; IR-LABEL: define i32 @rethrows(
; IR:       lpad1:
; IR-NEXT:    [[LP1:%.*]] = landingpad { ptr, i32 }
; IR-NEXT:            cleanup
; IR-NEXT:    %exn1 = extractvalue { ptr, i32 } [[LP1]], 0
; IR-NEXT:    [[VAL10:%.*]] = insertvalue { ptr, i32 } poison, ptr %exn1, 0
; IR-NEXT:    [[VAL1:%.*]] = insertvalue { ptr, i32 } [[VAL10]], i32 0, 1
; IR:       eh.resume:
; IR-NEXT:    %lpad.val4.merged = phi { ptr, i32 } [ [[VAL1]], %lpad1 ], [ {{%.*}}, %lpad ]

; CHECK-LABEL: rethrows:
; CHECK:         call __cxa_rethrow
; CHECK-NEXT:    unwind_goto [[PAD:.LBB[0-9_]+]]
; CHECK-NEXT:  [[PAD]]:
; CHECK-NEXT:    [[EXN:r[6-9]]] = r0
; CHECK-NEXT:    call __cxa_end_catch
; CHECK-NEXT:  .LBB{{.*}}: # %eh.resume
; CHECK-NEXT:    r0 = [[EXN]]
; CHECK-NEXT:    unwind_exit

declare void @__cxa_rethrow()

define i32 @catches_rethrown(i32 %v) uwtable personality ptr @__gxx_bpf_personality_v0 {
entry:
  %call = invoke i32 @rethrows(i32 %v)
          to label %return unwind label %lpad

lpad:
  %0 = landingpad { ptr, i32 }
          catch ptr @_ZTIi
  %1 = extractvalue { ptr, i32 } %0, 1
  %2 = tail call i32 @llvm.eh.typeid.for.p0(ptr nonnull @_ZTIi) nounwind
  %matches = icmp eq i32 %1, %2
  br i1 %matches, label %catch, label %eh.resume

catch:
  %3 = extractvalue { ptr, i32 } %0, 0
  %4 = tail call ptr @__cxa_begin_catch(ptr %3) nounwind
  %5 = load i32, ptr %4, align 4
  tail call void @__cxa_end_catch() nounwind
  br label %return

return:
  %retval.0 = phi i32 [ %5, %catch ], [ 0, %entry ]
  ret i32 %retval.0

eh.resume:
  resume { ptr, i32 } %0
}

; IR-LABEL: define i32 @catches_rethrown(
; IR:         call i32 @__gxx_bpf_personality_v0(ptr %exn)

; CHECK-LABEL: catches_rethrown:
; CHECK:         call rethrows
; CHECK-NEXT:    unwind_goto [[PAD:.LBB[0-9_]+]]
; CHECK:       [[PAD]]:
; CHECK:         [[EXN:r[6-9]]] = r0
; CHECK:         r1 = [[EXN]]
; CHECK-NEXT:    call __gxx_bpf_personality_v0

declare ptr @__cxa_allocate_exception(i64)

declare void @__cxa_throw(ptr, ptr, ptr) cold noreturn

declare void @_ZN5GuardD1Ev(ptr nonnull align 1 dereferenceable(1)) nounwind

; The runtime's calls do not unwind, so no unwind_goto follows them.
; IR: declare {{.*}}ptr @_Unwind_GetBPFLPadContext() #[[NOUNWIND]]
; IR: attributes #[[NOUNWIND]] = { nounwind }
