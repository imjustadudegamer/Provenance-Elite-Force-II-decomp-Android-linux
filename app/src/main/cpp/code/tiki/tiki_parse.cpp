

#include "q_shared.h"
#include "qcommon.h"
#include "../server/server.h"
#include "../skeletor/skeletor.h"
#include "../corepp/tiki.h"

typedef enum tiki_setupcase_e {
    SETUP_END_MESSAGE = -1,
    SETUP_LOAD_SCALE,
    SETUP_LOD_SCALE,
    SETUP_LOD_BIAS,
    SETUP_SKELMODEL,
    SETUP_LOAD_ORIGIN,
    SETUP_LIGHT_OFFSET,
    SETUP_RADIUS,
    SETUP_SURFACE,
    SETUP_FLAGS,
    SETUP_DAMAGE,
    SETUP_SHADER,
    SETUP_MORPHFILE,     
    SETUP_REPLACESURFACE, 
    SETUP_LOD,            
    SETUP_FADE_DIST_MOD,  
    SETUP_BOUNDING_MIN,   
    SETUP_BOUNDING_MAX,   
    SETUP_HEAD_OFFSET,    
    SETUP_LOAD_ALL_ANIMS  
} tiki_setupcase_t;

const char *TIKI_FileExtension(const char *in)
{
    static char exten[8];
    int         i;

    for (i = 0; in[i] != 0; i++) {
        if (in[i] == '.') {
            i++;
            break;
        }
    }

    if (!in[i]) {
        return "";
    }

    strncpy(exten, &in[i], sizeof(exten));
    return exten;
}

void TIKI_ParseFrameCommands(
    dloaddef_t       *ld,
    dloadframecmd_t **cmdlist,
    int               maxcmds,
    int              *numcmds,
    const char       *filename,
    const char       *animname
    )
{
    qboolean         usecurrentframe = false;
    const char      *token;
    dloadframecmd_t *cmds;
    int              framenum = 0;
    int              i;
    char            *pszArgs[TIKI_CMD_MAX_ARGS];

    ld->tikiFile.GetToken(true);

    while (ld->tikiFile.TokenAvailable(true)) {
        token = ld->tikiFile.GetToken(true);
        if (!Q_stricmp(token, "}")) {
            break;
        }

        if (*numcmds < maxcmds) {
            cmds              = (dloadframecmd_t *)TIKI_AllocateLoadData(sizeof(dloadframecmd_t));
            cmdlist[*numcmds] = cmds;
            if (!cmds) {
                TIKI_Error(
                    "TIKI_ParseFrameCommands: could not allocate storage for dloadframecmd_t in %s on line %d.\n",
                    ld->tikiFile.Filename(),
                    ld->tikiFile.GetLineNumber()
                );
                continue;
            }

            cmds->num_args = 0;
            (*numcmds)++;

            if (!Q_stricmp(token, "start") || !Q_stricmp(token, "first")) {
                framenum = TIKI_FRAME_FIRST;
            } else if (!Q_stricmp(token, "end")) {
                framenum = TIKI_FRAME_END;
            } else if (!Q_stricmp(token, "last")) {
                framenum = TIKI_FRAME_LAST;
            } else if (!Q_stricmp(token, "every")) {
                framenum = TIKI_FRAME_EVERY;
            } else if (!Q_stricmp(token, "exit")) {
                framenum = TIKI_FRAME_EXIT;
            } else if (!Q_stricmp(token, "entry") || !Q_stricmp(token, "enter")) {
                framenum = TIKI_FRAME_ENTRY;
            } else if (!Q_stricmp(token, "(")) {
                usecurrentframe = true;
                ld->tikiFile.UnGetToken();
            } else if (!Q_stricmp(token, ")")) {
                usecurrentframe = false;
                ld->tikiFile.UnGetToken();
            } else if (!usecurrentframe) {
                framenum = atoi(token);
            } else {
                ld->tikiFile.UnGetToken();
            }

            cmds->frame_num = framenum;

            while (ld->tikiFile.TokenAvailable(false)) {
                token = ld->tikiFile.GetToken(false);

                if (cmds->num_args >= TIKI_CMD_MAX_ARGS) {
                    TIKI_Error(
                        "TIKI_ParseFrameCommands: too many args in anim commands in %s anim %s.\n",
                        filename,
                        animname
                    );
                    continue;
                }

                pszArgs[cmds->num_args] = TIKI_CopyString(token);
                cmds->num_args++;
            }

            cmds->args = (char **)TIKI_AllocateLoadData(cmds->num_args * sizeof(char *));
            for (i = 0; i < cmds->num_args; i++) {
                cmds->args[i] = pszArgs[i];
            }
        } else {
            TIKI_Error(
                "TIKI_ParseFrameCommands: too many anim commands in %s anim %s.\n",
                filename,
                animname
            );
            while (ld->tikiFile.TokenAvailable(false)) {
                ld->tikiFile.GetToken(false);
            }
        }
    }
}

void TIKI_ParseAnimationCommands(dloaddef_t *ld, dloadanim_t *anim, const char *filename)
{
    const char *token;

    while (ld->tikiFile.TokenAvailable(true)) {
        token = ld->tikiFile.GetToken(true);
        if (!Q_stricmp(token, "client")) {
            TIKI_ParseFrameCommands(
                ld,
                anim->loadclientcmds,
                sizeof(anim->loadclientcmds) / sizeof(anim->loadclientcmds[0]),
                &anim->num_client_cmds,
                filename,
                anim->alias
            );
        } else if (!Q_stricmp(token, "server")) {
            TIKI_ParseFrameCommands(
                ld,
                anim->loadservercmds,
                sizeof(anim->loadservercmds) / sizeof(anim->loadservercmds[0]),
                &anim->num_server_cmds,
                filename,
                anim->alias
            );
        } else if (!Q_stricmp(token, "}")) {
            break;
        } else {
            TIKI_Error(
                "TIKI_ParseAnimationCommands: unknown anim command %s in %s on line %d, skipping line.\n",
                token,
                ld->tikiFile.Filename(),
                ld->tikiFile.GetLineNumber()
            );
            while (ld->tikiFile.TokenAvailable(false)) {
                token = ld->tikiFile.GetToken(false);
            }
        }
    }
}

