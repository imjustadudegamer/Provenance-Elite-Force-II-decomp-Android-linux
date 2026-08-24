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

    qboolean TIKI_Frame_Commands_Server(dtiki_t *pmdl, int animnum, int framenum, tiki_cmd_t *tiki_cmd);
    // does not resolve.  Retail's companion TIKI_InitCommands_Server is @0x000FE580.
    qboolean TIKI_InitCommands_Client(dtiki_t *pmdl, tiki_cmd_t *tiki_cmd);
    qboolean
    TIKI_Frame_Commands_Client(dtiki_t *pmdl, int animnum, int framenum, tiki_cmd_t *tiki_cmd, int *extra_anims);
    qboolean TIKI_Frame_Commands_ClientTime(dtiki_t *pmdl, int animnum, float start, float end, tiki_cmd_t *tiki_cmd);

#ifdef __cplusplus
}
#endif
