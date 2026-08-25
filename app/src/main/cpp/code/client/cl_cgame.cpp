
#include "client.h"
#include "cl_ui.h"
#define PROBE_GET(name, def, flags) Cvar_Get(name, def, flags)
#define PROBE_PRINT                 Com_Printf
#include "../qcommon/qa_probe.h"
#include "cg_import_verify_gen.h"
#include "../corepp/tiki.h"
#include "../corepp/script.h"
#include "../qcommon/gameplaymanager.h"
#include "../qcommon/stringresource.hpp"

extern "C" {
    int         GetNumViewModes(void);
    int         GetViewModeEntityColor(unsigned int viewModeMask, float *color);
    int         GetViewModeEntityShader(unsigned int viewModeMask, const char **shader);
    int         GetViewModeEntityAlpha(unsigned int viewModeMask, float *alpha);
    int         GetViewModeHud(unsigned int viewModeMask, const char **hudName);
    int         GetViewModeUseFullbright(unsigned int viewModeMask);
    void        PF_GetLevelDefs(const char *name, const char **environmentName,
                                const char **levelName, const char **sublevelName);
    qboolean    PF_areSublevels(const char *oldMapName, const char *newMapName);
}

#include "../qcommon/stringresource.h" 

extern qboolean loadCamera(const char *name);
extern void startCamera(int time);
extern qboolean getCameraInfo(int time, vec3_t *origin, vec3_t *angles);

void CL_GetGameState( gameState_t *gs ) {
	*gs = cl.gameState;
}

// glConfig`.  Retail moves this struct wholesale everywhere; it never enumerates
void CL_GetGlconfig( glconfig_t *glconfig ) {
	*glconfig = cls.glconfig;
}


qboolean CL_GetUserCmd( int cmdNumber, usercmd_t *ucmd ) {

	if ( cmdNumber > cl.cmdNumber ) {
		Com_Error( ERR_DROP, "CL_GetUserCmd: %i >= %i", cmdNumber, cl.cmdNumber );
	}

	if ( cmdNumber <= cl.cmdNumber - CMD_BACKUP ) {
		return qfalse;
	}

	*ucmd = cl.cmds[ cmdNumber & CMD_MASK ];

	return qtrue;
}

int CL_GetCurrentCmdNumber( void ) {
	return cl.cmdNumber;
}


qboolean	CL_GetParseEntityState( int parseEntityNumber, entityState_t *state ) {
	if ( parseEntityNumber >= cl.parseEntitiesNum ) {
		Com_Error( ERR_DROP, "CL_GetParseEntityState: %i >= %i",
			parseEntityNumber, cl.parseEntitiesNum );
	}

	if ( parseEntityNumber <= cl.parseEntitiesNum - MAX_PARSE_ENTITIES ) {
		return qfalse;
	}

	*state = cl.parseEntities[ parseEntityNumber & ( MAX_PARSE_ENTITIES - 1 ) ];
	return qtrue;
}

void	CL_GetCurrentSnapshotNumber( int *snapshotNumber, int *serverTime ) {
	*snapshotNumber = cl.snap.messageNum;
	*serverTime = cl.snap.serverTime;
}

void CL_SetTime( int time ) {
	cl.serverTime = time;
}

qboolean	CL_GetSnapshot( int snapshotNumber, snapshot_t *snapshot ) {
	clSnapshot_t	*clSnap;
	int				i, count;
	int				pnum;
	int				parents[MAX_ENTITIES_IN_SNAPSHOT];
	entityState_t	*s1;
	int				pcount = 0;

	if ( snapshotNumber > cl.snap.messageNum ) {
		Com_Error( ERR_DROP, "CL_GetSnapshot: snapshotNumber > cl.snap.messageNum" );
	}

	if ( cl.snap.messageNum - snapshotNumber >= PACKET_BACKUP ) {
		return qfalse;
	}

	clSnap = &cl.snapshots[snapshotNumber & PACKET_MASK];
	if ( !clSnap->valid ) {
		return qfalse;
	}

	if ( ( clSnap->snapFlags ^ cl.snap.snapFlags ) & SNAPFLAG_SERVERCOUNT ) {
		return qfalse;
	}

	if ( cl.parseEntitiesNum - clSnap->parseEntitiesNum >= MAX_PARSE_ENTITIES ) {
		return qfalse;
	}

	snapshot->snapFlags = clSnap->snapFlags;
	snapshot->serverCommandSequence = clSnap->serverCommandNum;
	snapshot->ping = clSnap->ping;
	snapshot->serverTime = clSnap->serverTime;
	Com_Memcpy( snapshot->areamask, clSnap->areamask, sizeof( snapshot->areamask ) );
	snapshot->ps = clSnap->ps;

	snapshot->number_of_sounds = clSnap->number_of_sounds;
	Com_Memcpy(snapshot->sounds, clSnap->sounds, sizeof(snapshot->sounds));

	Com_Memcpy( &snapshot->ws, &clSnap->ws, sizeof( snapshot->ws ) );

	count = clSnap->numEntities;
	if ( count > MAX_ENTITIES_IN_SNAPSHOT ) {
		Com_DPrintf( "CL_GetSnapshot: truncated %i visible entities to %i\n", count, MAX_ENTITIES_IN_SNAPSHOT );
		count = MAX_ENTITIES_IN_SNAPSHOT;
	}

	snapshot->numEntities = 0;
	for(i = 0; i < MAX_ENTITIES_IN_SNAPSHOT; i++) {
		parents[i] = -1;
	}

	for (i = 0; i < count; i++) {
		s1 = &cl.parseEntities[(clSnap->parseEntitiesNum + i) & (MAX_PARSE_ENTITIES - 1)];
		pnum = s1->parent;

		if (pnum == ENTITYNUM_NONE) {
			parents[s1->number] = -2;
		} else {
			if (parents[pnum] == -2) {
				parents[s1->number] = -2;
			} else {
				parents[s1->number] = pnum;
				continue;
			}
		}

		snapshot->entities[snapshot->numEntities++] = *s1;
	}

	for(pcount = 0; pcount < 8 && snapshot->numEntities != count; pcount++) {
		for (i = 0; i < count; i++) {
			s1 = &cl.parseEntities[(clSnap->parseEntitiesNum + i) & (MAX_PARSE_ENTITIES - 1)];
			pnum = parents[s1->number];
			if (pnum >= 0 && parents[pnum] == -2) {
				parents[s1->number] = -2;
				snapshot->entities[snapshot->numEntities++] = *s1;
			}
		}
	}

	if (snapshot->numEntities != count) {
		Com_DPrintf("CL_GetSnapshot: Not all children could find their parents.\n");

		for (i = snapshot->numEntities; i < count; i++)
		{
			for (pnum = 0; pnum < count; pnum++) {
				s1 = &cl.parseEntities[(clSnap->parseEntitiesNum + pnum) & (MAX_PARSE_ENTITIES - 1)];
				if (parents[s1->number] >= 0) {
					Com_DPrintf(
						"CL_GetSnapshot: entity %d with parent %d and model '%s' at %.2f %.2f %.2f, could not find parent.\n",
						s1->number,
						s1->parent,
						CL_ConfigString(CS_MODELS + s1->modelindex),
						s1->origin[0],
						s1->origin[1],
						s1->origin[2]
					);

					parents[s1->number] = -2;
				}
			}
		}
	}

	return qtrue;
}

