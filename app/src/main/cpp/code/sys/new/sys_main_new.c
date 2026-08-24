
#include "../sys_local.h"
#include "../sys_loadlib.h"

#ifndef DEDICATED
static void* clipboard_text = NULL;
#endif

static void* game_library = NULL;
static void* cgame_library = NULL;
qboolean	GLimp_SpawnRenderThread(void (*function)(void))
{
    return qfalse;
}

void* GLimp_RendererSleep(void)
{
    return NULL;
}

void		GLimp_FrontEndSleep(void)
{
}

void		GLimp_WakeRenderer(void* data)
{
}

void Sys_ShowConsole(int visLevel, qboolean quitOnClose)
{
}

qboolean SaveRegistryInfo(qboolean user, const char* pszName, void* pvBuf, long lSize)
{
    return qfalse;
}

qboolean LoadRegistryInfo(qboolean user, const char* pszName, void* pvBuf, long* plSize)
{
    return qfalse;
}

qboolean IsFirstRun(void)
{
    return qfalse;
}

qboolean IsNewConfig(void)
{
    return qfalse;
}

qboolean IsSafeMode(void)
{
    return qfalse;
}

void ClearNewConfigFlag(void)
{
}

const char* Sys_GetWholeClipboard(void)
{
#ifndef DEDICATED
    char *data = NULL;
    char *cliptext;

    if ((cliptext = SDL_GetClipboardText()) != NULL) {
        if (cliptext[0] != 0) {
            size_t bufsize = Q_min(strlen(cliptext) + 1, 4096);

            if (clipboard_text != NULL) {
                Z_Free(clipboard_text);
                clipboard_text = NULL;
            }

            data = clipboard_text = Z_Malloc(bufsize);
            Q_strncpyz(data, cliptext, bufsize);
        }
        SDL_free(cliptext);
    }
    return data;
#else
    return NULL;
#endif
}

void Sys_SetClipboard(const char *contents)
{
#ifndef DEDICATED
    if (!contents || !contents[0]) {
        return;
    }

    SDL_SetClipboardText(contents);
#endif
}

void RecoverLostAutodialData(void)
{
}

void Sys_CloseMutex(void)
{
}

void Sys_UnloadGame(void)
{
    Com_Printf("------ Unloading Game ------\n");

    if (game_library) {
        Sys_UnloadLibrary(game_library);
    }

    game_library = NULL;
}

void* Sys_GetGameAPI(void* parms)
{
    void* (*GetGameAPI) (void*);
    const char* gamename = "game" DLL_SUFFIX DLL_EXT;

    if (game_library)
        Com_Error(ERR_FATAL, "Sys_GetGameAPI without calling Sys_UnloadGame");

    game_library = Sys_LoadDll(gamename, qfalse);

    if (!game_library) {
        Com_Printf("Sys_GetGameAPI(%s) failed: \"%s\"\n", gamename, Sys_LibraryError());
        Com_Error(ERR_FATAL, "Couldn't load game");
    }

    Com_Printf("Sys_GetGameAPI(%s): succeeded ...\n", gamename);
    GetGameAPI = (void* (*)(void*))Sys_LoadFunction(game_library, "GetGameAPI");

    if (!GetGameAPI)
    {
        Sys_UnloadGame();
        return NULL;
    }

    return GetGameAPI(parms);
}

void Sys_UnloadCGame(void)
{
    Com_Printf("------ Unloading ClientGame ------\n");

    if (cgame_library) {
        Sys_UnloadLibrary(cgame_library);
    }

    cgame_library = NULL;
}

void* Sys_GetCGameAPI(void* parms)
{
    void* (*GetCGameAPI) (void*);
    const char* gamename = "cgame" DLL_SUFFIX DLL_EXT;

    if (cgame_library)
        Com_Error(ERR_FATAL, "Sys_GetCGameAPI without calling Sys_UnloadCGame");

    cgame_library = Sys_LoadDll(gamename, qfalse);

    if (!cgame_library) {
        Com_Printf("Sys_GetCGameAPI(%s) failed: \"%s\"\n", gamename, Sys_LibraryError());
        Com_Error(ERR_FATAL, "Couldn't load game");
    }

    Com_Printf("Sys_GetCGameAPI(%s): succeeded ...\n", gamename);
    GetCGameAPI = (void* (*)(void*))Sys_LoadFunction(cgame_library, "GetCGameAPI");

    if (!GetCGameAPI)
    {
        Sys_UnloadCGame();
        return NULL;
    }

    return GetCGameAPI(parms);
}

void VM_Forced_Unload_Start(void) {
}

void VM_Forced_Unload_Done(void) {
}

void Sys_InitEx(void)
{
}
void Sys_ShutdownEx(void)
{
}