void TIKI_ParseAnimationFlags(dloaddef_t *ld, dloadanim_t *anim)
{
    const char *token;

    anim->weight        = 1.0f;
    anim->blendtime     = 200.0f;
    anim->flags         = 0;
    anim->internalFlags = 0;

    while (ld->tikiFile.TokenAvailable(false)) {
        token = ld->tikiFile.GetToken(false);

        if (!Q_stricmp(token, "weight")) {
            anim->weight = ld->tikiFile.GetFloat(false);
            anim->internalFlags |= TAI_RANDOM;
        } else if (!Q_stricmp(token, "deltadriven")) {
            anim->flags |= TAF_DELTADRIVEN;
        } else if (!Q_stricmp(token, "default_angles")) {
            anim->flags |= TAF_DEFAULT_ANGLES;
        } else if (!Q_stricmp(token, "notimecheck")) {
            anim->flags |= TAF_NOTIMECHECK;
        } else if (!Q_stricmp(token, "crossblend")) {
            anim->blendtime = (float)ld->tikiFile.GetInteger(false);
        } else {
            TIKI_Error(
                "Unknown Animation flag %s for anim '%s' in %s\n",
                token,
                anim->alias,
                TikiScript::currentScript->Filename()
            );
        }
    }
}

void TIKI_ParseAnimationsFail(dloaddef_t *ld)
{
    int         nestcount = 0;
    const char *token;

    while (ld->tikiFile.TokenAvailable(false)) {
        ld->tikiFile.GetToken(false);
    }

    if (!ld->tikiFile.TokenAvailable(true)) {
        return;
    }

    token = ld->tikiFile.GetToken(true);
    if (Q_stricmp(token, "{")) {
        ld->tikiFile.UnGetToken();
        return;
    }

    ld->tikiFile.UnGetToken();

    while (ld->tikiFile.TokenAvailable(true)) {
        token = ld->tikiFile.GetToken(true);
        if (!Q_stricmp(token, "{")) {
            nestcount++;
        } else if (!Q_stricmp(token, "}")) {
            nestcount--;
            if (!nestcount) {
                break;
            }
        }
    }
}

void TIKI_ParseExpressionTargets(dloaddef_t *ld, dloadexpression_t *expression, const char *filename)
{
    const char        *token;
    dloadmorphtarget_t *target;

    while (ld->tikiFile.TokenAvailable(true)) {
        token = ld->tikiFile.GetToken(true);
        if (!Q_stricmp(token, "}")) {
            break;
        }

        if (strlen(token) >= MAX_EXPRESSION_NAME_LENGTH) {
            TIKI_Error(
                "TIKI_ParseExpressionTargets: morph target name %s in %s on line %d too long, skipping line.\n",
                token,
                filename,
                ld->tikiFile.GetLineNumber()
            );

            while (ld->tikiFile.TokenAvailable(false)) {
                ld->tikiFile.GetToken(false);
            }

            continue;
        }

        target = (dloadmorphtarget_t *)TIKI_AllocateLoadData(sizeof(dloadmorphtarget_t));
        expression->targets[expression->num_morph_targets] = target;

        if (!target) {
            TIKI_Error(
                "TIKI_ParseExpressionTargets: could not allocate storage for dloadmorphtarget_t in %s on line %d.\n",
                filename,
                ld->tikiFile.GetLineNumber()
            );
            continue;
        }

        strcpy(target->name, token);

        if (ld->tikiFile.TokenAvailable(false)) {
            target->percent = atof(ld->tikiFile.GetToken(false));
            expression->num_morph_targets++;
        }
    }
}

void TIKI_ParseExpressions(dloaddef_t *ld, const char *filename)
{
    const char        *token;
    dloadexpression_t *expression;

    ld->tikiFile.GetToken(true);

    while (ld->tikiFile.TokenAvailable(true)) {
        token = ld->tikiFile.GetToken(true);
        if (!Q_stricmp(token, "}")) {
            break;
        }

        if (ld->numexpressions >= MAX_TIKI_LOAD_EXPRESSIONS) {
            TIKI_Error("TIKI_ParseExpressions: Too many expressions.\n");
            continue;
        }

        expression                               = (dloadexpression_t *)TIKI_AllocateLoadData(sizeof(dloadexpression_t));
        ld->loadexpressions[ld->numexpressions] = expression;

        if (!expression) {
            TIKI_Error(
                "TIKI_ParseExpressions: could not allocate storage for dloadexpression_t in %s on line %d.\n",
                filename,
                ld->tikiFile.GetLineNumber()
            );
            continue;
        }

        if (strlen(token) >= MAX_EXPRESSION_NAME_LENGTH) {
            TIKI_Error("TIKI_ParseExpressions: Expression alias name %s is too long in %s.\n", token, filename);
        }

        expression->alias = (char *)TIKI_AllocateLoadData(strlen(token) + 1);

        if (!expression->alias) {
            TIKI_Error(
                "TIKI_ParseExpressions: Could not allocate storage for Expression alias name %s in %s.\n",
                token,
                filename
            );
            continue;
        }

        strcpy(expression->alias, token);
        expression->num_morph_targets = 0;
        ld->numexpressions++;

        if (ld->tikiFile.TokenAvailable(true)) {
            token = ld->tikiFile.GetToken(true);
            if (Q_stricmp(token, "{")) {
                ld->tikiFile.UnGetToken();
                TIKI_Error(
                    "TIKI_ParseExpressions: No morph targets defined for expression %s in %s.\n",
                    expression->alias,
                    filename
                );
            } else {
                TIKI_ParseExpressionTargets(ld, expression, filename);
            }
        }
    }
}