void CL_AddCgameCommand( const char *cmdName ) {
	Cmd_AddCommand( cmdName, NULL );
}

void CL_CgameError( const char *string ) {
	// Port-only MOHAA residue: the Mac symbol table has no CL_CgameError and
	Com_Error( ERR_DROP, "%s", string );
}


void CL_ConfigstringModified( void ) {
	char		*old;
	int			i;
	char		*dup;
	gameState_t	oldGs;
	size_t		len;
	int			index;
	char		*s;

	index = atoi( Cmd_Argv( 1 ) );
	s     = Cmd_Argv( 2 );

	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		Com_Error( ERR_DROP, "configstring > MAX_CONFIGSTRINGS" );
	}

	old = cl.gameState.stringData + cl.gameState.stringOffsets[ index ];
	if ( !strcmp( old, s ) ) {
		return;		
	}

	oldGs = cl.gameState;

	Com_Memset( &cl.gameState, 0, sizeof( cl.gameState ) );

	cl.gameState.dataCount = 1;

	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		if ( i == index ) {
			dup = s;
		} else {
			dup = oldGs.stringData + oldGs.stringOffsets[ i ];
		}
		if ( !dup[0] ) {
			continue;		
		}

		len = strlen( dup );

		if ( len + 1 + cl.gameState.dataCount > MAX_GAMESTATE_CHARS ) {
			Com_Error( ERR_DROP, "MAX_GAMESTATE_CHARS exceeded" );
		}

		cl.gameState.stringOffsets[ i ] = cl.gameState.dataCount;
		Com_Memcpy( cl.gameState.stringData + cl.gameState.dataCount, dup, len + 1 );
		cl.gameState.dataCount += len + 1;
	}

	if ( index == CS_SYSTEMINFO ) {
		CL_SystemInfoChanged();
	}

}

qboolean CL_ProcessServerCommand(const char* origString, const char* cmd) {
    if (!strcmp(cmd, "disconnect")) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
        Com_Error(ERR_DISCONNECT, Cmd_Argv(1));
#pragma GCC diagnostic pop
        return qtrue;
    }

    if (!strcmp(cmd, "cs")) {
        CL_ConfigstringModified();
        Cmd_TokenizeString(origString);
        return qtrue;
    }

    if (!strcmp(cmd, "fadesound")) {
        S_FadeSound((float)atof(Cmd_Argv(1)));
        return qfalse;
    }

    if (!strcmp(cmd, "updatemenus")) {
        UI_UpdateMenus();
        return qfalse;
    }

    if (!strcmp(cmd, "restart")) {
        memset(cl.cmds, 0, sizeof(cl.cmds));
        return qtrue;
    }

    if (!strcmp(cmd, "clientLevelShot")) {
        if (!com_dedicated->integer) {
            return qfalse;
        }

        Cbuf_AddText("wait ; wait ; wait ; wait ; screenshot levelshot\n");
        return qtrue;
    }

    return qtrue;
}

qboolean CL_GetServerCommand( int serverCommandNumber ) {
	char		*s;
	char		*cmd;

	if ( serverCommandNumber <= clc.serverCommandSequence - MAX_RELIABLE_COMMANDS ) {
		Com_Error( ERR_DROP, "CL_GetServerCommand: a reliable command was cycled out" );
		return qfalse;
	}

	if ( serverCommandNumber > clc.serverCommandSequence ) {
		Com_Error( ERR_DROP, "CL_GetServerCommand: requested a command not received" );
		return qfalse;
	}

	s = clc.serverCommands[ serverCommandNumber & ( MAX_RELIABLE_COMMANDS - 1 ) ];

	if( developer->integer == 2 ) {
		Com_DPrintf( "serverCommand: %i : %s\n", serverCommandNumber, s );
	}

	Cmd_TokenizeString(s);
	cmd = Cmd_Argv(0);

	return CL_ProcessServerCommand(s, cmd);
}


void CL_CM_LoadMap( const char *mapname, int *checksum ) {
	CM_LoadMap( mapname, qtrue, checksum );
	re.SetWorldVisData(CM_VisibilityPointer());
}

void *CL_CG_Malloc(int size )
{
	return Z_TagMalloc( size, TAG_CGAME );
}

void CL_CG_Free( void *ptr )
{
	Z_Free( ptr );
}

void CL_CG_AddReliableCommand(const char* cmd)
{
	CL_AddReliableCommand(cmd, qfalse);
}


dshader_t *CL_GetShaderPointer( int iShaderNum )
{
	return CM_ShaderPointer( iShaderNum );
}

