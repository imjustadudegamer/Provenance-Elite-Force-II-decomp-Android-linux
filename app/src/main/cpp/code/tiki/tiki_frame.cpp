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


#include "q_shared.h"
#include "qcommon.h"
#include "../skeletor/skeletor.h"
#include "../skeletor/tokenizer.h"
#include "../client/client.h"
#include "../corepp/tiki.h"

qboolean TIKI_Frame_Commands_Server(dtiki_t *pmdl, int animnum, int framenum, tiki_cmd_t *tiki_cmd)
{
    dtikicmd_t     *pcmd;
    dtikianimdef_t *panimdef;
    qboolean        found;
    int             i, j;

    tiki_cmd->num_cmds = 0;

    if (!pmdl || !pmdl->a) {
        return qfalse;
    }

    if (animnum >= pmdl->a->num_anims || animnum < 0) {
        TIKI_Error("TIKI_Frame_Commands_Server: Anim %d out of range for %s.\n", animnum, pmdl->a->name);
        return qfalse;
    }

    panimdef           = pmdl->a->animdefs[animnum];
    found              = false;

    for (i = 0; i < panimdef->num_server_cmds; i++) {
        pcmd = &panimdef->server_cmds[i];

        if ((pcmd->frame_num == framenum) || (pcmd->frame_num == TIKI_FRAME_EVERY && framenum >= 0)) {
            if (tiki_cmd->num_cmds >= TIKI_MAX_COMMANDS) {
                TIKI_Error(
                    "TIKI_Frame_Commands_Server: Too many cmds for anim %d frame %d in %s.\n",
                    animnum,
                    framenum,
                    pmdl->a->name
                );
                return false;
            }

            tiki_cmd->cmds[tiki_cmd->num_cmds].num_args = pcmd->num_args;
            for (j = 0; j < pcmd->num_args; j++) {
                tiki_cmd->cmds[tiki_cmd->num_cmds].args[j] = pcmd->args[j];
            }
            tiki_cmd->num_cmds++;

            found = true;
        }
    }

    return found;
}

static qboolean
TIKI_CollectClientFrameCommands(tiki_cmd_t *tiki_cmd, dtikianim_t *panims, int animnum, int framenum)
{
    dtikicmd_t     *pcmd;
    dtikianimdef_t *panimdef;
    qboolean        found;
    int             i, j;

    if (animnum >= panims->num_anims || animnum < 0) {
        TIKI_Error("TIKI_Frame_Commands_Client: Anim %d out of range for %s.\n", animnum, panims->name);
        return qfalse;
    }

    panimdef = panims->animdefs[animnum];
    found    = qfalse;

    for (i = 0; i < panimdef->num_client_cmds; i++) {
        pcmd = &panimdef->client_cmds[i];

        if ((pcmd->frame_num == framenum) || (pcmd->frame_num == TIKI_FRAME_EVERY && framenum >= 0)) {
            if (tiki_cmd->num_cmds >= TIKI_MAX_COMMANDS) {
                TIKI_Error(
                    "TIKI_Frame_Commands_Client: Too many cmds for anim %d frame %d for %s.\n",
                    animnum,
                    framenum,
                    panims->name
                );
                return qfalse;
            }

            tiki_cmd->cmds[tiki_cmd->num_cmds].num_args = pcmd->num_args;
            for (j = 0; j < pcmd->num_args; j++) {
                tiki_cmd->cmds[tiki_cmd->num_cmds].args[j] = pcmd->args[j];
            }
            tiki_cmd->num_cmds++;

            found = qtrue;
        }
    }

    return found;
}

qboolean TIKI_Frame_Commands_Client(dtiki_t *pmdl, int animnum, int framenum, tiki_cmd_t *tiki_cmd, int *extra_anims)
{
    dtikianim_t *panims;
    int          i;
    qboolean     found;

    tiki_cmd->num_cmds = 0;

    if (!pmdl || !pmdl->a) {
        return qfalse;
    }

    panims = pmdl->a;
    found  = TIKI_CollectClientFrameCommands(tiki_cmd, panims, animnum, framenum);

    if (extra_anims) {
        for (i = 0; i < NUM_EFFECTS_ANIMS; i++) {
            if (*extra_anims != -1) {
                if (TIKI_CollectClientFrameCommands(tiki_cmd, panims, *extra_anims, framenum)) {
                    found = qtrue;
                }
            }
            extra_anims++;
        }
    }

    return found;
}

qboolean TIKI_Frame_Commands_ClientTime(dtiki_t *pmdl, int animnum, float start, float end, tiki_cmd_t *tiki_cmd)
{
    dtikicmd_t     *pcmd;
    dtikianimdef_t *panimdef;
    qboolean        found;
    int             i, j;
    float           cmdTime;

    tiki_cmd->num_cmds = 0;

    if (!pmdl || !pmdl->a) {
        return qfalse;
    }

    if (animnum >= pmdl->a->num_anims || animnum < 0) {
        return qfalse;
    }

    panimdef           = pmdl->a->animdefs[animnum];
    found              = false;

    for (i = 0; i < panimdef->num_client_cmds; i++) {
        pcmd    = &panimdef->client_cmds[i];
        cmdTime = pcmd->frame_num / 1000.0f;

        if (cmdTime >= start && cmdTime <= end) {
            if (tiki_cmd->num_cmds >= TIKI_MAX_COMMANDS) {
                TIKI_Error(
                    "TIKI_Frame_Commands_Client: Too many cmds for anim %d times %f %f for %s.\n",
                    animnum,
                    start,
                    end,
                    pmdl->a->name
                );
                return false;
            }

            tiki_cmd->cmds[tiki_cmd->num_cmds].num_args = pcmd->num_args;
            for (j = 0; j < pcmd->num_args; j++) {
                tiki_cmd->cmds[tiki_cmd->num_cmds].args[j] = pcmd->args[j];
            }
            tiki_cmd->num_cmds++;

            found = true;
        }
    }

    return found;
}

qboolean TIKI_InitCommands_Client(dtiki_t *pmdl, tiki_cmd_t *tiki_cmd)
{
    dtikicmd_t *pcmd;
    int         i, j;

    tiki_cmd->num_cmds = 0;

    if (!pmdl || !pmdl->a) {
        return qfalse;
    }

    for (i = 0; i < pmdl->a->num_client_initcmds; i++) {
        pcmd = &pmdl->a->client_initcmds[i];

        if (tiki_cmd->num_cmds >= TIKI_CMD_MAX_CMDS) {
            TIKI_Error("TIKI_InitCommands_Client: Too many cmds for %s.\n", pmdl->name);
            return qfalse;
        }

        if (pcmd->num_args > TIKI_CMD_MAX_ARGS) {
            TIKI_Error("TIKI_InitCommands_Client: Too many args for cmd in %s\n", pmdl->name);
            continue;
        }

        tiki_cmd->cmds[tiki_cmd->num_cmds].num_args = pcmd->num_args;
        for (j = 0; j < pcmd->num_args; j++) {
            tiki_cmd->cmds[tiki_cmd->num_cmds].args[j] = pcmd->args[j];
        }
        tiki_cmd->num_cmds++;
    }

    return qtrue;
}