dloadanim_t *TIKI_AllocAnim(dloaddef_t *ld)
{
    dloadanim_t *panim;

    panim                         = static_cast<dloadanim_t *>(TIKI_AllocateLoadData(sizeof(dloadanim_t)));
    panim->weight                 = 1.f;
    panim->location[0]            = 0;
    panim->num_client_cmds        = 0;
    panim->num_server_cmds        = 0;
    panim->flags                  = 0;
    panim->blendtime              = 200.0f;
    ld->loadanims[ld->numanims++] = panim;

    return panim;
}

void TIKI_ParseAnimations(dloaddef_t *ld)
{
    const char  *token;
    dloadanim_t *anim;
    qboolean     b_mapspec = false;
    const char  *mapname;
    size_t       depth = 0;

    token = ld->tikiFile.GetToken(true);
    if (Q_stricmp(token, "{") && TikiScript::currentScript) {
        size_t len;

        Q_strncpyz(TikiScript::currentScript->path, token, sizeof(TikiScript::currentScript->path));
        len = strlen(TikiScript::currentScript->path);
        if (len && TikiScript::currentScript->path[len - 1] != '/'
            && len + 1 < sizeof(TikiScript::currentScript->path)) {
            TikiScript::currentScript->path[len]     = '/';
            TikiScript::currentScript->path[len + 1] = '\0';
        }

        ld->tikiFile.GetToken(true); 
    }

    while (ld->tikiFile.TokenAvailable(true)) {
        token = ld->tikiFile.GetToken(true);
        if (!Q_stricmp(token, "}")) {
            if (!b_mapspec) {
                return;
            }
            b_mapspec = false;
        } else if (!Q_stricmp(token, "$mapspec")) {
            token = ld->tikiFile.GetToken(true);
            if (sv_mapname) {
                mapname = sv_mapname->string;
            } else {
                mapname = "utils";
            }

            while (ld->tikiFile.TokenAvailable(true)) {
                if (!Q_stricmpn(token, mapname, strlen(token))) {
                    b_mapspec = true;
                } else if (!Q_stricmp(token, "{")) {
                    break;
                }

                token = ld->tikiFile.GetToken(true);
            }

            if (!b_mapspec) {
                while (ld->tikiFile.TokenAvailable(true)) {
                    token = ld->tikiFile.GetToken(true);
                    if (strstr(token, "{")) {
                        depth++;
                    }

                    if (strstr(token, "}")) {
                        if (!depth) {
                            break;
                        }

                        depth--;
                    }
                }
            }
        } else {
            if (ld->numanims >= MAX_TIKI_LOAD_ANIMS) {
                // retail does not consume the rest of the declaration before continuing --
                TIKI_Error("TIKI_ParseAnimations: Too many animations in %s.\n", ld->path);
                continue;
            }

            anim                        = (dloadanim_t *)TIKI_AllocateLoadData(sizeof(dloadanim_t));
            ld->loadanims[ld->numanims] = anim;
            anim->cache_index           = -1;
            if (!anim) {
                TIKI_Error(
                    "TIKI_ParseAnimations: Could not allocate storage for anim alias name %s in %s.\n",
                    token,
                    ld->tikiFile.Filename()
                );
                continue;
            }

            if (strlen(token) >= 48) {
                TIKI_Error(
                    "TIKI_ParseAnimations: Anim alias name %s is too long in %s.\n", token, ld->tikiFile.Filename()
                );
            }

            anim->alias = (char *)TIKI_CopyString(token);

            token = ld->tikiFile.GetToken(false);
            if (strncmp(token, "models/", 7) && strncmp(token, "models\\", 7)) {
                Q_strncpyz(anim->name, TikiScript::currentScript->path, sizeof(anim->name));
                Q_strcat(anim->name, sizeof(anim->name), token);
            } else {
                Q_strncpyz(anim->name, token, sizeof(anim->name));
            }

            {
                const char *ext = strrchr(anim->name, '.');
                if (ext && !Q_stricmp(ext, ".tan")) {
                    WriteSkelmodel(ld, anim->name);
                }
            }

            anim->location[0] = 0;
            if (ld->tikiFile.currentScript) {
                Com_sprintf(
                    anim->location,
                    sizeof(anim->location),
                    "%s, line: %d",
                    ld->tikiFile.currentScript->Filename(),
                    ld->tikiFile.currentScript->GetLineNumber()
                );
            }

            anim->cache_index =
                TIKI_RegisterAnim(anim->name, (!ld->bSkelModelParsed || ld->bLoadAllAnims) ? qtrue : qfalse);

            anim->num_client_cmds = 0;
            anim->num_server_cmds = 0;

            if (anim->cache_index >= 0) {
                TIKI_ParseAnimationFlags(ld, anim);
                ld->numanims++;

                if (ld->tikiFile.TokenAvailable(true)) {
                    token = ld->tikiFile.GetToken(true);
                    if (!Q_stricmp(token, "{")) {
                        TIKI_ParseAnimationCommands(ld, anim, ld->tikiFile.Filename());
                    } else {
                        ld->tikiFile.UnGetToken();
                    }
                }
            } else {
                TIKI_ParseAnimationsFail(ld);
            }
        }
    }
}

int TIKI_ParseSurfaceFlag(const char *token)
{
    int flags = 0;

    if (!Q_stricmp(token, "skin1")) {
        flags = TIKI_SURF_SKIN1;
    } else if (!Q_stricmp(token, "skin2")) {
        flags = TIKI_SURF_SKIN2;
    } else if (!Q_stricmp(token, "skin3")) {
        flags = TIKI_SURF_SKIN3;
    } else if (!Q_stricmp(token, "nodraw")) {
        flags = TIKI_SURF_NODRAW;
    } else if (!Q_stricmp(token, "nodamage")) {
        flags = TIKI_SURF_NODAMAGE;
    } else if (!Q_stricmp(token, "crossfade")) {
        flags = TIKI_SURF_CROSSFADE;
    } else if (!Q_stricmp(token, "nomipmaps")) {
        flags = TIKI_SURF_NOMIPMAPS;
    } else if (!Q_stricmp(token, "nopicmip")) {
        flags = TIKI_SURF_NOPICMIP;
    } else if (!Q_stricmp(token, "nocustomshader")) {
        flags = TIKI_SURF_REPLACED;
    } else {
        TIKI_Error("Unknown surface flag %s\n", token);
    }

    return flags;
}

