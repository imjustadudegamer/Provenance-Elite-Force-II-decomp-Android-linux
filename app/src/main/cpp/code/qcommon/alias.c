/*
===========================================================================
Copyright (C) 2024 the OpenMoHAA team

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
#include "alias.h"

static AliasList_t *Aliases;
/* retail: a 64-entry scratch array.  Alias_ListFindRandomRange only ever uses its
   size as a count bound -- it never reads an entry back -- exactly as retail. */
static AliasListNode_t *foundlist[64];

AliasListNode_t *AliasListNode_New(void)
{
    AliasListNode_t *node = Z_TagMalloc(sizeof(AliasListNode_t), TAG_TIKI);

    memset(node, 0, sizeof(AliasListNode_t));
    node->weight      = 1.0f;
    node->maximum_use = -1;
    node->next        = NULL;

    return node;
}

AliasList_t *AliasList_New(const char *name)
{
    AliasList_t *list = Z_TagMalloc(sizeof(AliasList_t), TAG_TIKI);

    list->num_in_list = 0;
    list->sorted_list = NULL;
    list->data_list   = NULL;
    list->dirty       = qfalse;

    if (name) {
        strncpy(list->name, name, sizeof(list->name));
    } else {
        list->name[0] = 0;
    }

    return list;
}

void Alias_ListClear(AliasList_t *list)
{
    AliasListNode_t *node;
    AliasListNode_t *next;

    if (list->sorted_list) {
        Z_Free(list->sorted_list);
    }

    for (node = list->data_list; node != NULL; node = next) {
        next = node->next;
        Z_Free(node);
    }

    list->num_in_list = 0;
    list->dirty       = qfalse;
    list->sorted_list = NULL;
    list->data_list   = NULL;
}

void Alias_ListDelete(AliasList_t *list)
{
    Alias_ListClear(list);
    Z_Free(list);
}

void Alias_ListClearActors(AliasList_t *list)
{
    AliasListNode_t  *node;
    AliasActorNode_t *actor;

    for (node = list->data_list; node; node = node->next) {
        actor = node->actor_list;
        node->actor_list = NULL;
        while (actor) {
            AliasActorNode_t *next = actor->next;
            Z_Free(actor);
            actor = next;
        }
    }
}

void Alias_ListAddParms(AliasListNode_t *node, const char *parameters)
{
    const char *token;

    if (!node || !parameters) {
        return;
    }

    for (;;) {
        token = COM_GetToken((char **)&parameters, qtrue);
        if (!token || !token[0]) {
            break;
        }

        if (!Q_stricmp(token, "global")) {
            node->global_flag = 1;
        } else if (!Q_stricmp(token, "stop")) {
            node->stop_flag = 1;
        } else if (!Q_stricmp(token, "timeout")) {
            token = COM_GetToken((char **)&parameters, qtrue);
            if (!token || !*token) {
                return;
            }
            if ((float)atof(token) <= 0.0f) {
                Com_WPrintf("Timeout value out of range in %s alias\n", node->alias_name);
            } else {
                node->timeout = (float)atof(token);
            }
        } else if (!Q_stricmp(token, "maxuse")) {
            token = COM_GetToken((char **)&parameters, qtrue);
            if (!token || !*token) {
                return;
            }
            if (atoi(token) <= 0) {
                Com_WPrintf("Maximum use value out of range in %s alias\n", node->alias_name);
            } else {
                node->maximum_use = atoi(token);
            }
        } else if (!Q_stricmp(token, "weight")) {
            token = COM_GetToken((char **)&parameters, qtrue);
            if (!token || !*token) {
                return;
            }
            if ((float)atof(token) <= 0.0f) {
                Com_WPrintf("Weight value out of range in %s alias\n", node->alias_name);
            } else {
                node->weight = (float)atof(token);
            }
        } else if (!Q_stricmp(token, "anim")) {
            token = COM_GetToken((char **)&parameters, qtrue);
            if (!token || !*token) {
                return;
            }
            strncpy(node->anim_name, token, sizeof(node->anim_name) - 1);
        } else if (!Q_stricmp(token, "loopanim")) {
            token = COM_GetToken((char **)&parameters, qtrue);
            if (!token || !*token) {
                return;
            }
            node->loop_anim = atoi(token) ? 1 : 0;
        } else {
            Com_WPrintf("Unknown parameter for %s alias\n", node->alias_name);
        }
    }
}

