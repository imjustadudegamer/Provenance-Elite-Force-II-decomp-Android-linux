/*
===========================================================================
Copyright (C) 2023 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#endif

#include "../sys_local.h"

#include <signal.h>
#include <unistd.h>
#include <unwind.h>
#include <sys/resource.h>
#if defined(__linux__)
#    include <ucontext.h>
#    include <dlfcn.h>
#endif

void Sys_PlatformInit_New(void) {
    {
        struct rlimit rl;

        if (getrlimit(RLIMIT_CORE, &rl) == 0 && rl.rlim_cur != rl.rlim_max) {
            rl.rlim_cur = rl.rlim_max;
            setrlimit(RLIMIT_CORE, &rl);
        }
    }
}

static void (*s_faultMirror)(const char *text) = NULL;

void Sys_FaultOutSetMirror(void (*fn)(const char *text)) {
    s_faultMirror = fn;
}

void Sys_FaultOutf(const char *fmt, ...) {
    char    buf[1024];
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    if (!s_faultMirror) {
        return;
    }

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 1] = '\0';
    s_faultMirror(buf);
}

void Sys_PrintFaultContext(int signum, void *siginfo, void *context) {
#if defined(__linux__) && (defined(__x86_64__) || defined(__i386__) || defined(__aarch64__))
    const siginfo_t *si = (const siginfo_t *)siginfo;
    const ucontext_t *uc = (const ucontext_t *)context;
    void             *pc = NULL;
    Dl_info           info;

    if (uc) {
#    if defined(__x86_64__)
        pc = (void *)uc->uc_mcontext.gregs[REG_RIP];
#    elif defined(__i386__)
        pc = (void *)uc->uc_mcontext.gregs[REG_EIP];
#    elif defined(__aarch64__)
        pc = (void *)uc->uc_mcontext.pc;
#    endif
    }

    Sys_FaultOutf("signal %d", signum);
    if (si) {
        Sys_FaultOutf(", si_code %d, faulting address %p", si->si_code, si->si_addr);
    }
    Sys_FaultOutf("\n");

    if (pc) {
        Sys_FaultOutf("faulting instruction: %p\n", pc);
        if (dladdr(pc, &info) && info.dli_fname) {
            Sys_FaultOutf(
                "  in %s (+0x%lx)%s%s\n",
                info.dli_fname,
                (unsigned long)((const char *)pc - (const char *)info.dli_fbase),
                info.dli_sname ? " near " : "",
                info.dli_sname ? info.dli_sname : ""
            );
        }
    }
#else
    (void)signum;
    (void)siginfo;
    (void)context;
#endif
}

void Sys_PrepareBackTrace(void) {
}

typedef struct {
    void **cur;
    void **end;
} sysUnwindState_t;

static _Unwind_Reason_Code Sys_Unwind_Cb(struct _Unwind_Context *ctx, void *arg) {
    sysUnwindState_t *s = (sysUnwindState_t *)arg;
    void *pc = (void *)_Unwind_GetIP(ctx);

    if (pc) {
        if (s->cur == s->end) {
            return _URC_END_OF_STACK;
        }
        *s->cur++ = pc;
    }
    return _URC_NO_REASON;
}

static size_t Sys_Unwind_Frames(void **frames, size_t maxFrames) {
    sysUnwindState_t s;

    s.cur = frames;
    s.end = frames + maxFrames;
    _Unwind_Backtrace(Sys_Unwind_Cb, &s);
    return (size_t)(s.cur - frames);
}

void Sys_PrintBackTrace(void) {
    void*   frames[128];
    size_t  count = 0;
    size_t  i;

    count = Sys_Unwind_Frames(frames, ARRAY_LEN(frames));

    for (i = 0; i < count; i++) {
        Dl_info info;

        if (dladdr(frames[i], &info) && info.dli_sname) {
            Sys_FaultOutf("=> %s(%s+0x%lx) [%p]\n",
                info.dli_fname ? info.dli_fname : "?",
                info.dli_sname,
                (unsigned long)((const char*)frames[i] - (const char*)info.dli_saddr),
                frames[i]);
        } else if (dladdr(frames[i], &info) && info.dli_fbase) {
            Sys_FaultOutf("=> %s(+0x%lx) [%p]\n",
                info.dli_fname ? info.dli_fname : "?",
                (unsigned long)((const char*)frames[i] - (const char*)info.dli_fbase),
                frames[i]);
        } else {
            Sys_FaultOutf("=> %p\n", frames[i]);
        }
    }

    if (count <= 3) {
        Sys_FaultOutf("(stack walk stopped at the signal frame -- see the faulting instruction above,\n");
        Sys_FaultOutf(" and the core dump, for the real location)\n");
    }
}

void Sys_DebugPrint(const char* message) {
    fprintf(stderr, "%s", message);
}

void Sys_PumpMessageLoop(void)
{
}

void SetNormalThreadPriority(void)
{
}

void SetBelowNormalThreadPriority(void)
{
}