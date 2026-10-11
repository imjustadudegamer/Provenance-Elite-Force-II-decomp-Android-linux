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


#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    void TIKI_DPrintf(const char *fmt, ...) Q_PRINTF_FUNC(1, 2);

    void  TIKI_Free(void *ptr);
    void *TIKI_Alloc(size_t size);

    void TIKI_FreeFile(void *buffer);
    int  TIKI_ReadFileEx(const char *qpath, void **buffer, qboolean quiet);

    int  TIKI_SkelRollStart( void );
    void TIKI_SkelRollRead( int bytes, int ms, const char *path );
    void TIKI_SkelRollEnd( int t0 );

    int  TIKI_Millis( void );
    void TIKI_TikiRollParse( int ms );
    void TIKI_TikiRollBuild( int ms );
    void TIKI_TikiRollWarm( void );
    void TIKI_TikiRoll( void );
    void TIKI_TikiRollKeep( int kept );

    void TIKI_UpdateScreen(void);

#ifdef __cplusplus
}
#endif