int S_ChannelNameToNum(const char *pszName)
{
    if (!Q_stricmp(pszName, "auto")) {
        return CHAN_AUTO;
    } else if (!Q_stricmp(pszName, "local")) {
        return CHAN_LOCAL;
    } else if (!Q_stricmp(pszName, "weapon")) {
        return CHAN_WEAPON;
    } else if (!Q_stricmp(pszName, "voice")) {
        return CHAN_VOICE;
    } else if (!Q_stricmp(pszName, "item")) {
        return CHAN_ITEM;
    } else if (!Q_stricmp(pszName, "body")) {
        return CHAN_BODY;
    } else if (!Q_stricmp(pszName, "dialog")) {
        return CHAN_DIALOG;
    } else if (!Q_stricmp(pszName, "dialog_secondary")) {
        return CHAN_DIALOG_SECONDARY;
    } else if (!Q_stricmp(pszName, "weaponidle")) {
        return CHAN_WEAPONIDLE;
    } else if (!Q_stricmp(pszName, "menu")) {
        return CHAN_MENU;
    } else {
        return -1;
    }
}

const char *S_ChannelNumToName(int iChannel)
{
    switch (iChannel) {
    case CHAN_AUTO:
        return "auto";
    case CHAN_LOCAL:
        return "local";
    case CHAN_WEAPON:
        return "weapon";
    case CHAN_VOICE:
        return "voice";
    case CHAN_ITEM:
        return "item";
    case CHAN_BODY:
        return "body";
    case CHAN_DIALOG:
        return "dialog";
    case CHAN_DIALOG_SECONDARY:
        return "dialog_secondary";
    case CHAN_WEAPONIDLE:
        return "weaponidle";
    case CHAN_MENU:
        return "menu";
    default:
        return NULL;
    }
}

qboolean Alias_ListAdd(AliasList_t *list, const char *alias, const char *name, const char *parameters)
{
    AliasListNode_t *ptr = NULL;
    int              i;
    AliasListNode_t *node;

    if (!list) {
        return qfalse;
    }

    if (list->dirty) {
        for (node = list->data_list; node != NULL; node = node->next) {
            if (!strncmp(node->alias_name, alias, strlen(node->alias_name))) {
                return qtrue;
            }
        }
    } else {
        ptr = Alias_ListFindNode(list, alias);
    }

    if (!ptr) {
        node = AliasListNode_New();

        if (!node) {
            Sys_Error("Alias_ListAdd : Out of memory");
        }

        if (strlen(alias) >= ALIAS_NAME_MAX) {
            Com_WDPrintf("Alias Name is too long: %s.\n", alias);
        }

        if (strlen(name) >= ALIAS_REAL_NAME_MAX) {
            Com_WDPrintf("Real Name is too long for alias: %s , %s ,\n", alias, name);
        }

        strncpy(node->alias_name, alias, ALIAS_NAME_MAX - 1);

        for (i = 0; i < ALIAS_NAME_MAX; i++) {
            node->alias_name[i] = tolower(node->alias_name[i]);
        }

        strncpy(node->real_name, name, ALIAS_REAL_NAME_MAX - 1);

        Alias_ListAddParms(node, parameters);
        list->dirty = qtrue;
        list->num_in_list++;
        node->next      = list->data_list;
        list->data_list = node;
    } else if (strcmp(name, ptr->real_name)) {
        /* retail Alias_ListAdd @0x000B7AE0: Com_WDPrintf, and the second %s is
         * the LIST name, not the new real name (string at off_22B2B8+244) */
        Com_WDPrintf("Duplicate Aliases for %s in list %s.\n", alias, list->name);
    }

    return qtrue;
}

