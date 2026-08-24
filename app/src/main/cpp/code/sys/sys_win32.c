
#ifdef _WIN32_WINNT_WINXP
#  define PSAPI_VERSION 1
#endif

#ifndef STANDALONE
#define STANDALONE
#endif

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "sys_local.h"

#include <windows.h>
#include <lmerr.h>
#include <lmcons.h>
#include <lmwksta.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <conio.h>
#include <wincrypt.h>
#include <shlobj.h>
#include <psapi.h>
#include <float.h>

#ifndef KEY_WOW64_32KEY
#define KEY_WOW64_32KEY 0x0200
#endif

#ifndef DEDICATED
static UINT timerResolution = 0;
#endif


#ifndef _RC_CHOP

  #define _MCW_EM	0x0008001fU
  #define _MCW_RC	0x00000300U
  #define _MCW_PC	0x00030000U
  #define _RC_NEAR      0x00000000U
  #define _PC_53	0x00010000U
  
  unsigned int _controlfp(unsigned int new, unsigned int mask);
#endif

#define FPUCWMASK1 (_MCW_RC | _MCW_EM)
#define FPUCW (_RC_NEAR | _MCW_EM | _PC_53)

#if idx64
#define FPUCWMASK	(FPUCWMASK1)
#else
#define FPUCWMASK	(FPUCWMASK1 | _MCW_PC)
#endif

void Sys_SetFloatEnv(void)
{
	_controlfp(FPUCW, FPUCWMASK);
}

static char *Sys_DefaultHomePath( void )
{
	static char homePath[ MAX_OSPATH ] = { 0 };

	if(!*homePath && com_homepath)
	{
		TCHAR szPath[MAX_PATH];

		if( !SUCCEEDED( SHGetFolderPathA( NULL, CSIDL_APPDATA,
						NULL, 0, szPath ) ) )
		{
			Com_Printf("Unable to detect CSIDL_APPDATA\n");
			return NULL;
		}
		
		Com_sprintf(homePath, sizeof(homePath), "%s%c", szPath, PATH_SEP);

		if(com_homepath->string[0])
			Q_strcat(homePath, sizeof(homePath), com_homepath->string);
		else
			Q_strcat(homePath, sizeof(homePath), HOMEPATH_NAME);
	}

	return homePath;
}

char *Sys_DefaultHomeConfigPath(void) { return Sys_DefaultHomePath(); }
char *Sys_DefaultHomeDataPath(void)   { return Sys_DefaultHomePath(); }
char *Sys_DefaultHomeStatePath(void)  { return Sys_DefaultHomePath(); }

char *Sys_SteamPath( void )
{
#ifndef STANDALONE

#define STEAMPATH_NAME "Quake 3 Arena"
#define STEAMPATH_APPID "2200"

	static char steamPath[ MAX_OSPATH ] = { 0 };

	HKEY steamRegKey;
	DWORD pathLen = MAX_OSPATH;
	qboolean finishPath = qfalse;

	if (!steamPath[0] && !RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App "
		STEAMPATH_APPID, 0, KEY_QUERY_VALUE | KEY_WOW64_32KEY, &steamRegKey))
	{
		pathLen = MAX_OSPATH;
		if (RegQueryValueEx(steamRegKey, "InstallLocation", NULL, NULL, (LPBYTE)steamPath, &pathLen))
			steamPath[0] = '\0';

		RegCloseKey(steamRegKey);
	}

	if (!steamPath[0] && !RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_QUERY_VALUE, &steamRegKey))
	{
		pathLen = MAX_OSPATH;
		if (RegQueryValueEx(steamRegKey, "SteamPath", NULL, NULL, (LPBYTE)steamPath, &pathLen))
			if (RegQueryValueEx(steamRegKey, "InstallPath", NULL, NULL, (LPBYTE)steamPath, &pathLen))
				steamPath[0] = '\0';

		if (steamPath[0])
			finishPath = qtrue;

		RegCloseKey(steamRegKey);
	}

	if (steamPath[0])
	{
		if (pathLen == MAX_OSPATH)
			pathLen--;

		steamPath[pathLen] = '\0';

		if (finishPath)
			Q_strcat(steamPath, MAX_OSPATH, "\\SteamApps\\common\\" STEAMPATH_NAME );
	}

	return steamPath;