static void WriteScale(dloaddef_t *ld, float scale)
{
    MSG_WriteByte(ld->modelBuf, SETUP_LOAD_SCALE);
    MSG_WriteFloat(ld->modelBuf, scale);
}

static void WriteLoadScale(dloaddef_t *ld, float lod_scale)
{
    MSG_WriteByte(ld->modelBuf, SETUP_LOD_SCALE);
    MSG_WriteFloat(ld->modelBuf, lod_scale);
}

static void WriteLodBias(dloaddef_t *ld, float lod_bias)
{
    MSG_WriteByte(ld->modelBuf, SETUP_LOD_BIAS);
    MSG_WriteFloat(ld->modelBuf, lod_bias);
}

void WriteSkelmodel(dloaddef_t *ld, const char *name)
{
    MSG_WriteByte(ld->modelBuf, SETUP_SKELMODEL);
    MSG_WriteString(ld->modelBuf, name);

    Q_strncpyz(ld->idleSkel, name, sizeof(ld->idleSkel));
    ld->numskels++;
}

static void WriteMorphfile(dloaddef_t *ld, const char *name)
{
    MSG_WriteByte(ld->modelBuf, SETUP_MORPHFILE);
    MSG_WriteString(ld->modelBuf, name);
}

static void WriteReplaceSurface(
    dloaddef_t *ld, const char *oldsurface, const char *newsurface, const char *skbfile, const char *morphfile
)
{
    MSG_WriteByte(ld->modelBuf, SETUP_REPLACESURFACE);
    MSG_WriteString(ld->modelBuf, oldsurface);
    MSG_WriteString(ld->modelBuf, newsurface);
    MSG_WriteString(ld->modelBuf, skbfile);
    MSG_WriteString(ld->modelBuf, morphfile ? morphfile : "");
}

static void WriteOrigin(dloaddef_t *ld, float origin_x, float origin_y, float origin_z)
{
    MSG_WriteByte(ld->modelBuf, SETUP_LOAD_ORIGIN);
    MSG_WriteFloat(ld->modelBuf, origin_x);
    MSG_WriteFloat(ld->modelBuf, origin_y);
    MSG_WriteFloat(ld->modelBuf, origin_z);
}

static void WriteLightOffset(dloaddef_t *ld, float light_offset_x, float light_offset_y, float light_offset_z)
{
    MSG_WriteByte(ld->modelBuf, SETUP_LIGHT_OFFSET);
    MSG_WriteFloat(ld->modelBuf, light_offset_x);
    MSG_WriteFloat(ld->modelBuf, light_offset_y);
    MSG_WriteFloat(ld->modelBuf, light_offset_z);
}

static void WriteRadius(dloaddef_t *ld, float radius)
{
    MSG_WriteByte(ld->modelBuf, SETUP_RADIUS);
    MSG_WriteFloat(ld->modelBuf, radius);
}

static void WriteLod(dloaddef_t *ld, float start_dist, float start_pct, float stop_dist, float stop_pct)
{
    MSG_WriteByte(ld->modelBuf, SETUP_LOD);
    MSG_WriteFloat(ld->modelBuf, start_dist < 0.0f ? 0.0f : start_dist);
    MSG_WriteFloat(ld->modelBuf, start_pct > 1.0f ? 1.0f : (start_pct < 0.0f ? 0.0f : start_pct));
    MSG_WriteFloat(ld->modelBuf, stop_dist < 0.0f ? 0.0f : stop_dist);
    MSG_WriteFloat(ld->modelBuf, stop_pct > 1.0f ? 1.0f : (stop_pct < 0.0f ? 0.0f : stop_pct));
}

static void WriteFadeDistMod(dloaddef_t *ld, float fade_dist_mod)
{
    MSG_WriteByte(ld->modelBuf, SETUP_FADE_DIST_MOD);
    MSG_WriteFloat(ld->modelBuf, fade_dist_mod);
}

static void WriteBoundingVolume(dloaddef_t *ld, qboolean isMax, const vec3_t v)
{
    MSG_WriteByte(ld->modelBuf, isMax ? SETUP_BOUNDING_MAX : SETUP_BOUNDING_MIN);
    MSG_WriteFloat(ld->modelBuf, v[0]);
    MSG_WriteFloat(ld->modelBuf, v[1]);
    MSG_WriteFloat(ld->modelBuf, v[2]);
}

static void WriteHeadOffset(dloaddef_t *ld, float x, float y, float z)
{
    MSG_WriteByte(ld->modelBuf, SETUP_HEAD_OFFSET);
    MSG_WriteFloat(ld->modelBuf, x);
    MSG_WriteFloat(ld->modelBuf, y);
    MSG_WriteFloat(ld->modelBuf, z);
}

static void WriteLoadAllAnims(dloaddef_t *ld)
{
    MSG_WriteByte(ld->modelBuf, SETUP_LOAD_ALL_ANIMS);
}

static void WriteSurface(dloaddef_t *ld, const char *surface)
{
    MSG_WriteByte(ld->modelBuf, SETUP_SURFACE);
    MSG_WriteString(ld->modelBuf, surface);
}

static void WriteFlags(dloaddef_t *ld, int flags)
{
    MSG_WriteByte(ld->modelBuf, SETUP_FLAGS);
    MSG_WriteLong(ld->modelBuf, flags);
}

static void WriteDamage(dloaddef_t *ld, float damage)
{
    MSG_WriteByte(ld->modelBuf, SETUP_DAMAGE);
    MSG_WriteFloat(ld->modelBuf, damage);
}

static void WriteShader(dloaddef_t *ld, const char *shader)
{
    MSG_WriteByte(ld->modelBuf, SETUP_SHADER);
    MSG_WriteString(ld->modelBuf, shader);
}

void TIKI_InitSetup(dloaddef_t *ld)
{
    MSG_Init(ld->modelBuf, ld->modelData, 8192);
}