void Alias_ListSort(AliasList_t *list)
{
    AliasListNode_t *remaining;
    AliasListNode_t *node;
    AliasListNode_t *prev;
    AliasListNode_t *lowest;
    AliasListNode_t *prev_of_lowest = NULL;
    AliasListNode_t *kept = NULL;
    AliasListNode_t *next;
    int              num;
    int              i;
    int              kept_count = 0;
    int              sorted_index = 0;

    if (!list || !list->dirty) {
        return;
    }

    remaining    = list->data_list;
    list->data_list = NULL;

    if (list->sorted_list) {
        Z_Free(list->sorted_list);
    }

    num = list->num_in_list;

    list->sorted_list = Z_TagMalloc(num * sizeof(AliasListNode_t *), TAG_TIKI);
    memset(list->sorted_list, 0, num * sizeof(AliasListNode_t *));

    for (i = 0; i < num; i++) {
        node = remaining;
        prev = remaining;
        lowest = remaining;

        for (next = node->next; next != NULL; next = next->next) {
            if (strncmp(next->alias_name, lowest->alias_name, strlen(next->alias_name)) < 0) {
                prev_of_lowest = prev;
                lowest = next;
            }

            prev = next;
        }

        if (lowest == remaining) {
            remaining = lowest->next;
        } else {
            prev_of_lowest->next = lowest->next;
        }

        if (!kept || strncmp(kept->alias_name, lowest->alias_name, strlen(kept->alias_name))) {
            list->sorted_list[sorted_index++] = lowest;
            kept_count++;
            kept = lowest;

            lowest->next   = list->data_list;
            list->data_list = lowest;
        } else {
            if (strcmp(kept->real_name, lowest->real_name)) {
                Com_WDPrintf(
                    "Duplicate Aliases for %s in list %s clashes with %s.\n",
                    kept->alias_name,
                    list->name,
                    lowest->alias_name
                );
            }

            Z_Free(lowest);
        }
    }

    list->dirty      = 0;
    list->num_in_list = kept_count;
}

const char *Alias_ListFind(AliasList_t *list, const char *alias)
{
    AliasListNode_t *node = Alias_ListFindNode(list, alias);

    if (node) {
        return node->real_name;
    } else {
        return NULL;
    }
}


int Alias_IsGlobal(AliasListNode_t *node, int actor_number)
{
    /* retail Alias_IsGlobal @0x000B8FC0 has no node guard -- `actor_number == -1 ||
     * node->global_flag`.  Both callers pass a sorted_list entry, never NULL. */
    return (actor_number == -1 || node->global_flag);
}

AliasActorNode_t *Alias_FindActor(AliasListNode_t *node, int actor_number)
{
    AliasActorNode_t *actor;

    if (!node) {
        return NULL;
    }

    for (actor = node->actor_list; actor; actor = actor->next) {
        if (actor->actor_number == actor_number) {
            return actor;
        }
    }

    return NULL;
}

static AliasActorNode_t *Alias_ListAddActor(AliasListNode_t *node, int actor_number)
{
    AliasActorNode_t *actor;
    AliasActorNode_t *check;

    if (!node || actor_number == -1) {
        return NULL;
    }

    for (check = node->actor_list; check; check = check->next) {
        if (check->actor_number == actor_number) {
            Com_WPrintf("Actor %d already added to this %s alias\n", actor_number, node->alias_name);
            return NULL;
        }
    }

    actor = (AliasActorNode_t *)Z_TagMalloc(sizeof(AliasActorNode_t), TAG_TIKI);
    if (actor) {
        memset(actor, 0, sizeof(*actor));
        actor->actor_number = actor_number;
        actor->next         = node->actor_list;
        node->actor_list    = actor;
    } else {
        actor = NULL;
    }

    return actor;
}

