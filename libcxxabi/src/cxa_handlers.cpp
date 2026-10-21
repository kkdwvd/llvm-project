//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//
// This file implements the functionality associated with the terminate_handler,
// unexpected_handler, and new_handler.
//===----------------------------------------------------------------------===//

#include <stdexcept>
#include <new>
#include <exception>
#include "abort_message.h"
#include "cxxabi.h"
#include "cxa_handlers.h"
#include "cxa_exception.h"
#include "private_typeinfo.h"
#include "include/atomic_support.h" // from libc++

namespace std
{

unexpected_handler
get_unexpected() noexcept
{
    return __libcpp_atomic_load(&__cxa_unexpected_handler, _AO_Acquire);
}

void
__unexpected(unexpected_handler func)
{
    func();
    // unexpected handler should not return
    __abort_message("unexpected_handler unexpectedly returned");
}

[[noreturn]] void
unexpected()
{
    __unexpected(get_unexpected());
}

terminate_handler
get_terminate() noexcept
{
    return __libcpp_atomic_load(&__cxa_terminate_handler, _AO_Acquire);
}

void
__terminate(terminate_handler func) noexcept
{
#if defined(__BPF__)
    // The handler is not called. A call through the stored pointer here would
    // be a call through a number inside every destructor, which is noexcept,
    // and the kernel refuses a function whose address a program stores, as a
    // thrown object's destructor and every virtual function are, when it can
    // make such a call itself, as recursion. The program ends as it would
    // after the handler, through bpf_throw() (abort_message.cpp).
    // TODO(kkd): kernel limitation. With verified calls through a pointer the
    // handler is called as below.
    (void)func;
    __abort_message("terminating");
#else
#ifndef _LIBCXXABI_NO_EXCEPTIONS
    try
    {
#endif // _LIBCXXABI_NO_EXCEPTIONS
        func();
        // handler should not return
        __abort_message("terminate_handler unexpectedly returned");
#ifndef _LIBCXXABI_NO_EXCEPTIONS
    }
    catch (...)
    {
        // handler should not throw exception
        __abort_message("terminate_handler unexpectedly threw an exception");
    }
#endif // _LIBCXXABI_NO_EXCEPTIONS
#endif // __BPF__
}

[[noreturn]] void
terminate() noexcept
{
#ifndef _LIBCXXABI_NO_EXCEPTIONS
    // If there might be an uncaught exception
    using namespace __cxxabiv1;
    __cxa_eh_globals* globals = __cxa_get_globals_fast();
    if (globals)
    {
        __cxa_exception* exception_header = globals->caughtExceptions;
        if (exception_header)
        {
            _Unwind_Exception* unwind_exception =
                reinterpret_cast<_Unwind_Exception*>(exception_header + 1) - 1;
            if (__isOurExceptionClass(unwind_exception))
                __terminate(exception_header->terminateHandler);
        }
    }
#endif
    __terminate(get_terminate());
}

new_handler
get_new_handler() noexcept
{
    return __libcpp_atomic_load(&__cxa_new_handler, _AO_Acquire);
}

}  // std