qboolean TIKI_LoadSetupCase(
    dtiki_t        *tiki,
    const char     *filename,
    dloadsurface_t *loadsurfaces,
    int            *numSurfacesSetUp,
    msg_t          *msg
)
{
    int               c;
    const char       *name;
    int               currentSurface = -1;
    int               mesh;
    skelHeaderGame_t *skelmodel;
    float             load_scale;
    float             lod_scale;
    float             lod_bias;
    float             load_origin[3];
    float             light_offset[3];
    float             radius;
    int               flags;
    float             damage_multiplier;

    while (1) {
        c = MSG_ReadByte(msg);
        switch (c) {
        default:
            break;
        case SETUP_LOAD_SCALE:
            load_scale = MSG_ReadFloat(msg);
            tiki->load_scale = load_scale;
            break;
        case SETUP_LOD_SCALE:
            lod_scale = MSG_ReadFloat(msg);
            tiki->lod_scale = lod_scale;
            break;
        case SETUP_LOD_BIAS:
            lod_bias = MSG_ReadFloat(msg);
            tiki->lod_bias = lod_bias;
            break;
        case SETUP_SKELMODEL:
            name = MSG_ReadString(msg);

            if (tiki->numMeshes >= MAX_SKELMODELS) {
                TIKI_Error("^~^~^ TIKI_LoadSetup: too many skelmodels in %s.\n", filename);
                return false;
            }

            mesh = TIKI_RegisterSkel(name, tiki);
            if (mesh < 0) {
                return false;
            }

            tiki->mesh[tiki->numMeshes] = mesh;
            skelmodel                   = TIKI_GetSkel(mesh);
            if (strcmp(TIKI_FileExtension(name), "tan") || tiki->numMeshes == 0) {
                tiki->num_surfaces += skelmodel->numSurfaces;
                tiki->numMeshes++;
            }
            break;
        case SETUP_MORPHFILE:
            name = MSG_ReadString(msg);
            if (tiki->numMeshes > 0) {
                skelmodel = TIKI_GetSkel(tiki->mesh[tiki->numMeshes - 1]);
                if (skelmodel) {
                    TIKI_LoadSKBMorph(skelmodel, name);
                }
            }
            break;
        case SETUP_REPLACESURFACE: {
            // does NOT hide the old surface.  Retail resolves <skbfile> in the skel cache
            char oldsurface[64];
            char newsurface[64];
            char skbfile[128];
            char morphfile[128];
            int  replOldSurface;

            Q_strncpyz(oldsurface, MSG_ReadString(msg), sizeof(oldsurface));
            Q_strncpyz(newsurface, MSG_ReadString(msg), sizeof(newsurface));
            Q_strncpyz(skbfile, MSG_ReadString(msg), sizeof(skbfile));
            Q_strncpyz(morphfile, MSG_ReadString(msg), sizeof(morphfile));

            for (replOldSurface = 0; replOldSurface < *numSurfacesSetUp; replOldSurface++) {
                if (!Q_stricmp(loadsurfaces[replOldSurface].name, oldsurface)) {
                    break;
                }
            }

            if (replOldSurface == *numSurfacesSetUp) {
                if (*numSurfacesSetUp >= MAX_MODEL_SURFACES) {
                    TIKI_Error(
                        "TIKI_ParseSetup: Too many surfaces (max %d) in %s.\n", MAX_MODEL_SURFACES, filename
                    );
                    break;
                }
                loadsurfaces[replOldSurface].flags = 0;
                (*numSurfacesSetUp)++;
            }

            Q_strncpyz(loadsurfaces[replOldSurface].name, oldsurface, sizeof(loadsurfaces[replOldSurface].name));
            Q_strncpyz(loadsurfaces[replOldSurface].replSkel, skbfile, sizeof(loadsurfaces[replOldSurface].replSkel));
            Q_strncpyz(
                loadsurfaces[replOldSurface].replMorph, morphfile, sizeof(loadsurfaces[replOldSurface].replMorph)
            );
            Q_strncpyz(
                loadsurfaces[replOldSurface].replSurf,
                newsurface[0] ? newsurface : oldsurface,
                sizeof(loadsurfaces[replOldSurface].replSurf)
            );
            break;
        }
        case SETUP_LOAD_ORIGIN:
            load_origin[0] = MSG_ReadFloat(msg);
            load_origin[1] = MSG_ReadFloat(msg);
            load_origin[2] = MSG_ReadFloat(msg);
            VectorCopy(load_origin, tiki->load_origin);
            break;
        case SETUP_LIGHT_OFFSET:
            light_offset[0] = MSG_ReadFloat(msg);
            light_offset[1] = MSG_ReadFloat(msg);
            light_offset[2] = MSG_ReadFloat(msg);
            VectorCopy(light_offset, tiki->light_offset);
            break;
        case SETUP_LOD:
            tiki->lod_start_distance     = MSG_ReadFloat(msg);
            tiki->lod_start_percent_verts = MSG_ReadFloat(msg);
            tiki->lod_stop_distance      = MSG_ReadFloat(msg);
            tiki->lod_stop_percent_verts = MSG_ReadFloat(msg);
            break;
        case SETUP_FADE_DIST_MOD:
            tiki->fade_dist_mod = MSG_ReadFloat(msg);
            break;
        case SETUP_BOUNDING_MIN:
            tiki->boundingVolumeMin[0] = MSG_ReadFloat(msg);
            tiki->boundingVolumeMin[1] = MSG_ReadFloat(msg);
            tiki->boundingVolumeMin[2] = MSG_ReadFloat(msg);
            break;
        case SETUP_BOUNDING_MAX:
            tiki->boundingVolumeMax[0] = MSG_ReadFloat(msg);
            tiki->boundingVolumeMax[1] = MSG_ReadFloat(msg);
            tiki->boundingVolumeMax[2] = MSG_ReadFloat(msg);
            break;
        case SETUP_HEAD_OFFSET:
            tiki->head_offset[0] = MSG_ReadFloat(msg);
            tiki->head_offset[1] = MSG_ReadFloat(msg);
            tiki->head_offset[2] = MSG_ReadFloat(msg);
            break;
        case SETUP_LOAD_ALL_ANIMS:
            tiki->_loadAllAnims = qtrue;
            break;
        case SETUP_RADIUS:
            radius = MSG_ReadFloat(msg);
            tiki->radius = radius;
            break;
        case SETUP_SURFACE:
            name = MSG_ReadString(msg);

            for (currentSurface = 0; currentSurface < *numSurfacesSetUp; currentSurface++) {
                if (!Q_stricmp(loadsurfaces[currentSurface].name, name)) {
                    break;
                }
            }

            if (currentSurface == *numSurfacesSetUp) {
                if (*numSurfacesSetUp >= MAX_MODEL_SURFACES) {
                    TIKI_Error(
                        "TIKI_ParseSetup: Too many surfaces (max %d) in %s.\n", MAX_MODEL_SURFACES, filename
                    );
                    break;
                }
                loadsurfaces[currentSurface].flags = 0;
                (*numSurfacesSetUp)++;
            }
            Q_strncpyz(loadsurfaces[currentSurface].name, name, sizeof(loadsurfaces[currentSurface].name));
            break;
        case SETUP_FLAGS:
            flags = MSG_ReadLong(msg);
            if (currentSurface >= 0 && currentSurface < MAX_MODEL_SURFACES) {
                loadsurfaces[currentSurface].flags |= flags;
            }
            break;
        case SETUP_DAMAGE:
            damage_multiplier = MSG_ReadFloat(msg);
            if (currentSurface >= 0 && currentSurface < MAX_MODEL_SURFACES) {
                loadsurfaces[currentSurface].damage_multiplier = damage_multiplier;
            }
            break;
        case SETUP_SHADER:
            name = MSG_ReadString(msg);

            if (currentSurface < 0 || currentSurface >= MAX_MODEL_SURFACES) {
                break;
            }

            if (loadsurfaces[currentSurface].numskins > 3) {
                TIKI_Error(
                    "TIKI_ParseSetup: Too many skins defined for surface %s in %s.\n",
                    loadsurfaces[currentSurface].name,
                    filename
                );
                break;
            }

            strncpy(
                loadsurfaces[currentSurface].shader[loadsurfaces[currentSurface].numskins],
                name,
                sizeof(loadsurfaces[currentSurface].shader[loadsurfaces[currentSurface].numskins])
            );
            loadsurfaces[currentSurface].numskins++;
            break;
        case SETUP_END_MESSAGE:
            return true;
        }
    }
}