int Alias_ListNumNotUsed(AliasList_t *list, int min_index, int max_index, int actor_number)
{
    AliasListNode_t  *node;
    AliasActorNode_t *actor;
    int               i;
    int               count;
    int               entries;

    if (min_index == -1 || max_index == -1) {
        return 0;
    }

    count   = 0;
    entries = max_index - min_index + 1;

    for (i = 0; i < entries; i++) {
        node = list->sorted_list[min_index + i];

        if (actor_number == -1 || node->global_flag) {
            if (!node->been_played_this_loop) {
                count++;
            }
            continue;
        }

        actor = Alias_FindActor(node, actor_number);
        if (!actor) {
            actor = Alias_ListAddActor(node, actor_number);
        }

        if (actor && !actor->been_played_this_loop) {
            count++;
        }
    }

    return count;
}

void Alias_ListResetAlias(AliasList_t *list, int min_index, int max_index, int actor_number)
{
    AliasListNode_t  *node;
    AliasActorNode_t *actor;
    int               i;
    int               entries;

    if (min_index == -1 || max_index == -1) {
        return;
    }

    entries = max_index - min_index + 1;

    for (i = 0; i < entries; i++) {
        node = list->sorted_list[min_index + i];

        if (Alias_IsGlobal(node, actor_number)) {
            if (node->stop_flag) {
                continue;
            }

            if ((double)(Sys_Milliseconds() - node->last_time_played) / 1000.0 > (double)node->timeout) {
                if (node->maximum_use == -1 || node->number_of_times_played < node->maximum_use) {
                    node->been_played_this_loop = 0;
                }
            }
        } else {
            actor = Alias_FindActor(node, actor_number);
            if (!actor) {
                continue;
            }

            if (node->stop_flag) {
                continue;
            }

            if ((double)(Sys_Milliseconds() - actor->last_time_played) / 1000.0 > (double)node->timeout) {
                if (node->maximum_use == -1 || actor->number_of_times_played < node->maximum_use) {
                    actor->been_played_this_loop = 0;
                }
            }
        }
    }
}

const char *Alias_ListFindDialog(AliasList_t *list, const char *alias, qboolean random_flag, int actor_number)
{
    AliasListNode_t  *node;
    AliasActorNode_t *actor;
    int               min_index;
    int               max_index;
    float             total_weight;
    int               num_not_used;
    int               target;
    int               used;
    int               i;
    int               entries;

    if (!list) {
        return NULL;
    }

    Alias_ListFindRandomRange(list, alias, &min_index, &max_index, &total_weight);

    if (min_index == -1 || max_index == -1) {
        return NULL;
    }

    num_not_used = Alias_ListNumNotUsed(list, min_index, max_index, actor_number);

    if (!num_not_used) {
        Alias_ListResetAlias(list, min_index, max_index, actor_number);
        num_not_used = Alias_ListNumNotUsed(list, min_index, max_index, actor_number);
    }

    if (!num_not_used) {
        return NULL;
    }

    if (random_flag) {
        target = (int)((float)num_not_used * randweight() + 0.5f);
    } else {
        target = 0;
    }

    used    = 0;
    entries = max_index - min_index + 1;

    for (i = 0; i < entries; i++) {
        node = list->sorted_list[min_index + i];

        if (actor_number == -1 || node->global_flag) {
            if (node->been_played_this_loop) {
                continue;
            }

            if (target != used) {
                used++;
                continue;
            }

            node->been_played_this_loop = 1;
            node->last_time_played      = Sys_Milliseconds();
            node->number_of_times_played++;
        } else {
            actor = Alias_FindActor(node, actor_number);
            if (!actor || actor->been_played_this_loop) {
                continue;
            }

            if (target != used) {
                used++;
                continue;
            }

            actor->been_played_this_loop = 1;
            actor->last_time_played      = Sys_Milliseconds();
            actor->number_of_times_played++;
        }

        return node->real_name;
    }

    return NULL;
}