void CL_ShutdownCGame( void ) {
	cls.cgameStarted = qfalse;

	if( cge )
	{
		cge->CG_Shutdown();
		cge = NULL;
	}

	Sys_UnloadCGame();

	// refexport_t slot it would have used does not exist in retail either: retail's
	// CL_ShutdownRef with CL_InitRef) -- so retail never reaches that state.

	Z_FreeTags( TAG_CGAME );
}

sfxHandle_t CL_RegisterSound( const char *sample ) {
	return S_RegisterSoundExt( sample, qfalse, qfalse );
}

void CL_StartLocalSound(const char* soundName, qboolean forceLoad) {
	S_StartLocalSound(soundName, qfalse );
}




static int CL_CGI_R_Model_GetHandle(qhandle_t hModel)
{
    dtiki_t *tiki = re.R_Model_GetHandle(hModel);

    return TIKI_HandleForTiki(tiki);
}

static qboolean CL_CGI_TIKI_AnimLoaded(int handle, int animnum)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_AnimLoaded(tiki, animnum) : qfalse;
}

static int CL_CGI_TIKI_NumAnims(int handle)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_NumAnims(tiki) : 0;
}

static int CL_CGI_TIKI_NumSurfaces(int handle)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_NumSurfaces(tiki) : 0;
}

static int CL_CGI_TIKI_NumTags(int handle)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_NumTags(tiki) : 0;
}

static int CL_CGI_TIKI_InitCommands_Client(int handle, tiki_cmd_t *tiki_cmd)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    if (!tiki) {
        return 0;
    }

    return TIKI_InitCommands_Client(tiki, tiki_cmd);
}

static void CL_CGI_TIKI_FlushAll(void)
{
    re.TIKI_FlushAll();
}

static void CL_CGI_TIKI_CalculateBounds(int handle, float scale, vec3_t mins, vec3_t maxs)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    if (tiki) {
        TIKI_CalculateBounds(tiki, scale, mins, maxs);
        return;
    }

    VectorClear(mins);
    VectorClear(maxs);
}

static const char *CL_CGI_TIKI_Name(int handle)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_Name(tiki) : NULL;
}

static float CL_CGI_TIKI_GetFadeDistMod(int handle)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_GetFadeDistMod(tiki) : 1.0f;
}

static const char *CL_CGI_Anim_NameForNum(int handle, int animnum)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_Anim_NameForNum(tiki, animnum) : NULL;
}

static int CL_CGI_Anim_NumForName(int handle, const char *name)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_Anim_NumForName(tiki, name) : -1;
}

static int CL_CGI_Anim_Random(int handle, const char *name)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_Anim_Random(tiki, name) : -1;
}

static int CL_CGI_Anim_NumFrames(int handle, int animnum)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_Anim_NumFrames(tiki, animnum) : 0;
}

static float CL_CGI_Anim_Time(int handle, int animnum)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_Anim_Time(tiki, animnum) : 0.0f;
}

static void CL_CGI_Anim_Delta(int handle, int animnum, vec3_t delta)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    if (tiki) {
        TIKI_Anim_Delta(tiki, animnum, delta);
        return;
    }

    VectorClear(delta);
}

static int CL_CGI_Anim_Flags(int handle, int animnum)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_Anim_Flags(tiki, animnum) : 0;
}

static int CL_CGI_Anim_CrossblendTime(int handle, int animnum)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_Anim_CrossblendTime(tiki, animnum) : 0;
}

static qboolean CL_CGI_Anim_HasClientCommands(int handle, int animnum)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_Anim_HasClientCommands(tiki, animnum) : qfalse;
}

static qboolean CL_CGI_Frame_Commands(
    int         handle,
    int         animnum,
    int         framenum,
    tiki_cmd_t *tiki_cmd,
    int        *extra_anims
    )
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_Frame_Commands_Client(tiki, animnum, framenum, tiki_cmd, extra_anims) : qfalse;
}

static void CL_CGI_TIKI_Frame_Delta(int handle, int animnum, int framenum, vec3_t delta)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    if (tiki) {
        TIKI_Frame_Delta(tiki, animnum, framenum, delta);
        return;
    }

    VectorClear(delta);
}

static float CL_CGI_TIKI_Frame_Time(int handle, int animnum, int framenum)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_Frame_Time(tiki, animnum, framenum) : 0.0f;
}

static void CL_CGI_TIKI_Frame_Bounds(
    int     handle,
    int     animnum,
    int     framenum,
    float   scale,
    vec3_t  mins,
    vec3_t  maxs
    )
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    if (tiki) {
        TIKI_Frame_Bounds(tiki, animnum, framenum, scale, mins, maxs);
        return;
    }

    VectorClear(mins);
    VectorClear(maxs);
}

static void CL_CGI_TIKI_Frame_Coordinates(int handle, int animnum, int framenum, float scale, float *coordinates)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    if (tiki) {
        TIKI_Frame_Coordinates(tiki, animnum, framenum, scale, coordinates);
        return;
    }

    memset(coordinates, 0, sizeof(float) * 9);
}

static float CL_CGI_TIKI_Frame_Radius(int handle, int animnum, int framenum)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_Frame_Radius(tiki, animnum, framenum) : 0.0f;
}

static int CL_CGI_Surface_NameToNum(int handle, const char *name)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_Surface_NameToNum(tiki, name) : 0;
}

static const char *CL_CGI_TIKI_Surface_NumToName(int handle, int num)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_Surface_NumToName(tiki, num) : NULL;
}

static int CL_CGI_TIKI_Surface_Flags(int handle, int num)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_Surface_Flags(tiki, num) : 0;
}

static int CL_CGI_TIKI_Surface_NumSkins(int handle, int num)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_Surface_NumSkins(tiki, num) : 0;
}

static int CL_CGI_Tag_NumForName(int handle, const char *name)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_Tag_NameToNum(tiki, name) : -1;
}

static const char *CL_CGI_Tag_NameForNum(int handle, int num)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_Tag_NumToName(tiki, num) : NULL;
}

static qboolean CL_CGI_TIKI_Alias_Add(int handle, const char *alias, const char *name, const char *parameters)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_Alias_Add(tiki, alias, name, parameters) : qfalse;
}

