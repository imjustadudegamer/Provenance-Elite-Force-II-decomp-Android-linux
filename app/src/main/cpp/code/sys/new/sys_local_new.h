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

#include "../qcommon/q_version.h"

#ifdef __cplusplus
extern "C" {
#endif


void Sys_InitEx(void);
void Sys_ShutdownEx(void);

void Sys_PrepareBackTrace(void);
void Sys_PrintBackTrace(void);
void Sys_PrintFaultContext(int signum, void *siginfo, void *context);
void Sys_PlatformInit_New(void);

void Sys_FaultOutSetMirror(void (*fn)(const char *text));
#if defined(__GNUC__) || defined(__clang__)
void Sys_FaultOutf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#else
void Sys_FaultOutf(const char *fmt, ...);
#endif

static inline int Sys_CaptureRotateRule(const char *prevPath, const char *curPath) {
    FILE *f;
    long  size;

    if (!prevPath || !curPath) {
        return 0;
    }

    f = fopen(curPath, "rb");
    if (!f) {
        return 0;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return 0;
    }
    size = ftell(f);
    fclose(f);

    if (size <= 0) {
        return 0;
    }

    remove(prevPath);
    if (rename(curPath, prevPath) != 0) {
        return 0;
    }
    return 1;
}


#ifdef __cplusplus
}
#endif