#else
	return "";
#endif
}

char *Sys_GogPath( void )
{
#ifndef STANDALONE

#define GOGPATH_ID "1441704920"

	static char gogPath[ MAX_OSPATH ] = { 0 };

	HKEY gogRegKey;
	DWORD pathLen = MAX_OSPATH;

	if (!gogPath[0] && !RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\GOG.com\\Games\\" GOGPATH_ID, 0, KEY_QUERY_VALUE | KEY_WOW64_32KEY, &gogRegKey))
	{
		pathLen = MAX_OSPATH;
		if (RegQueryValueEx(gogRegKey, "PATH", NULL, NULL, (LPBYTE)gogPath, &pathLen))
			gogPath[0] = '\0';

		RegCloseKey(gogRegKey);
	}

	if (gogPath[0])
	{
		if (pathLen == MAX_OSPATH)
			pathLen--;

		gogPath[pathLen] = '\0';
	}

	return gogPath;
#else
	return "";
#endif
}

char* Sys_MicrosoftStorePath(void)
{
#ifndef STANDALONE

#define MSSTORE_PATH "Quake 3"

	static char microsoftStorePath[MAX_OSPATH] = { 0 };

	if (!microsoftStorePath[0]) 
	{
		TCHAR szPath[MAX_PATH];

		if( !SUCCEEDED( SHGetFolderPathA( NULL, CSIDL_PROGRAM_FILES,
						NULL, 0, szPath ) ) )
		{
			Com_Printf("Unable to detect CSIDL_PROGRAM_FILES\n");
			return microsoftStorePath;
		}

		Com_sprintf(microsoftStorePath, sizeof(microsoftStorePath), "%s%cModifiableWindowsApps%c%s%cEN", szPath, PATH_SEP, PATH_SEP, MSSTORE_PATH, PATH_SEP);
	}

	return microsoftStorePath;
#else
	return "";
#endif
}

int sys_timeBase;
int Sys_Milliseconds (void)
{
	int             sys_curtime;
	static qboolean initialized = qfalse;

	if (!initialized) {
		sys_timeBase = timeGetTime();
		initialized = qtrue;
	}
	sys_curtime = timeGetTime() - sys_timeBase;

	return sys_curtime;
}

qboolean Sys_RandomBytes( byte *string, int len )
{
	HCRYPTPROV  prov;

	if( !CryptAcquireContext( &prov, NULL, NULL,
		PROV_RSA_FULL, CRYPT_VERIFYCONTEXT ) )  {

		return qfalse;
	}

	if( !CryptGenRandom( prov, len, (BYTE *)string ) )  {
		CryptReleaseContext( prov, 0 );
		return qfalse;
	}
	CryptReleaseContext( prov, 0 );
	return qtrue;
}

char *Sys_GetCurrentUser( void )
{
	static char s_userName[1024];
	unsigned long size = sizeof( s_userName );

	if( !GetUserName( s_userName, &size ) )
		strcpy( s_userName, "player" );

	if( !s_userName[0] )
	{
		strcpy( s_userName, "player" );
	}

	return s_userName;
}

#define MEM_THRESHOLD 96*1024*1024

qboolean Sys_LowPhysicalMemory( void )
{
	MEMORYSTATUS stat;
	GlobalMemoryStatus (&stat);
	return (stat.dwTotalPhys <= MEM_THRESHOLD) ? qtrue : qfalse;
}

const char *Sys_Basename( char *path )
{
	static char base[ MAX_OSPATH ] = { 0 };
	int length;

	length = strlen( path ) - 1;

	while( length > 0 && path[ length ] == '\\' )
		length--;

	while( length > 0 && path[ length - 1 ] != '\\' )
		length--;

	Q_strncpyz( base, &path[ length ], sizeof( base ) );

	length = strlen( base ) - 1;

	while( length > 0 && base[ length ] == '\\' )
    base[ length-- ] = '\0';

	return base;
}

const char *Sys_Dirname( char *path )
{
	static char dir[ MAX_OSPATH ] = { 0 };
	int length;

	Q_strncpyz( dir, path, sizeof( dir ) );
	length = strlen( dir ) - 1;

	while( length > 0 && dir[ length ] != '\\' )
		length--;

	dir[ length ] = '\0';

	return dir;
}

FILE *Sys_FOpen( const char *ospath, const char *mode ) {
	size_t length;

	length = strlen( ospath );
	if ( length == 0 || ospath[length-1] == ' ' || ospath[length-1] == '.' ) {
		return NULL;
	}

	return fopen( ospath, mode );
}

qboolean Sys_Mkdir( const char *path )
{
	if( !CreateDirectory( path, NULL ) )
	{
		if( GetLastError( ) != ERROR_ALREADY_EXISTS )
			return qfalse;
	}

	return qtrue;
}

char *Sys_Cwd( void ) {
	static char cwd[MAX_OSPATH];

	_getcwd( cwd, sizeof( cwd ) - 1 );
	cwd[MAX_OSPATH-1] = 0;

	return cwd;
}

char *Sys_BinaryPathRelative(const char *relative)
{
	static char resolved[MAX_OSPATH];
	char combined[MAX_OSPATH];

	snprintf(combined, sizeof(combined), "%s\\%s", Sys_BinaryPath(), relative);

	DWORD len = GetFullPathNameA(combined, MAX_OSPATH, resolved, NULL);
	if (len == 0 || len >= MAX_OSPATH)
		return NULL;

	return resolved;
}


#define MAX_FOUND_FILES 0x1000

void Sys_ListFilteredFiles(
    const char *basedir, const char *subdirs, char *filter, qboolean wantsubs, char **list, int *numfiles
)
{
    char               search[MAX_OSPATH], newsubdirs[MAX_OSPATH];
    char               filename[MAX_OSPATH];
    intptr_t           findhandle;
    struct _finddata_t findinfo;

    if (*numfiles >= MAX_FOUND_FILES - 1) {
        return;
    }

    if (basedir[0] == '\0') {
        return;
    }

    if (strlen(subdirs)) {
        Com_sprintf(search, sizeof(search), "%s\\%s\\*", basedir, subdirs);
    } else {
        Com_sprintf(search, sizeof(search), "%s\\*", basedir);
    }

    findhandle = _findfirst(search, &findinfo);
    if (findhandle == -1) {
        return;
    }

    do {
        if (!(Q_stricmp(findinfo.name, ".") && Q_stricmp(findinfo.name, ".."))) {
            continue;
        }

        if ((findinfo.attrib & _A_SUBDIR) != 0 && wantsubs) {
            if (strlen(subdirs)) {
                Com_sprintf(newsubdirs, sizeof(newsubdirs), "%s\\%s\\*", subdirs, findinfo.name);
            } else {
                Com_sprintf(newsubdirs, sizeof(newsubdirs), "%s", findinfo.name);
            }

            Sys_ListFilteredFiles(basedir, newsubdirs, filter, wantsubs, list, numfiles);
        }

        if (*numfiles >= MAX_FOUND_FILES - 1) {
            break;
        }

        if (strlen(subdirs)) {
            Com_sprintf(filename, sizeof(filename), "%s\\%s", subdirs, findinfo.name);
        } else {
            Q_strncpyz(filename, findinfo.name, sizeof(filename));
        }

        if (!Com_FilterPath(filter, filename, qfalse)) {
            continue;
        }

        list[*numfiles] = CopyString(filename);

        for (char *c = strchr(list[*numfiles], '\\'); c; c = strchr(list[*numfiles], '\\')) {
            *c = '/';
        }

        (*numfiles)++;
    } while (_findnext(findhandle, &findinfo) != -1);

    _findclose(findhandle);
}

static qboolean strgtr(const char *s0, const char *s1)
{
    int l0, l1, i;

    l0 = strlen(s0);
    l1 = strlen(s1);

    if (l1 < l0) {
        l0 = l1;
    }

    for (i = 0; i < l0; i++) {
        if (s1[i] > s0[i]) {
            return qtrue;
        }
        if (s1[i] < s0[i]) {
            return qfalse;
        }
    }
    return qfalse;
}

char **Sys_ListFiles(const char *directory, const char *extension, const char *filter, int *numfiles, qboolean wantsubs)
{
    char               search[MAX_OSPATH];
    int                nfiles;
    char             **listCopy;
    char              *list[MAX_FOUND_FILES] = {NULL};
    struct _finddata_t findinfo;
    intptr_t           findhandle;
    qboolean           swapped;
    int                i;
    char               buffer[64];

    if (directory[0] == NULL) {
        *numfiles = 0;
        return NULL;
    }

    if (!extension) {
        extension = "";
    }

    if (!filter && (extension[0] != '/' || extension[1])) {
        Q_snprintf(buffer, sizeof(buffer), "*%s", extension);
        filter = buffer;
    }

    if (filter) {
        nfiles = 0;
        Sys_ListFilteredFiles(directory, "", filter, wantsubs, list, &nfiles);

        list[nfiles] = NULL;
        *numfiles    = nfiles;
        if (!nfiles) {
            return NULL;
        }

        listCopy = Z_Malloc((nfiles + 1) * sizeof(*listCopy));
        for (i = 0; i < nfiles; i++) {
            listCopy[i] = list[i];
        }
        listCopy[i] = NULL;

        return listCopy;
    }

    Com_sprintf(search, 256, "%s\\*", directory);

    nfiles     = 0;
    findhandle = _findfirst(search, &findinfo);
    if (findhandle == -1) {
        *numfiles = 0;
        return NULL;
    }

    do {
        if (!(Q_stricmp(findinfo.name, ".") && Q_stricmp(findinfo.name, ".."))) {
            continue;
        }

        if ((findinfo.attrib & _A_SUBDIR) == 0) {
            continue;
        }

        if (nfiles >= MAX_FOUND_FILES - 1) {
            break;
        }

        list[nfiles] = CopyString(findinfo.name);

        for (char *c = strchr(list[nfiles], '\\'); c; c = strchr(list[nfiles], '\\')) {
            *c = '/';
        }

        ++nfiles;
    } while (_findnext(findhandle, &findinfo) != -1);

    list[nfiles] = NULL;

    _findclose(findhandle);

    *numfiles = nfiles;

    if (!nfiles) {
        return NULL;
    }

    listCopy = Z_Malloc((nfiles + 1) * sizeof(*listCopy));
    for (i = 0; i < nfiles; i++) {
        listCopy[i] = list[i];
    }
    listCopy[i] = NULL;

    do {
        swapped = qfalse;
        for (i = 1; i < nfiles; i++) {
            if (strgtr(listCopy[i - 1], listCopy[i])) {
                char *temp      = listCopy[i];
                listCopy[i]     = listCopy[i - 1];
                listCopy[i - 1] = temp;
                swapped         = qtrue;
            }
        }
    } while (swapped);

    return listCopy;
}

void Sys_FreeFileList( char **list )
{
	int i;

	if ( !list ) {
		return;
	}

	for ( i = 0 ; list[i] ; i++ ) {
		Z_Free( list[i] );
	}

	Z_Free( list );
}


void Sys_Sleep( int msec )
{
	if( msec == 0 )
		return;

#ifdef DEDICATED
	if( msec < 0 )
		WaitForSingleObject( GetStdHandle( STD_INPUT_HANDLE ), INFINITE );
	else
		WaitForSingleObject( GetStdHandle( STD_INPUT_HANDLE ), msec );
#else
	if( msec < 0 )
		return;

	Sleep( msec );
#endif
}

void Sys_ErrorDialog( const char *error )
{
	Sys_Print( va( "%s\n", error ) );

	if( Sys_Dialog( DT_YES_NO, va( "%s. Copy console log to clipboard?", error ),
			"Error" ) == DR_YES )
	{
		HGLOBAL memoryHandle;
		char *clipMemory;

		memoryHandle = GlobalAlloc( GMEM_MOVEABLE|GMEM_DDESHARE, CON_LogSize( ) + 1 );
		clipMemory = (char *)GlobalLock( memoryHandle );

		if( clipMemory )
		{
			char *p = clipMemory;
			char buffer[ 1024 ];
			unsigned int size;

			while( ( size = CON_LogRead( buffer, sizeof( buffer ) ) ) > 0 )
			{
				Com_Memcpy( p, buffer, size );
				p += size;
			}

			*p = '\0';

			if( OpenClipboard( NULL ) && EmptyClipboard( ) )
				SetClipboardData( CF_TEXT, memoryHandle );

			GlobalUnlock( clipMemory );
			CloseClipboard( );
		}
	}
}

dialogResult_t Sys_Dialog( dialogType_t type, const char *message, const char *title )
{
	UINT uType;

	switch( type )
	{
		default:
		case DT_INFO:      uType = MB_ICONINFORMATION|MB_OK; break;
		case DT_WARNING:   uType = MB_ICONWARNING|MB_OK; break;
		case DT_ERROR:     uType = MB_ICONERROR|MB_OK; break;
		case DT_YES_NO:    uType = MB_ICONQUESTION|MB_YESNO; break;
		case DT_OK_CANCEL: uType = MB_ICONWARNING|MB_OKCANCEL; break;
	}

	switch( MessageBox( NULL, message, title, uType ) )
	{
		default:
		case IDOK:      return DR_OK;
		case IDCANCEL:  return DR_CANCEL;
		case IDYES:     return DR_YES;
		case IDNO:      return DR_NO;
	}
}

void Sys_GLimpSafeInit( void )
{
}

void Sys_GLimpInit( void )
{
}

void Sys_PlatformInit( void )
{
#ifndef DEDICATED
	TIMECAPS ptc;
#endif

	Sys_SetFloatEnv();

#ifndef DEDICATED
	if(timeGetDevCaps(&ptc, sizeof(ptc)) == MMSYSERR_NOERROR)
	{
		timerResolution = ptc.wPeriodMin;

		if(timerResolution > 1)
		{
			Com_Printf("Warning: Minimum supported timer resolution is %ums "
				"on this system, recommended resolution 1ms\n", timerResolution);
		}
		
		timeBeginPeriod(timerResolution);				
	}
	else
		timerResolution = 0;
#endif

	Sys_PlatformInit_New();
}

void Sys_PlatformExit( void )
{
#ifndef DEDICATED
	if(timerResolution)
		timeEndPeriod(timerResolution);
#endif
}

void Sys_SetEnv(const char *name, const char *value)
{
	if(value)
		_putenv(va("%s=%s", name, value));
	else
		_putenv(va("%s=", name));
}

int Sys_PID( void )
{
	return GetCurrentProcessId( );
}

qboolean Sys_PIDIsRunning( int pid )
{
	DWORD processes[ 1024 ];
	DWORD numBytes, numProcesses;
	int i;

	if( !EnumProcesses( processes, sizeof( processes ), &numBytes ) )
		return qfalse; 

	numProcesses = numBytes / sizeof( DWORD );

	for( i = 0; i < numProcesses; i++ )
	{
		if( (int)processes[ i ] == pid )
			return qtrue;
	}

	return qfalse;
}

qboolean Sys_DllExtension( const char *name ) {
	return COM_CompareExtension( name, DLL_EXT );
}

qboolean Sys_OpenFolderInPlatformFileManager( const char *path )
{
	return ShellExecute( NULL, "explore", path, NULL, NULL, SW_SHOWDEFAULT ) > (HINSTANCE)32;
}

qboolean Sys_SetMaxFileLimit( void )
{
	return qtrue;
}