void Alias_ListUpdateDialog(
    AliasList_t *list, const char *alias, int number_of_times_played, byte been_played_this_loop, int last_time_played
)
{
    AliasListNode_t *node;
    int              min_index;
    int              max_index;
    float            total_weight;

    Alias_ListFindRandomRange(list, alias, &min_index, &max_index, &total_weight);

    if (min_index == -1 || min_index != max_index) {
        return;
    }

    node                        = list->sorted_list[min_index];
    node->been_played_this_loop = been_played_this_loop;
    node->last_time_played      = last_time_played;
    node->number_of_times_played = number_of_times_played;
}

void Alias_ListAddActorDialog(
    AliasList_t *list, const char *alias, int actor_number, int number_of_times_played, byte been_played_this_loop,
    int last_time_played
)
{
    AliasListNode_t  *node;
    AliasActorNode_t *actor;
    int               min_index;
    int               max_index;
    float             total_weight;

    Alias_ListFindRandomRange(list, alias, &min_index, &max_index, &total_weight);

    if (min_index == -1 || min_index != max_index) {
        return;
    }

    node = list->sorted_list[min_index];

    actor = Alias_FindActor(node, actor_number);
    if (!actor) {
        actor = Alias_ListAddActor(node, actor_number);
    }

    actor->actor_number          = actor_number;
    actor->been_played_this_loop = been_played_this_loop;
    actor->last_time_played      = last_time_played;
    actor->number_of_times_played = number_of_times_played;
}

AliasListNode_t *Alias_ListFindNode(AliasList_t *list, const char *alias)
{
    char   convalias[ALIAS_NAME_MAX];
    int    index;
    int    l;
    int    r;
    int    diff;
    size_t length;

    if (!list) {
        return NULL;
    }

    if (!alias) {
        return NULL;
    }

    if (list->dirty) {
        Alias_ListSort(list);
    }

    length = strlen(alias);

    if (length >= ALIAS_NAME_MAX) {
        Com_WPrintf("Alias name too long - %s\n", alias);
        return NULL;
    }

    for (index = 0; index < length; index++) {
        convalias[index] = tolower(alias[index]);
    }

    convalias[length] = 0;

    r = 0;
    l = list->num_in_list - 1;
    while (l >= r) {
        index = (r + l) >> 1;
        diff  = strncmp(convalias, list->sorted_list[index]->alias_name, length);

        if (diff == 0) {
            return list->sorted_list[index];
        } else if (diff > 0) {
            r = index + 1;
        } else {
            l = index - 1;
        }
    }

    return NULL;
}

float randweight(void)
{
    return ((float)(rand() & 0x7fff)) / ((float)0x8000);
}

const char *Alias_ListFindRandom(AliasList_t *list, const char *alias)
{
    int   min_index;
    int   max_index;
    float total_weight = 0.0f;
    int   i;
    int   numfound;
    float weight;
    float num;

    Alias_ListFindRandomRange(list, alias, &min_index, &max_index, &total_weight);

    if (min_index == -1 || max_index == -1) {
        return NULL;
    }

    num      = 0.0f;
    numfound = max_index - min_index + 1;
    weight   = randweight() * total_weight;

    for (i = 0; i < numfound; i++) {
        num += list->sorted_list[i + min_index]->weight;
        if (num > weight) {
            return list->sorted_list[i + min_index]->real_name;
        }
    }

    return NULL;
}

