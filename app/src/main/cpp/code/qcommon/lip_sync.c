/*
===========================================================================
This file is part of the port of Star Trek: Voyager -- Elite Force II, whose
engine descends from id Tech 3 through OpenMoHAA.

It is free software; you can redistribute it and/or modify it under the terms of
the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU General Public License, COPYING.txt at the repository
root, for more details.
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later
// Modified 2026-10-04 for this port; the changes are listed in PROVENANCE.tsv.

#include "q_shared.h"
#include "qcommon.h"

#define LIP_MAX_SLOTS     64
#define LIP_MAX_ENTRIES   512
#define LIP_MAX_NAMES     100
#define MORPH_NAME_LEN    32

#define LIP_AMPLITUDE        100.0f
#define LIP_FPS               20.0f
#define LIP_VLP_RESET          0.0f
#define LIP_VLP_GROUP          3.0f
#define LIP_VLP_THRESHOLD      5.0f
#define LIP_VLP_SCALE        100.0
#define LIP_VLP_DIVISOR      255.0

#define LIP_STR_TOOMANYNAMES  "Too many morph target names\n"
#define LIP_STR_TOOMANYENTRIES "Too many lip sync morph targets in %s\n"
#define LIP_STR_MORPH_O       "morph_o"
#define LIP_STR_MORPH_BASE    "morph_mouth_base"
#define LIP_STR_DIALOG        "dialog"
#define LIP_STR_LIP_EXT       ".lip"
#define LIP_STR_VLP_EXT       ".vlp"

typedef struct {
    int   section;
    int   nameIndex;
    float amplitude;
} lip_entry_t;

typedef struct {
    char         name[64];
    float        duration;
    int          num_entries;
    lip_entry_t  entries[LIP_MAX_ENTRIES];
} lip_sync_t;

static int        g_lipSlot;
static int        g_lipCount;
static int        g_lipWriteCursor;
static lip_sync_t g_lipSlots[LIP_MAX_SLOTS];
static char       g_morphNames[LIP_MAX_NAMES][MORPH_NAME_LEN];
static int        g_morphNameCount;

static int LipFindOrRegisterName(const char *token)
{
    int i;

    for (i = 0; i < g_morphNameCount; i++) {
        if (!stricmp(g_morphNames[i], token)) {
            return i;
        }
    }

    if (g_morphNameCount < LIP_MAX_NAMES) {
        Q_strncpyz(g_morphNames[g_morphNameCount], token, MORPH_NAME_LEN);
        i = g_morphNameCount;
        g_morphNameCount++;
        return i;
    }

    Com_WDPrintf(LIP_STR_TOOMANYNAMES);
    return -1;
}

static int loadLipFile(const char *filename, lip_sync_t *lip)
{
    char  *buffer = NULL;
    char  *cursor;
    const char *token;
    int   section = 0;

    FS_ReadFile(filename, (void **)&buffer);
    if (!buffer) {
        return 0;
    }

    cursor = buffer;

    do {
        token = COM_GetToken(&cursor, qfalse);
    } while (token && *token);

    lip->num_entries = 0;

    while (1) {
        token = COM_GetToken(&cursor, qtrue);
        if (!token || !*token) {
            break;
        }

        while (1) {
            lip_entry_t *entry;
            int          nameIndex;

            token = COM_GetToken(&cursor, qfalse);
            if (!token || !*token) {
                break;
            }

            nameIndex = LipFindOrRegisterName(token);
            if (lip->num_entries >= LIP_MAX_ENTRIES) {
                Com_WDPrintf(LIP_STR_TOOMANYENTRIES, filename);
                FS_FreeFile(buffer);
                return -1;
            }

            entry = &lip->entries[lip->num_entries];
            entry->nameIndex = nameIndex;
            entry->amplitude = LIP_AMPLITUDE;
            entry->section   = section;

            if (nameIndex != -1) {
                lip->num_entries++;
            }
        }

        section++;
    }

    lip->duration = (float)section / LIP_FPS;

    FS_FreeFile(buffer);
    return 1;
}

static int loadVolumeLipFile(const char *filename, lip_sync_t *lip)
{
    char       *buffer = NULL;
    char       *cursor;
    const char *token;
    int         idxO;
    int         idxBase;
    float       running;
    float       minv;
    float       maxv;
    float       prev;
    int         group;
    int         frame;

    idxO    = LipFindOrRegisterName(LIP_STR_MORPH_O);
    idxBase = LipFindOrRegisterName(LIP_STR_MORPH_BASE);

    if (idxO == -1 || idxBase == -1) {
        return 0;
    }

    FS_ReadFile(filename, (void **)&buffer);
    if (!buffer) {
        return 0;
    }

    cursor = buffer;

    lip->duration    = (float)atof(COM_GetToken(&cursor, qtrue));
    lip->num_entries = 0;

    running = LIP_VLP_RESET;
    minv    = LIP_AMPLITUDE;
    maxv    = LIP_VLP_RESET;
    prev    = LIP_VLP_RESET;
    group   = 0;
    frame   = 0;

    while (1) {
        float amp;
        float sel;
        lip_entry_t *entry;

        token = COM_GetToken(&cursor, qtrue);
        if (!token || !*token) {
            break;
        }

        amp = (float)(LIP_VLP_SCALE * (atof(token) / LIP_VLP_DIVISOR));
        if (amp > LIP_AMPLITUDE) {
            amp = LIP_AMPLITUDE;
        }

        group++;
        running += amp;
        if (amp < minv) {
            minv = amp;
        }
        if (amp > maxv) {
            maxv = amp;
        }

        if (group != (int)LIP_VLP_GROUP) {
            frame++;
            continue;
        }

        sel = running / LIP_VLP_GROUP;
        if (sel > prev) {
            sel = maxv;
        } else if (sel < prev) {
            sel = minv;
        }

        prev    = sel;
        running = LIP_VLP_RESET;
        maxv    = LIP_VLP_RESET;
        minv    = LIP_AMPLITUDE;
        group   = 0;

        entry = &lip->entries[lip->num_entries];

        // 0xDFAD0/0xDFADC: a loud group drives morph_o by the selected amplitude,
        // a quiet one drives morph_mouth_base at rest.
        if (sel >= LIP_VLP_THRESHOLD) {
            entry->nameIndex = idxO;
            entry->amplitude = sel;
        } else {
            entry->nameIndex = idxBase;
            entry->amplitude = LIP_VLP_RESET;
        }
        entry->section = frame;

        lip->num_entries++;
        if (lip->num_entries >= LIP_MAX_ENTRIES) {
            Com_WDPrintf(LIP_STR_TOOMANYENTRIES, filename);
            FS_FreeFile(buffer);
            return 0;
        }

        frame++;
    }

    FS_FreeFile(buffer);
    return 1;
}

int load_lip_info(const char *name)
{
    int         slot;
    lip_sync_t *lip;
    char        base[MAX_QPATH];
    size_t      len;

    if (!name || strlen(name) < 5) {
        return -1;
    }

    if (!strstr(name, LIP_STR_DIALOG)) {
        return -1;
    }

    slot = g_lipWriteCursor;
    lip  = &g_lipSlots[slot];

    memset(lip, 0, sizeof(*lip));

    len = strlen(name) - 4;
    if (len >= sizeof(base)) {
        len = sizeof(base) - 1;
    }
    memcpy(base, name, len);
    base[len] = '\0';

    Q_strcat(base, sizeof(base), LIP_STR_LIP_EXT);
    if (loadLipFile(base, lip) == 0) {
        base[len] = '\0';
        Q_strcat(base, sizeof(base), LIP_STR_VLP_EXT);
        if (loadVolumeLipFile(base, lip) == 0) {
            return -1;
        }
    }

    Q_strncpyz(lip->name, name, sizeof(lip->name));

    g_lipWriteCursor++;
    if (g_lipWriteCursor >= LIP_MAX_SLOTS) {
        g_lipWriteCursor = 0;
    }

    if (g_lipCount < LIP_MAX_SLOTS) {
        g_lipCount++;
    }

    return slot;
}

float LIP_GetLength(const char *name)
{
    int i;
    int slot;

    if (!name) {
        return -1.0f;
    }

    for (i = 0; i < g_lipCount; i++) {
        if (!stricmp(name, g_lipSlots[i].name)) {
            return g_lipSlots[i].duration;
        }
    }

    slot = load_lip_info(name);
    if (slot == -1) {
        return -1.0f;
    }

    return g_lipSlots[slot].duration;
}

char *GetNextMorphTarget(const char *name, int *index, int *frame, float *amplitude)
{
    int         lip_info = -1;
    lip_sync_t *lip;
    int         i;

    if (!name || !index) {
        if (index) {
            *index = -1;
        }
        return NULL;
    }

    if (g_lipSlot != -1 && !stricmp(name, g_lipSlots[g_lipSlot].name)) {
        lip_info = g_lipSlot;
    } else {
        for (i = 0; i < g_lipCount; i++) {
            if (!stricmp(name, g_lipSlots[i].name)) {
                lip_info = i;
                break;
            }
        }
    }

    if (lip_info == -1) {
        lip_info = load_lip_info(name);
    }

    if (lip_info == -1) {
        *index = -1;
        if (frame) {
            *frame = -1;
        }
        return NULL;
    }

    // NOTE: retail never stores lip_info back into the last-slot register.  A

    lip = &g_lipSlots[lip_info];

    *index = *index + 1;
    if (*index >= lip->num_entries || *index < 0) {
        *index = -1;
        if (frame) {
            *frame = -1;
        }
        return NULL;
    }

    if (frame) {
        *frame = lip->entries[*index].section;
    }
    if (amplitude) {
        *amplitude = lip->entries[*index].amplitude;
    }

    return g_morphNames[lip->entries[*index].nameIndex];
}

void resetAllLips(void)
{
    g_lipWriteCursor = 0;
    g_lipCount       = 0;
    g_lipSlot        = -1;
    memset(g_lipSlots, 0, sizeof(g_lipSlots));
}