static const char *CL_CGI_TIKI_Alias_FindRandom(int handle, const char *alias)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_Alias_FindRandom(tiki, alias) : NULL;
}

static void CL_CGI_TIKI_Alias_Dump(int handle)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    if (tiki) {
        TIKI_Alias_Dump(tiki);
    }
}

static void CL_CGI_TIKI_Alias_Clear(int handle)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    if (tiki) {
        TIKI_Alias_Clear(tiki);
    }
}

static const char *CL_CGI_TIKI_Alias_FindDialog(int handle, const char *alias, int random, int entity_number)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_Alias_FindDialog(tiki, alias, random, entity_number) : NULL;
}

static const char *CL_CGI_TIKI_Alias_FindSpecificAnim(int handle, const char *name)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_Alias_FindSpecificAnim(tiki, name) : NULL;
}

static qboolean CL_CGI_TIKI_Alias_CheckLoopAnim(int handle, const char *name)
{
    dtiki_t *tiki = TIKI_GetTiki(handle);

    return tiki ? TIKI_Alias_CheckLoopAnim(tiki, name) : qfalse;
}

void CL_InitCGameDLL( clientGameImport_t *cgi, clientGameExport_t **outCge ) {

	
*outCge = ( clientGameExport_t * )Sys_GetCGameAPI( cgi );
	if( !*outCge ) {
		Com_Error( ERR_DROP, "CL_InitCGameImport failed" );
	}

	memset( cgi, 0, sizeof( clientGameImport_t ) );


	cgi->Printf							= Com_Printf;
	cgi->DPrintf						= Com_DPrintf;
	cgi->Com_WPrintf					= Com_WPrintf;
	cgi->Com_WDPrintf					= Com_WDPrintf;
	cgi->DebugPrintf					= Com_DebugPrintf;
	cgi->Malloc							= CL_CG_Malloc;
	cgi->Free							= CL_CG_Free;
	cgi->Error							= Com_Error;
	cgi->Milliseconds					= Com_Milliseconds;
	cgi->Cvar_Get						= Cvar_Get;
	cgi->Cvar_SetValue					= Cvar_SetValue;
	cgi->Cvar_Set						= Cvar_Set;
	cgi->Argc							= Cmd_Argc;
	cgi->Argv							= Cmd_Argv;
	cgi->Args							= Cmd_Args;
	cgi->AddCommand						= CL_AddCgameCommand;
	cgi->Cmd_Stuff						= Cbuf_AddText;
	cgi->Cmd_Execute					= Cbuf_ExecuteText;
	cgi->Cmd_TokenizeString				= Cmd_TokenizeString;
	cgi->FS_ReadFile					= FS_ReadFileEx;
	// [21] 0x054 RESERVED -- retail declares the slot and never fills it
	cgi->FS_FreeFile					= FS_FreeFile;
	cgi->FS_WriteFile					= FS_WriteFile;
	cgi->FS_WriteTextFile				= FS_WriteTextFile;
	cgi->SendConsoleCommand				= Cbuf_AddText;
	cgi->SendClientCommand				= CL_CG_AddReliableCommand;

	cgi->CM_LoadMap						= CL_CM_LoadMap;
	cgi->CM_InlineModel					= CM_InlineModel;
	cgi->CM_NumInlineModels				= CM_NumInlineModels;
	cgi->CM_PointContents				= CM_PointContents;
	cgi->CM_TransformedPointContents	= CM_TransformedPointContents;
	cgi->CM_BoxTrace					= CM_BoxTrace;
	cgi->CM_BoxTraceEx					= CM_BoxTraceEx;
	cgi->CM_TransformedBoxTrace			= CM_TransformedBoxTrace;
	cgi->CM_TempBoxModel				= CM_TempBoxModel;
	cgi->R_MarkFragments				= re.MarkFragments;
	cgi->Com_GetGameTypeName			= Com_GetGameTypeName;
	cgi->CL_MapLoadingClientOnly		= CL_MapLoadingClientOnly;

	cgi->S_StartSound					= S_StartSound;
	cgi->S_StartLocalSound				= CL_StartLocalSound;
	cgi->S_StopSound					= S_StopSound;
	cgi->S_ClearLoopingSounds			= S_ClearLoopingSounds;
	cgi->S_AddLoopingSound				= S_AddLoopingSound;
	cgi->S_Respatialize					= S_Respatialize;
	cgi->S_BeginRegistration			= S_BeginRegistration;
	cgi->S_RegisterSound				= CL_RegisterSound;
	cgi->S_EndRegistration				= S_EndRegistration;
	cgi->S_UpdateEntity					= S_UpdateEntity;
	cgi->S_SetReverb					= S_SetReverb;

	cgi->MUSIC_NewSoundtrack			= MUSIC_NewSoundtrack;
	cgi->MUSIC_UpdateMood				= MUSIC_UpdateMood;
	cgi->MUSIC_UpdateVolume				= MUSIC_UpdateVolume;
	cgi->MUSIC_UpdateDucking				= MUSIC_UpdateDucking;
	cgi->get_lip_length					= LIP_GetLength;
	cgi->GetNextMorphTarget				= GetNextMorphTarget;
	cgi->get_camera_offset				= get_camera_offset;
	cgi->get_crosshairoffset			= get_crosshairoffset;
	cgi->CL_BeginRegistration			= CL_BeginRegistration;

	cgi->RE_EndRegistration				= re.EndRegistration;
	cgi->R_ClearScene					= re.ClearScene;
	cgi->RE_ClearFlares					= re.ClearFlares;
	cgi->R_RenderScene					= re.RenderScene;
	cgi->R_LoadWorldMap					= re.LoadWorld;
	cgi->R_RegisterModel				= re.RegisterModel;
	cgi->RE_ForceRegisterModel			= re.ForceRegisterModel;
	cgi->RE_RegisterSkin				= re.RegisterSkin;
	cgi->R_RegisterShader				= re.RegisterShader;
	cgi->R_RegisterShaderNoMip			= re.RegisterShaderNoMip;
	cgi->R_AddRefEntityToScene			= re.AddRefEntityToScene;
	cgi->R_AddRefSpriteToScene			= re.AddRefSpriteToScene;
	cgi->R_AddLightToScene				= re.AddLightToScene;
	cgi->R_AddPolyToScene				= re.AddPolyToScene;
	cgi->R_SetColor						= re.SetColor;
	cgi->R_DrawStretchPic				= re.DrawStretchPic;
	cgi->R_GetRenderEntity				= re.GetRenderEntity;
	cgi->R_ModelBounds					= re.ModelBounds;
	cgi->R_ModelRadius					= re.ModelRadius;
	cgi->R_Noise						= re.Noise;
	cgi->R_DebugLine					= re.DebugLine;
	cgi->R_AddLensflareToScene			= re.AddLensflareToScene;
	cgi->R_LensflareNameToNum			= re.LensflareNameToNum;
	cgi->R_SwipeBegin					= re.SwipeBegin;
	cgi->R_SwipePoint					= re.SwipePoint;
	cgi->R_SwipeEnd						= re.SwipeEnd;
	cgi->R_GetShaderWidth				= re.GetShaderWidth;
	cgi->R_GetShaderHeight				= re.GetShaderHeight;
	cgi->R_DrawBox						= re.DrawBox;

	cgi->GetGameState					= CL_GetGameState;
	cgi->GetSnapshot					= CL_GetSnapshot;
	cgi->GetCurrentSnapshotNumber		= CL_GetCurrentSnapshotNumber;
	cgi->GetGlconfig					= CL_GetGlconfig;
	cgi->GetParseEntityState			= CL_GetParseEntityState;
	cgi->GetCurrentCmdNumber			= CL_GetCurrentCmdNumber;
	cgi->GetUserCmd						= CL_GetUserCmd;
	cgi->GetServerCommand				= CL_GetServerCommand;

	cgi->Alias_Add						= Alias_Add;
	cgi->Alias_FindRandom				= Alias_FindRandom;
	cgi->Alias_Dump						= Alias_Dump;
	cgi->Alias_Clear					= Alias_Clear;


	cgi->R_Model_GetHandle				= CL_CGI_R_Model_GetHandle;
	cgi->TIKI_AnimLoaded				= CL_CGI_TIKI_AnimLoaded;
	cgi->TIKI_NumAnims					= CL_CGI_TIKI_NumAnims;
	// [103] 0x19C RESERVED -- retail declares the slot and never fills it
	cgi->TIKI_NumSurfaces				= CL_CGI_TIKI_NumSurfaces;
	cgi->TIKI_NumTags					= CL_CGI_TIKI_NumTags;
	cgi->TIKI_InitCommands_Client		= CL_CGI_TIKI_InitCommands_Client;
	cgi->TIKI_CalculateBounds			= CL_CGI_TIKI_CalculateBounds;
	cgi->TIKI_FlushAll                = CL_CGI_TIKI_FlushAll;
	cgi->TIKI_Name						= CL_CGI_TIKI_Name;
	cgi->TIKI_GetFadeDistMod			= CL_CGI_TIKI_GetFadeDistMod;
	cgi->TIKI_GetTiki					= TIKI_GetTiki;
	cgi->TIKI_CacheAnim					= TIKI_CacheAnim;
	cgi->Anim_NameForNum				= CL_CGI_Anim_NameForNum;
	cgi->Anim_NumForName				= CL_CGI_Anim_NumForName;
	cgi->Anim_Random					= CL_CGI_Anim_Random;
	cgi->Anim_NumFrames					= CL_CGI_Anim_NumFrames;
	cgi->Anim_Time						= CL_CGI_Anim_Time;
	cgi->Anim_Delta						= CL_CGI_Anim_Delta;
	cgi->Anim_Flags						= CL_CGI_Anim_Flags;
	cgi->Anim_CrossblendTime			= CL_CGI_Anim_CrossblendTime;
	cgi->Anim_HasCommands				= CL_CGI_Anim_HasClientCommands;
	cgi->Frame_Commands					= CL_CGI_Frame_Commands;
	cgi->TIKI_Frame_Delta				= CL_CGI_TIKI_Frame_Delta;
	cgi->TIKI_Frame_Time				= CL_CGI_TIKI_Frame_Time;
	cgi->TIKI_Frame_Bounds				= CL_CGI_TIKI_Frame_Bounds;
	cgi->TIKI_Frame_Coordinates			= CL_CGI_TIKI_Frame_Coordinates;
	cgi->TIKI_Frame_Radius				= CL_CGI_TIKI_Frame_Radius;
	cgi->Surface_NameToNum				= CL_CGI_Surface_NameToNum;
	cgi->TIKI_Surface_NumToName			= CL_CGI_TIKI_Surface_NumToName;
	cgi->TIKI_Surface_Flags				= CL_CGI_TIKI_Surface_Flags;
	cgi->TIKI_Surface_NumSkins			= CL_CGI_TIKI_Surface_NumSkins;
	cgi->Tag_NumForName					= CL_CGI_Tag_NumForName;
	cgi->Tag_NameForNum					= CL_CGI_Tag_NameForNum;
	cgi->TIKI_Orientation				= TIKI_Tag_Orientation;
	cgi->TIKI_Tag_OrientationEx			= TIKI_Tag_OrientationEx;
	cgi->TIKI_Tag_NamedOrientation			= TIKI_Tag_NamedOrientation;
	cgi->TIKI_Tag_LerpOrientation			= TIKI_Tag_LerpOrientation;
	cgi->TIKI_Alias_Add					= CL_CGI_TIKI_Alias_Add;
	cgi->TIKI_Alias_FindRandom			= CL_CGI_TIKI_Alias_FindRandom;
	cgi->TIKI_Alias_Dump				= CL_CGI_TIKI_Alias_Dump;
	cgi->TIKI_Alias_Clear				= CL_CGI_TIKI_Alias_Clear;
	cgi->TIKI_Alias_FindDialog			= CL_CGI_TIKI_Alias_FindDialog;
	cgi->TIKI_Alias_FindSpecificAnim	= CL_CGI_TIKI_Alias_FindSpecificAnim;
	cgi->TIKI_Alias_CheckLoopAnim		= CL_CGI_TIKI_Alias_CheckLoopAnim;
	cgi->TIKI_RegisterTiki				= TIKI_RegisterTiki;

	cgi->UI_ParseScores				= UI_ParseScores;
	cgi->Dialog_LoadDialog				= Dialog_LoadDialog;
	cgi->UI_DisplayDialogConsole		= UI_DisplayDialogConsole;
	cgi->Dialog_ResetConsole			= Dialog_ResetConsole;
	cgi->UI_AddPrintString				= UI_AddPrintString;
	cgi->UI_PrintString					= UI_PrintString;
	cgi->UI_AddNotifyString				= UI_AddNotifyString;
	cgi->UI_GetGlobalFontHeight			= UI_GetGlobalFontHeight;
	cgi->UI_GetFontHeight				= UI_GetFontHeight;
	cgi->UI_NewMap						= UI_NewMap;
	cgi->UI_ShowMenu					= UI_ShowMenu;
	cgi->UI_HideMenu					= UI_HideMenu;
	cgi->UI_IsMenuActive				= UI_IsMenuActive;

	cgi->MissionObjective_GetDescription	= MissionObjective_GetDescription;
	cgi->MissionObjective_SetDescription	= MissionObjective_SetDescription;
	cgi->MissionObjective_GetShowObjective	= MissionObjective_GetShowObjective;
	cgi->MissionObjective_SetShowObjective	= MissionObjective_SetShowObjective;
	cgi->MissionObjective_GetObjectiveComplete	= MissionObjective_GetObjectiveComplete;
	cgi->MissionObjective_SetObjectiveComplete	= MissionObjective_SetObjectiveComplete;
	cgi->MissionObjective_GetObjectiveFailed	= MissionObjective_GetObjectiveFailed;
	cgi->MissionObjective_SetObjectiveFailed	= MissionObjective_SetObjectiveFailed;
	cgi->MissionObjective_GetNameFromIndex	= MissionObjective_GetNameFromIndex;
	cgi->MissionObjective_GetIndexFromName	= MissionObjective_GetIndexFromName;
	cgi->MissionObjective_NewObjective		= MissionObjective_NewObjective;
	cgi->MissionObjective_ClearObjectiveList	= MissionObjective_ClearObjectiveList;
	cgi->MissionObjectives_ParseObjectiveFile	= MissionObjectives_ParseObjectiveFile;

	cgi->MissionInformation_GetShader		= MissionInformation_GetShader;
	cgi->MissionInformation_SetShader		= MissionInformation_SetShader;
	cgi->MissionInformation_GetInformationData	= MissionInformation_GetInformationData;
	cgi->MissionInformation_SetInformationData	= MissionInformation_SetInformationData;
	cgi->MissionInformation_GetNameFromIndex	= MissionInformation_GetNameFromIndex;
	cgi->MissionInformation_GetIndexFromName	= MissionInformation_GetIndexFromName;
	cgi->MissionInformation_NewInformation	= MissionInformation_NewInformation;
	cgi->MissionInformation_ClearInformationList	= MissionInformation_ClearInformationList;
	cgi->MissionInformation_SetShowInformation	= MissionInformation_SetShowInformation;
	cgi->MissionInformation_GetShowInformation	= MissionInformation_GetShowInformation;

	cgi->Radar_AddBlip					= Radar_AddBlip;
	cgi->Radar_AddMissionObjectiveBlip	= Radar_AddMissionObjectiveBlip;
	cgi->Radar_SetViewAngles			= Radar_SetViewAngles;
	cgi->ProcessLoadingScreen			= CL_ProcessLoadingScreen;
	cgi->CL_ConfigString				= CL_ConfigString;

	cgi->GetViewModeEntityColor			= GetViewModeEntityColor;
	cgi->GetViewModeEntityShader			= GetViewModeEntityShader;
	cgi->GetViewModeEntityAlpha			= GetViewModeEntityAlpha;
	cgi->GetViewModeHud				= GetViewModeHud;
	cgi->GetViewModeUseFullbright			= GetViewModeUseFullbright;
	cgi->GetNumViewModes				= GetNumViewModes;
	cgi->GetLevelDefs				= PF_GetLevelDefs;
	cgi->areSublevels				= PF_areSublevels;
	cgi->loadLevelStrings				= loadLevelStrings;
	cgi->getStringResource				= getStringResource;
	cgi->isStringResource				= isStringResource;

	// The database is engine-resident: retail's cgame never creates it --

	cgi->SetGameplayFloatProperty			= SetGameplayFloatProperty;
	cgi->GetGameplayFloatProperty			= GetGameplayFloatProperty;
	cgi->SetGameplayStringProperty			= SetGameplayStringProperty;
	cgi->GetGameplayStringProperty			= GetGameplayStringProperty;
	cgi->HasGameplayProperty				= HasGameplayProperty;
	cgi->SetGameplayFloatProperty		= SetGameplayFloatProperty;
	cgi->GetGameplayFloatProperty		= GetGameplayFloatProperty;
	cgi->SetGameplayStringProperty		= SetGameplayStringProperty;
	cgi->GetGameplayStringProperty		= GetGameplayStringProperty;
	cgi->HasGameplayProperty			= HasGameplayProperty;

	cgi->SurfaceNameToType				= SurfaceNameToType;
	cgi->SurfaceTypeToName				= SurfaceTypeToName;

	cgi->Cvar_Find						= Cvar_FindVar;
	cgi->Cvar_CheckRange				= Cvar_CheckRange;
	cgi->FS_ListFilteredFiles			= FS_ListFilteredFiles;
	cgi->FS_FreeFileList				= FS_FreeFileList;
	cgi->FS_FOpenFileWrite				= FS_FOpenFileWrite_HomeData;
	cgi->FS_Write						= FS_Write;
	cgi->FS_FCloseFile					= FS_FCloseFile;
	cgi->FS_CanonicalFilename			= FS_CanonicalFilename;
	cgi->CM_PrintBSPFileSizes			= CM_PrintBSPFileSizes;
	cgi->CM_LeafInPVS					= CM_LeafInPVS;
	cgi->CM_PointLeafnum				= CM_PointLeafnum;
	cgi->R_MarkFragmentsForInlineModel	= re.MarkFragmentsForInlineModel;
	cgi->R_GetInlineModelBounds			= re.GetInlineModelBounds;
	cgi->R_GetLightingForDecal			= re.GetLightingForDecal;
	cgi->R_GetLightingForSmoke			= re.GetLightingForSmoke;
	cgi->R_GatherLightSources			= re.R_GatherLightSources;
	cgi->R_PrintBSPFileSizes			= re.PrintBSPFileSizes;
	cgi->MapVersion						= re.MapVersion;
	cgi->R_MapVersion					= re.MapVersion;
	cgi->R_SpawnEffectModel				= re.SpawnEffectModel;
	cgi->R_RegisterServerModel			= re.RegisterServerModel;
	cgi->R_UnregisterServerModel		= re.UnregisterServerModel;
	cgi->R_GetModelName					= re.GetModelName;
	cgi->R_GetShaderName				= re.GetShaderName;
	cgi->R_AddTerrainMarkToScene		= re.AddTerrainMarkToScene;
	cgi->R_LoadFont						= re.LoadFont;
	cgi->R_DrawString					= re.DrawString;
	cgi->GetShader						= CL_GetShaderPointer;
	cgi->S_SetGlobalAmbientVolumeLevel	= S_SetGlobalAmbientVolumeLevel;
	cgi->S_GetSoundTime					= S_GetSoundTime;
	cgi->S_ChannelNameToNum				= S_ChannelNameToNum;
	cgi->S_ChannelNumToName				= S_ChannelNumToName;
	cgi->S_IsSoundPlaying				= S_IsSoundPlaying;
	cgi->SetTime						= CL_SetTime;
	cgi->Alias_ListAdd					= Alias_ListAdd;
	cgi->Alias_ListFindRandom			= Alias_ListFindRandom;
	cgi->AliasList_New					= AliasList_New;
	cgi->UI_GetHighResolutionScale		= UI_GetHighResolutionScale;
	cgi->TIKI_GetSkeletor				= TIKI_GetSkeletor;
	cgi->TIKI_SetEyeTargetPos			= TIKI_SetEyeTargetPos;
	cgi->Anim_FlagsSkel					= TIKI_Anim_FlagsSkel;
	cgi->Frame_CommandsTime				= TIKI_Frame_Commands_ClientTime;
	cgi->ForceUpdatePose				= re.ForceUpdatePose;
	cgi->TIKI_IsOnGround				= re.TIKI_IsOnGround;
	cgi->TIKI_FindTiki					= TIKI_FindTiki;

	cls.cgameStarted = qtrue;
}