void Alias_ListFindRandomRange(
    AliasList_t *list, const char *alias, int *min_index, int *max_index, float *total_weight
)
{
    char              convalias[ALIAS_NAME_MAX];
    int               index;
    int               l;
    int               r;
    int               diff;
    int               i;
    size_t            length;
    int               numfound;
    float             totalfoundweight = 0.0f;
    AliasListNode_t **ptr;

    *min_index = -1;
    *max_index = -1;

    if (!list || !alias) {
        return;
    }

    if (list->dirty) {
        Alias_ListSort(list);
    }

    length = strlen(alias);

    if (length + 1 > ALIAS_NAME_MAX) {
        return;
    }

    for (i = 0; i < length; i++) {
        convalias[i] = tolower(alias[i]);
    }

    convalias[length] = 0;

    l = 0;
    r = list->num_in_list - 1;
    while (r >= l) {
        index = (l + r) >> 1;
        diff  = strncmp(convalias, list->sorted_list[index]->alias_name, length);

        if (diff == 0) {
            break;
        } else if (diff > 0) {
            l = index + 1;
        } else {
            r = index - 1;
        }
    }

    if (r < l) {
        return;
    }

    numfound         = 0;
    *min_index       = index;
    *max_index       = index;
    totalfoundweight = 0.f;

    ptr = &list->sorted_list[index];
    for (i = index + 1; i > 0; i--, ptr--) {
        if (strncmp(convalias, (*ptr)->alias_name, length)) {
            break;
        }

        if (numfound < ARRAY_LEN(foundlist) && (*ptr)->alias_name[length] != '_') {
            foundlist[numfound++] = *ptr;
            totalfoundweight += (*ptr)->weight;
            *min_index = i - 1;
        }
    }

    ptr = &list->sorted_list[index + 1];
    for (i = index + 1; i < list->num_in_list; i++, ptr++) {
        if (strncmp(convalias, (*ptr)->alias_name, length)) {
            break;
        }

        if (numfound < ARRAY_LEN(foundlist) && (*ptr)->alias_name[length] != '_') {
            foundlist[numfound++] = *ptr;
            totalfoundweight += (*ptr)->weight;
            *max_index = i;
        }
    }

    if (!numfound) {
        *min_index = -1;
        *max_index = -1;
        return;
    }

    *total_weight = totalfoundweight;
}

void Alias_ListDump(AliasList_t *list)
{
    AliasListNode_t *ptr;
    int              i;

    if (!list) {
        return;
    }

    if (list->dirty) {
        Alias_ListSort(list);
    }

    Com_DPrintf("Alias List: %s\n", list->name);
    Com_DPrintf("Alias Name / Weight / Real Name\n");

    for (i = 0; i < list->num_in_list; ++i) {
        ptr = list->sorted_list[i];
        Com_DPrintf("%32s %3.2f %s\n", ptr->alias_name, ptr->weight, ptr->real_name);
    }

    Com_DPrintf("%d total aliases\n", list->num_in_list);
}

const char *Alias_Find(const char *alias)
{
    if (!Aliases) {
        Aliases = AliasList_New("Global");
    }
    return Alias_ListFind(Aliases, alias);
}

qboolean Alias_Add(const char *alias, const char *name, const char *parameters)
{
    if (!Aliases) {
        Aliases = AliasList_New("Global");
    }
    return Alias_ListAdd(Aliases, alias, name, parameters);
}

qboolean Alias_Delete(const char *alias)
{
    return qtrue;
}

const char *Alias_FindRandom(const char *alias)
{
    if (!Aliases) {
        Aliases = AliasList_New("Global");
    }
    return Alias_ListFindRandom(Aliases, alias);
}

void Alias_Dump(void)
{
    if (!Aliases) {
        Aliases = AliasList_New("Global");
    }
    Alias_ListDump(Aliases);
}

void Alias_Clear(void)
{
    if (!Aliases) {
        Aliases = AliasList_New("Global");
    }
    Alias_ListClear(Aliases);
}

const char *Alias_ListFindSpecificAnim(AliasList_t *list, const char *name)
{
    AliasListNode_t *node;

    if (!list) {
        return NULL;
    }

    for (node = list->data_list; node; node = node->next) {
        size_t len = strlen(node->real_name);
        if (!strncmp(node->real_name, name, len)) {
            return node->anim_name;
        }
    }

    return NULL;
}

qboolean Alias_ListCheckLoopAnim(AliasList_t *list, const char *name)
{
    AliasListNode_t *node;

    if (!list) {
        return qfalse;
    }

    for (node = list->data_list; node; node = node->next) {
        size_t len = strlen(node->real_name);
        if (!strncmp(node->real_name, name, len)) {
            return node->loop_anim;
        }
    }

    return qfalse;
}