qboolean TIKI_LoadSetup(
    dtiki_t        *tiki,
    const char     *filename,
    dloadsurface_t *loadsurfaces,
    int            *numSurfacesSetUp,
    byte           *modelData,
    size_t          modelDataSize
)
{
    msg_t msg;

    MSG_Init(&msg, modelData, modelDataSize);
    msg.cursize = modelDataSize;
    MSG_BeginReading(&msg);

    memset(tiki, 0, sizeof(dtiki_t));
    memset(loadsurfaces, 0, sizeof(dloadsurface_t) * MAX_MODEL_SURFACES);
    *numSurfacesSetUp = 0;

    VectorCopy(vec3_origin, tiki->load_origin);
    VectorCopy(vec3_origin, tiki->light_offset);
    tiki->load_scale = 1.0f;
    tiki->lod_scale  = 1.0f;
    tiki->lod_bias   = 0.0f;
    tiki->numMeshes  = 0;

    return TIKI_LoadSetupCase(tiki, filename, loadsurfaces, numSurfacesSetUp, &msg);
}

qboolean TIKI_ParseSetup(dloaddef_t *ld)
{
    const char *token;
    float       load_scale;
    float       lod_scale;
    float       lod_bias;
    float       tmpFloat;
    float       tmpVec[3];
    int         tmpInt;
    size_t      length;
    char        name[128];

    ld->tikiFile.GetToken(true);

    while (ld->tikiFile.TokenAvailable(true)) {
        token = ld->tikiFile.GetToken(true);

        if (!Q_stricmp(token, "scale")) {
            load_scale = ld->tikiFile.GetFloat(false);
            if (load_scale <= 0.0f) {
                load_scale = 1.0f;
            }
            WriteScale(ld, load_scale);
        } else if (!Q_stricmp(token, "lod_scale")) {
            lod_scale = ld->tikiFile.GetFloat(false) / 5.0f;
            WriteLoadScale(ld, lod_scale);
        } else if (!Q_stricmp(token, "lod_bias")) {
            lod_bias = ld->tikiFile.GetFloat(false);
            WriteLodBias(ld, lod_bias);
        } else if (!Q_stricmp(token, "skelmodel")) {
            // Because the token is not consumed, retail's next outer-loop iteration
            if (ld->bSkelModelParsed) {
                TIKI_Error("TIKI_ParseSetup: skelmodel called twice on %s, skipping line.\n", ld->path);
            } else {
                token = ld->tikiFile.GetToken(false);
                Q_strncpyz(name, ld->tikiFile.currentScript->path, sizeof(name));
                Q_strcat(name, sizeof(name), token);
                ld->bSkelModelParsed = qtrue;
                WriteSkelmodel(ld, name);
            }
        } else if (!Q_stricmp(token, "path")) {
            token = ld->tikiFile.GetToken(false);
            Q_strncpyz(ld->tikiFile.currentScript->path, token, sizeof(ld->tikiFile.currentScript->path));
            length = strlen(ld->tikiFile.currentScript->path);
            token  = ld->tikiFile.currentScript->path + length - 1;

            if (*token != '/' && *token != '\\') {
                Q_strcat(ld->tikiFile.currentScript->path, sizeof(ld->tikiFile.currentScript->path), "/");
            }
        } else if (!Q_stricmp(token, "origin")) {
            tmpVec[0] = ld->tikiFile.GetFloat(false);
            tmpVec[1] = ld->tikiFile.GetFloat(false);
            tmpVec[2] = ld->tikiFile.GetFloat(false);
            WriteOrigin(ld, tmpVec[0], tmpVec[1], tmpVec[2]);
        } else if (!Q_stricmp(token, "lightoffset")) {
            tmpVec[0] = ld->tikiFile.GetFloat(false);
            tmpVec[1] = ld->tikiFile.GetFloat(false);
            tmpVec[2] = ld->tikiFile.GetFloat(false);
            WriteLightOffset(ld, tmpVec[0], tmpVec[1], tmpVec[2]);
        } else if (!Q_stricmp(token, "radius")) {
            tmpFloat = ld->tikiFile.GetFloat(false);
            WriteRadius(ld, tmpFloat);
        } else if (!Q_stricmp(token, "surface")) {
            char surfaceName[64];

            token = ld->tikiFile.GetToken(false);
            Q_strncpyz(surfaceName, token, sizeof(surfaceName));
            WriteSurface(ld, token);
            while (ld->tikiFile.TokenAvailable(false)) {
                token = ld->tikiFile.GetToken(false);
                if (!Q_stricmp(token, "flags")) {
                    token  = ld->tikiFile.GetToken(false);
                    tmpInt = TIKI_ParseSurfaceFlag(token);
                    WriteFlags(ld, tmpInt);
                } else if (!Q_stricmp(token, "damage")) {
                    tmpFloat = ld->tikiFile.GetFloat(false);
                    WriteDamage(ld, tmpFloat);
                } else if (!Q_stricmp(token, "shader")) {
                    token                   = ld->tikiFile.GetToken(false);
                    const char *texturepath = ld->tikiFile.currentScript->texturepath;
                    if (texturepath[0] && strstr(token, ".") && !strchr(token, '/') && !strchr(token, '\\')) {
                        Q_strncpyz(name, texturepath, sizeof(name));
                        Q_strcat(name, sizeof(name), token);
                        WriteShader(ld, name);
                    } else if (strstr(token, ".")) {
                        Q_strncpyz(name, ld->tikiFile.currentScript->path, sizeof(name));
                        Q_strcat(name, sizeof(name), token);
                        WriteShader(ld, name);
                    } else {
                        WriteShader(ld, token);
                    }
                } else {
                    TIKI_Error(
                        "TIKI_ParseSetup: unknown shader command %s in %s for surface %s on line %d, skipping "
                        "line.\n",
                        token,
                        ld->tikiFile.Filename(),
                        surfaceName,
                        ld->tikiFile.GetLineNumber()
                    );
                }
            }
        } else if (!Q_stricmp(token, "texturepath")) {
            size_t tplen;
            token = ld->tikiFile.GetToken(false);
            Q_strncpyz(ld->tikiFile.currentScript->texturepath, token, sizeof(ld->tikiFile.currentScript->texturepath));
            tplen = strlen(ld->tikiFile.currentScript->texturepath);
            if (tplen && ld->tikiFile.currentScript->texturepath[tplen - 1] != '/'
                && ld->tikiFile.currentScript->texturepath[tplen - 1] != '\\') {
                Q_strcat(
                    ld->tikiFile.currentScript->texturepath,
                    sizeof(ld->tikiFile.currentScript->texturepath),
                    "/"
                );
            }
        } else if (!Q_stricmp(token, "morphfile")) {
            token = ld->tikiFile.GetToken(false);
            Q_strncpyz(name, ld->tikiFile.currentScript->path, sizeof(name));
            Q_strcat(name, sizeof(name), token);
            WriteMorphfile(ld, name);
        } else if (!Q_stricmp(token, "replacesurface")) {
            char oldsurface[64];
            char newsurface[64];
            char skbfile[128];
            char morphfile[128];

            Q_strncpyz(oldsurface, ld->tikiFile.GetToken(false), sizeof(oldsurface));
            Q_strncpyz(newsurface, ld->tikiFile.GetToken(false), sizeof(newsurface));

            token = ld->tikiFile.GetToken(false);
            if (strchr(token, '/') || strchr(token, '\\')) {
                Q_strncpyz(skbfile, token, sizeof(skbfile));
            } else {
                Q_strncpyz(skbfile, ld->tikiFile.currentScript->path, sizeof(skbfile));
                Q_strcat(skbfile, sizeof(skbfile), token);
            }

            morphfile[0] = '\0';
            if (ld->tikiFile.TokenAvailable(false)) {
                token = ld->tikiFile.GetToken(false);
                if (strchr(token, '/') || strchr(token, '\\')) {
                    Q_strncpyz(morphfile, token, sizeof(morphfile));
                } else {
                    Q_strncpyz(morphfile, ld->tikiFile.currentScript->path, sizeof(morphfile));
                    Q_strcat(morphfile, sizeof(morphfile), token);
                }
            }

            WriteReplaceSurface(ld, oldsurface, newsurface, skbfile, morphfile);
        } else if (!Q_stricmp(token, "bonemapping")) {
            if (ld->numbonemappings < MAX_TIKI_LOAD_BONE_MAPPINGS) {
                dloadbonemapping_t *mapping = &ld->loadbonemappings[ld->numbonemappings];

                Q_strncpyz(mapping->bone, ld->tikiFile.GetToken(false), sizeof(mapping->bone));
                Q_strncpyz(mapping->surface, ld->tikiFile.GetToken(false), sizeof(mapping->surface));
                mapping->radius = (float)atof(ld->tikiFile.GetToken(false));

                if (ld->tikiFile.TokenAvailable(false)) {
                    mapping->length = (float)atof(ld->tikiFile.GetToken(false));
                } else {
                    mapping->length = 0.0f;
                }

                ld->numbonemappings++;
            } else {
                TIKI_Error(
                    "TIKI_ParseSetup: too many bonemappings (max %d) in '%s' on line %d.\n",
                    MAX_TIKI_LOAD_BONE_MAPPINGS,
                    ld->tikiFile.Filename(),
                    ld->tikiFile.GetLineNumber()
                );

                while (ld->tikiFile.TokenAvailable(false)) {
                    ld->tikiFile.GetToken(false);
                }
            }
        } else if (!Q_stricmp(token, "lod")) {
            float lod_start_distance     = atof(ld->tikiFile.GetToken(false));
            float lod_start_percent_verts = atof(ld->tikiFile.GetToken(false));
            float lod_stop_distance      = atof(ld->tikiFile.GetToken(false));
            float lod_stop_percent_verts = atof(ld->tikiFile.GetToken(false));
            ld->tikiFile.GetToken(false);

            if (lod_start_distance < 0.0f) {
                lod_start_distance = 0.0f;
            }
            if (lod_start_percent_verts > 1.0f) {
                lod_start_percent_verts = 1.0f;
            }
            if (lod_start_percent_verts < 0.0f) {
                lod_start_percent_verts = 0.0f;
            }
            if (lod_stop_distance < 0.0f) {
                lod_stop_distance = 0.0f;
            }
            if (lod_stop_percent_verts > 1.0f) {
                lod_stop_percent_verts = 1.0f;
            }
            if (lod_stop_percent_verts < 0.0f) {
                lod_stop_percent_verts = 0.0f;
            }

            WriteLod(
                ld,
                lod_start_distance,
                lod_start_percent_verts,
                lod_stop_distance,
                lod_stop_percent_verts
            );
        } else if (!Q_stricmp(token, "fade_dist_mod")) {
            WriteFadeDistMod(ld, (float)atof(ld->tikiFile.GetToken(false)));
        } else if (!Q_stricmp(token, "headoffset")) {
            float hx = (float)atof(ld->tikiFile.GetToken(false));
            float hy = (float)atof(ld->tikiFile.GetToken(false));
            float hz = (float)atof(ld->tikiFile.GetToken(false));
            WriteHeadOffset(ld, hx, hy, hz);
        } else if (!Q_stricmp(token, "boundingvolumemin")) {
            vec3_t bmin;
            ld->tikiFile.GetVector(qfalse, bmin);
            WriteBoundingVolume(ld, qfalse, bmin);
        } else if (!Q_stricmp(token, "boundingvolumemax")) {
            vec3_t bmax;
            ld->tikiFile.GetVector(qfalse, bmax);
            WriteBoundingVolume(ld, qtrue, bmax);
        } else if (!Q_stricmp(token, "loadallanims") || !Q_stricmp(token, "loadAllAnims")) {
            ld->bLoadAllAnims = qtrue;
            WriteLoadAllAnims(ld);
        } else if (!Q_stricmp(token, "}")) {
            return true;
        } else {
            // operands in single quotes, which retail's literal does not have.  Emitted
            TIKI_Error(
                "TIKI_ParseSetup: unknown setup command %s in %s on line %d, skipping line.\n",
                token,
                ld->tikiFile.Filename(),
                ld->tikiFile.GetLineNumber()
            );
            while (ld->tikiFile.TokenAvailable(false)) {
                ld->tikiFile.GetToken(false);
            }
        }
    }

    return true;
}