static void CL_ViewModelAnimation_Init() {
	VectorClear( cls.anim.g_vCurrentVMPosOffset );
}

static void CL_InitializeStopwatch() {
	cls.stopwatch.iStartTime = 0;
	cls.stopwatch.iEndTime = 0;
}

void CL_InitClientSavedData( void ) {
	CL_ViewModelAnimation_Init();
	CL_InitializeStopwatch();
}

void CL_InitCGame( void ) {
	const char			*info;
	const char			*mapname;
	int					t1, t2;
	clientGameImport_t	cgi;

	SMP_LOAD_BEGIN( initcg );

	t1 = Sys_Milliseconds();

	info = cl.gameState.stringData + cl.gameState.stringOffsets[ CS_SERVERINFO ];
	mapname = Info_ValueForKey( info, "mapname" );

	Com_sprintf(cl.mapname, sizeof(cl.mapname), "maps/%s.bsp", mapname);

	Dialog_LoadDialog( mapname );

	S_BeginRegistration();
	CL_ShutdownCGame();
	CL_InitCGameDLL( &cgi, &cge );
	CGIMP_VerifyBound( &cgi );
	cls.state = CA_LOADING;

	if( !com_sv_running->integer && !cls.vid_restart )
	{
		TIKI_End();
		TIKI_Begin();
		// alias state (TIKI_FreeLevelData @0x00107E30), never the model cache.
		TIKI_FreeLevelData();
		CL_InitClientSavedData();
	}

	// sent just before a gamestate are not dropped" has no retail counterpart and
	SMP_LOAD_BEGIN( cginit );
	cge->CG_Init(&cgi, clc.serverMessageSequence, clc.serverCommandSequence);
	SMP_LOAD_END( cginit );

	ClearNewConfigFlag();
	TIKI_FinishLoad();


	cls.state = CA_PRIMED;

	CL_EndRegistration();

	t2 = Sys_Milliseconds();

	Com_Printf( "CL_InitCGame: %5.2f seconds\n", ( t2 - t1 ) / 1000.0 );

	SMP_LOAD_END( initcg );
}


