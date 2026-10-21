//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "abort_message.h"

#if defined(__BPF__)

// No libc abort() ends a BPF program, and nothing prints: the one way out is
// the kernel's bpf_throw(), which discards every frame and returns its cookie
// from the program. It runs no destructor, so the verifier refuses it where a
// frame still holds a lock, a reference or interrupts off; a program that may
// terminate there loads with the work-in-progress BPF_F_THROW_NO_RELEASE.
#  include "bpf_platform.h"

void __bpf_abort(void) noexcept { bpf_throw(0); }

#else // __BPF__

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef __BIONIC__
#  include <syslog.h>
extern "C" void android_set_abort_message(const char* msg);
#endif // __BIONIC__

#if defined(__APPLE__) && __has_include(<CrashReporterClient.h>)
#   include <CrashReporterClient.h>
#   define _LIBCXXABI_USE_CRASHREPORTER_CLIENT
#endif

void __abort_message(const char* format, ...)
{
    // Write message to stderr. We do this before formatting into a
    // variable-size buffer so that we still get some information if
    // formatting into the variable-sized buffer fails.
#if !defined(NDEBUG) || !defined(LIBCXXABI_BAREMETAL)
    {
        fprintf(stderr, "libc++abi: ");
        va_list list;
        va_start(list, format);
        vfprintf(stderr, format, list);
        va_end(list);
        fprintf(stderr, "\n");
    }
#endif

    // Format the arguments into an allocated buffer. We leak the buffer on
    // purpose, since we're about to abort() anyway.
#if defined(_LIBCXXABI_USE_CRASHREPORTER_CLIENT)
    char* buffer;
    va_list list;
    va_start(list, format);
    vasprintf(&buffer, format, list);
    va_end(list);

    CRSetCrashLogMessage(buffer);
#elif defined(__BIONIC__)
    char* buffer;
    va_list list;
    va_start(list, format);
    vasprintf(&buffer, format, list);
    va_end(list);

    // Show error in tombstone.
    android_set_abort_message(buffer);

    // Show error in logcat.
    openlog("libc++abi", 0, 0);
    syslog(LOG_CRIT, "%s", buffer);
    closelog();
#endif // __BIONIC__

    abort();
}

#endif // __BPF__
