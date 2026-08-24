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

#include "../qcommon/q_shared.h"

#include "../tiki/tiki_shared.h"

#ifdef __cplusplus
class Archiver;
class str;
template<typename key, class val> class con_map;
#endif

#define MAX_TIKI_LOAD_ANIMS                 511
#define MAX_TIKI_LOAD_SKEL_INDEX            12
#define MAX_TIKI_LOAD_SERVER_INIT_COMMANDS  200
#define MAX_TIKI_LOAD_CLIENT_INIT_COMMANDS  340
#define MAX_TIKI_LOAD_BONE_MAPPINGS         96
#define MAX_TIKI_LOAD_MODEL_BUFFER          8192
#define MAX_TIKI_LOAD_EXPRESSIONS           32
#define MAX_TIKI_LOAD_MORPH_TARGETS         16

#define MAX_TIKI_LOAD_FRAME_SERVER_COMMANDS 32
#define MAX_TIKI_LOAD_FRAME_CLIENT_COMMANDS 128

#define MAX_TIKI_LOAD_SHADERS               4

typedef struct AliasList_s     AliasList_t;
typedef struct AliasListNode_s AliasListNode_t;
typedef struct msg_s           msg_t;

typedef struct {
    int indexes[3];
} tikiTriangle_t;

typedef struct {
    vec2_t st;
} tikiSt_t;

typedef struct {
    short unsigned int xyz[3];
    short int          normal;
} tikiXyzNormal_t;

typedef struct {
    vec3_t origin;
    vec3_t axis[3];
} tikiTagData_t;

typedef struct {
    char name[64];
} tikiTag_t;


typedef struct {
    int indexes[3];
} skelTriangle_t;

typedef struct dtikicmd_s {
    int    frame_num;
    int    num_args;
    char **args;
} dtikicmd_t;

typedef struct {
    int    frame_num;
    int    num_args;
    char **args;
} dloadframecmd_t;

typedef struct {
    int    num_args;
    char **args;
} dloadinitcmd_t;

typedef struct dloadmorphtarget_s {
    char  name[MAX_EXPRESSION_NAME_LENGTH]; 
    float percent;                          
} dloadmorphtarget_t;

typedef struct dloadexpression_s {
    char               *alias;                        
    int                 num_morph_targets;            
    dloadmorphtarget_t *targets[MAX_TIKI_LOAD_MORPH_TARGETS];  
} dloadexpression_t;

typedef struct {
    char  name[MAX_NAME_LENGTH];
    char  shader[MAX_TIKI_LOAD_SHADERS][MAX_RES_NAME];
    int   numskins;
    int   flags;
    float damage_multiplier;
    // Empty replSkel == the surface is not replaced (retail leaves the row at -1).
    char  replSkel[MAX_SKELNAME];
    char  replMorph[MAX_SKELNAME];
    char  replSurf[MAX_SKELNAME];
} dloadsurface_t;

typedef struct {
    char            *alias;
    char             name[MAX_QPATH];
    char             location[MAX_QPATH];
    float            weight;
    float            blendtime;
    int              flags;          
    int              internalFlags; 
    int              cache_index;
    int              num_client_cmds;
    int              num_server_cmds;
    dloadframecmd_t *loadservercmds[MAX_TIKI_LOAD_FRAME_SERVER_COMMANDS];
    dloadframecmd_t *loadclientcmds[MAX_TIKI_LOAD_FRAME_CLIENT_COMMANDS];
} dloadanim_t;

typedef struct dloaddef_s dloaddef_t;

#include "../qcommon/tiki_script.h"

#ifdef __cplusplus

typedef struct dloaddef_s {
    const char      *path;
    class TikiScript tikiFile;

    dloadanim_t    *loadanims[MAX_TIKI_LOAD_ANIMS];
    dloadinitcmd_t *loadserverinitcmds[MAX_TIKI_LOAD_SERVER_INIT_COMMANDS];
    dloadinitcmd_t *loadclientinitcmds[MAX_TIKI_LOAD_CLIENT_INIT_COMMANDS];

    dloadbonemapping_t loadbonemappings[MAX_TIKI_LOAD_BONE_MAPPINGS];
    int                numbonemappings;

    dloadexpression_t *loadexpressions[MAX_TIKI_LOAD_EXPRESSIONS];
    int                numexpressions;

    int skelIndex_ld[MAX_TIKI_LOAD_SKEL_INDEX];
    int numanims;
    int numserverinitcmds;
    int numclientinitcmds;


    qboolean bSkelModelParsed;
    qboolean bLoadAllAnims;

    struct msg_s *modelBuf;
    unsigned char modelData[MAX_TIKI_LOAD_MODEL_BUFFER];


    char     idleSkel[MAX_QPATH];
    int      numskels;
    qboolean hasSkel;
} dloaddef_t;

#endif

#include "../skeletor/skeletor.h"

#include "../corepp/tiki_main.h"
#include "../tiki/tiki_anim.h"
#include "../tiki/tiki_cache.h"
#include "../tiki/tiki_commands.h"
#include "../tiki/tiki_files.h"
#include "../tiki/tiki_imports.h"
#include "../tiki/tiki_parse.h"
#include "../tiki/tiki_skel.h"
#include "../tiki/tiki_tag.h"
#include "../tiki/tiki_utility.h"
#include "../tiki/tiki_frame.h"
#include "../tiki/tiki_surface.h"
#include "../tiki/tiki_mesh.h"