qboolean CL_GameCommand( void ) {
	if ( !cge ) {
		return qfalse;
	}

	return cge->CG_ConsoleCommand();
}

void CL_CGameRendering( stereoFrame_t stereo ) {
	cge->CG_DrawActiveFrame( cl.serverTime, stereo, clc.demoplaying );

	cl.oldServerTime = cl.serverTime;
}

void CL_CGame2D( stereoFrame_t stereo ) {
	cge->CG_Draw2D();
}



#define	RESET_TIME	500

void CL_AdjustTimeDelta( void ) {
	int		resetTime;
	int		newDelta;
	int		deltaDelta;

	cl.newSnapshots = qfalse;

	if ( clc.demoplaying ) {
		return;
	}

	if ( com_sv_running->integer ) {
		resetTime = 100;
	} else {
		resetTime = RESET_TIME;
	}

	(void)resetTime;

	newDelta = cl.snap.serverTime - cls.realtime;
	deltaDelta = abs( newDelta - cl.serverTimeDelta );

	if ( deltaDelta > RESET_TIME ) {
		cl.serverTimeDelta = newDelta;
		cl.oldServerTime = cl.snap.serverTime;	
		cl.serverTime = cl.snap.serverTime;
		if ( cl_showTimeDelta->integer ) {
			Com_Printf( "<RESET> " );
		}
	} else if ( deltaDelta > 100 ) {
		if ( cl_showTimeDelta->integer ) {
			Com_Printf( "<FAST> " );
		}
		cl.serverTimeDelta = ( cl.serverTimeDelta + newDelta ) >> 1;
	} else {

		if ( !cls.timeScaled ) {
			if ( cl.extrapolatedSnapshot ) {
				cl.extrapolatedSnapshot = qfalse;
				cl.serverTimeDelta -= 2;
			} else {
				cl.serverTimeDelta++;
			}
		}
	}

	if ( cl_showTimeDelta->integer ) {
		Com_Printf( "%i ", cl.serverTimeDelta );
	}
}