void TIKI_ParseInitCommands(dloaddef_t *ld, dloadinitcmd_t **cmdlist, int maxcmds, int *numcmds)
{
    int             i;
    const char     *token;
    char           *pszArgs[TIKI_CMD_MAX_ARGS];
    dloadinitcmd_t *cmd;

    ld->tikiFile.GetToken(true);

    while (ld->tikiFile.TokenAvailable(true)) {
        token = ld->tikiFile.GetToken(true);

        if (!Q_stricmp(token, "}")) {
            break;
        }

        if (*numcmds >= maxcmds) {
            TIKI_Error(
                "TIKI_ParseInitCommands: too many init commands in %s on line %d.\n",
                ld->tikiFile.Filename(),
                ld->tikiFile.GetLineNumber()
            );

            while (ld->tikiFile.TokenAvailable(false)) {
                ld->tikiFile.GetToken(false);
            }
        } else {
            cmd               = (dloadinitcmd_t *)TIKI_AllocateLoadData(sizeof(dloadinitcmd_t));
            cmdlist[*numcmds] = cmd;
            if (cmd) {
                (*numcmds)++;

                cmd->num_args = 0;
                cmd->args     = NULL;

                ld->tikiFile.UnGetToken();
                while (ld->tikiFile.TokenAvailable(false)) {
                    token = ld->tikiFile.GetToken(false);
                    if (cmd->num_args < TIKI_CMD_MAX_ARGS) {
                        pszArgs[cmd->num_args] = TIKI_CopyString(token);
                        cmd->num_args++;
                    } else {
                        TIKI_Error(
                            "TIKI_ParseInitCommands: too many args in anim commands in %s.\n", ld->tikiFile.Filename()
                        );
                    }
                }

                cmd->args = (char **)TIKI_AllocateLoadData(cmd->num_args * sizeof(char *));
                for (i = 0; i < cmd->num_args; i++) {
                    cmd->args[i] = pszArgs[i];
                }
            } else {
                TIKI_Error(
                    "TIKI_ParseInitCommands: could not allocate storage for dloadinitcmd_t in %s on line %d.\n",
                    ld->tikiFile.Filename(),
                    ld->tikiFile.GetLineNumber()
                );
            }
        }
    }
}

void TIKI_ParseInit(dloaddef_t *ld)
{
    const char *token;

    ld->tikiFile.GetToken(true);

    while (ld->tikiFile.TokenAvailable(true)) {
        token = ld->tikiFile.GetToken(true);
        if (!Q_stricmp(token, "client")) {
            TIKI_ParseInitCommands(
                ld, ld->loadclientinitcmds, MAX_TIKI_LOAD_CLIENT_INIT_COMMANDS, &ld->numclientinitcmds
            );
        } else if (!Q_stricmp(token, "server")) {
            TIKI_ParseInitCommands(
                ld, ld->loadserverinitcmds, MAX_TIKI_LOAD_SERVER_INIT_COMMANDS, &ld->numserverinitcmds
            );
        } else if (!Q_stricmp(token, "}")) {
            break;
        } else {
            TIKI_Error(
                "TIKI_ParseInit: unknown init command %s in %s on line %d, skipping line.\n",
                token,
                ld->tikiFile.Filename(),
                ld->tikiFile.GetLineNumber()
            );

            while (ld->tikiFile.TokenAvailable(false)) {
                ld->tikiFile.GetToken(false);
            }
        }
    }
}