void CL_FirstSnapshot( void ) {
	if ( cl.snap.snapFlags & SNAPFLAG_NOT_ACTIVE ) {
		WIDGETM1_PROBE( "first", "HOLD state=%d snapFlags=%d msgNum=%d", cls.state, cl.snap.snapFlags, cl.snap.messageNum );
		return;
	}
	cls.state = CA_ACTIVE;

	WIDGETM1_PROBE( "first", "ACTIVE state=%d snapFlags=%d cmdNum=%d snapTime=%d", cls.state, cl.snap.snapFlags, cl.cmdNumber, cl.snap.serverTime );

	// 5 -> 4 latch, UI_ServerLoaded @0x00197CA0 repeats it as its sixth step), but
	StopLoadingScreen();

	UI_ServerLoaded();

	cl.serverStartTime = cl.snap.serverTime;
	cl.serverTimeDelta = cl.snap.serverTime - cls.realtime;

	clc.timeDemoBaseTime = cl.snap.serverTime;

	if ( cl_activeAction->string[0] ) {
		Cbuf_AddText( cl_activeAction->string );
		Cvar_Set( "activeAction", "" );
	}

#ifdef USE_MUMBLE
#endif

#ifdef USE_VOIP
#endif

	// widgetContainerCompare (@0x000B4450) NEVER answers 0 -- a tie returns -1 in
	UI_ActivateView3D();
}

static int lastSnapFlags;
void CL_UpdateSnapFlags( void ) {
	lastSnapFlags = cl.snap.snapFlags;
}

void CL_SetCGameTime( void ) {
	if ( cls.state != CA_ACTIVE ) {
		if ( cls.state != CA_PRIMED ) {
			return;
		}
		WIDGETM1_PROBE( "sgt", "state=%d newSnap=1 snapFlags=%d cmdNum=%d msgNum=%d snapTime=%d paused=%d svrun=%d",
			cls.state, cl.snap.snapFlags, cl.cmdNumber, cl.snap.messageNum, cl.snap.serverTime,
			paused->integer, com_sv_running->integer );
		if ( clc.demoplaying ) {
			if ( !clc.firstDemoFrameSkipped ) {
				clc.firstDemoFrameSkipped = qtrue;
				return;
			}
			CL_ReadDemoMessage();
		}
		if ( cl.newSnapshots ) {
			cl.newSnapshots = qfalse;
			CL_FirstSnapshot();
		}
		if ( cls.state != CA_ACTIVE ) {
			return;
		}
	}

	if ( !cl.snap.valid ) {
		assert( 0 );
		Com_Error( ERR_DROP, "CL_SetCGameTime: !cl.snap.valid" );
	}

	if ( paused->integer && com_sv_running->integer ) {
		CL_AdjustTimeDelta();
		return;
	}

	if( ( cl.snap.snapFlags ^ lastSnapFlags ) & SNAPFLAG_SERVERCOUNT ) {
		CL_ServerRestarted();
	} else {
		if( cl.snap.serverTime < cl.oldFrameServerTime ) {
			assert( 0 );
			Com_Error( ERR_DROP, "cl.snap.serverTime < cl.oldFrameServerTime" );
		}
	}

	cl.oldFrameServerTime = cl.snap.serverTime;



	if ( clc.demoplaying && cl_freezeDemo->integer ) {

	} else {
		int tn;

		tn = cl_timeNudge->integer;
		if (tn<-30) {
			tn = -30;
		} else if (tn>30) {
			tn = 30;
		}

		cl.serverTime = cls.realtime + cl.serverTimeDelta - tn;

		if ( cl.serverTime < cl.oldServerTime ) {
			cl.serverTime = cl.oldServerTime;
		}

		if ( cls.realtime + cl.serverTimeDelta >= cl.snap.serverTime - 5 ) {
			cl.extrapolatedSnapshot = qtrue;
		}
	}

	if ( cl.newSnapshots ) {
		CL_AdjustTimeDelta();
	}

	if ( !clc.demoplaying ) {
		return;
	}


	if ( cl_timedemo->integer ) {
		int now = Sys_Milliseconds( );
		int frameDuration;

		if (!clc.timeDemoStart) {
			clc.timeDemoStart = clc.timeDemoLastFrame = now;
			clc.timeDemoMinDuration = INT_MAX;
			clc.timeDemoMaxDuration = 0;
		}

		frameDuration = now - clc.timeDemoLastFrame;
		clc.timeDemoLastFrame = now;

		if( clc.timeDemoFrames > 0 )
		{
			if( frameDuration > clc.timeDemoMaxDuration )
				clc.timeDemoMaxDuration = frameDuration;

			if( frameDuration < clc.timeDemoMinDuration )
				clc.timeDemoMinDuration = frameDuration;

			if( frameDuration > UCHAR_MAX )
				frameDuration = UCHAR_MAX;

			clc.timeDemoDurations[ ( clc.timeDemoFrames - 1 ) %
				MAX_TIMEDEMO_DURATIONS ] = frameDuration;
		}

		clc.timeDemoFrames++;
		cl.serverTime = clc.timeDemoBaseTime + clc.timeDemoFrames * 50;
	}

	while ( cl.serverTime >= cl.snap.serverTime ) {
		CL_ReadDemoMessage();
		if ( cls.state != CA_ACTIVE ) {
			return;		
		}
	}

}



