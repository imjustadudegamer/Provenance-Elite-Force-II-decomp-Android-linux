/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later

#define _FILE_OFFSET_BITS 64

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "sys_local.h"

#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <pwd.h>
#include <libgen.h>
#include <fcntl.h>
#include <fenv.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/resource.h>

#ifdef __ANDROID__
#include <android/log.h>
#include <jni.h>
#ifdef USE_INTERNAL_SDL_HEADERS
#include "SDL_system.h"
#else
#include <SDL_system.h>
#endif
#endif

qboolean stdinIsATTY;

static char execBuffer[ 1024 ];
static char *execBufferPointer;
static char *execArgv[ 16 ];
static int execArgc;

static void Sys_ClearExecBuffer( void )
{
	execBufferPointer = execBuffer;
	Com_Memset( execArgv, 0, sizeof( execArgv ) );
	execArgc = 0;
}

static void Sys_AppendToExecBuffer( const char *text )
{
	size_t size = sizeof( execBuffer ) - ( execBufferPointer - execBuffer );
	int length = strlen( text ) + 1;

	if( length > size || execArgc >= ARRAY_LEN( execArgv ) )
		return;

	Q_strncpyz( execBufferPointer, text, size );
	execArgv[ execArgc++ ] = execBufferPointer;

	execBufferPointer += length;
}

static int Sys_Exec( void )
{
	pid_t pid = fork( );

	if( pid < 0 )
		return -1;

	if( pid )
	{
		int exitCode;

		wait( &exitCode );

		return WEXITSTATUS( exitCode );
	}
	else
	{
		execvp( execArgv[ 0 ], execArgv );

		exit( -1 );

		return -1;
	}
}


#ifdef __ANDROID__

static const char *Sys_AndroidInternalStoragePath( void )
{
	return SDL_AndroidGetInternalStoragePath();
}

static qboolean Sys_AndroidMakeDirs( const char *root )
{
	char	make[ MAX_OSPATH ];
	char	probe[ MAX_OSPATH ];
	FILE	*f;
	const char *p;

	if( root == NULL || !*root || strlen( root ) >= sizeof( make ) )
		return qfalse;

	for( p = root + 1; *p; p++ )
	{
		if( *p == '/' )
		{
			size_t n = (size_t)( p - root );

			if( n >= sizeof( make ) )
				return qfalse;

			memcpy( make, root, n );
			make[ n ] = 0;
			mkdir( make, 0775 );
		}
	}
	mkdir( root, 0775 );

	Com_sprintf( probe, sizeof( probe ), "%s/.provenanceef2_home_probe", root );
	f = fopen( probe, "wb" );
	if( f == NULL )
		return qfalse;

	fclose( f );
	remove( probe );

	return qtrue;
}

static char *Sys_AndroidHomePath( char *buf, size_t bufsize )
{
	if( buf == NULL || bufsize == 0 )
		return (char *)ANDROID_DATA_ROOT;

	if( !*buf )
	{
		static qboolean announced = qfalse;
		char		candidate[ MAX_OSPATH ];
		const char *obbRoot = ANDROID_DATA_ROOT;
		const char *chosen = NULL;
		const char *reason = NULL;

		if( obbRoot == NULL || !*obbRoot )
			obbRoot = "/data/local/tmp";       

		//     operator can still reach with adb, and it is NOT where the retail tree
		Com_sprintf( candidate, sizeof( candidate ), "%s/%s", obbRoot, HOMEPATH_NAME );
		if( Sys_AndroidMakeDirs( candidate ) )
		{
			chosen = candidate;
			reason = "app dir inside the OBB root";
		}
		else
		{
			const char *internal = Sys_AndroidInternalStoragePath();

			if( internal != NULL && *internal )
			{
				Com_sprintf( candidate, sizeof( candidate ), "%s/%s", internal, HOMEPATH_NAME );
				if( Sys_AndroidMakeDirs( candidate ) )
				{
					chosen = candidate;
					reason = "app internal storage (OBB root is not writable)";
				}
			}
		}

		if( chosen == NULL )
		{
			Com_sprintf( candidate, sizeof( candidate ), "%s", obbRoot );
			chosen = candidate;
			reason = "OBB root itself (no writable candidate)";
		}

		Q_strncpyz( buf, chosen, bufsize );
		Q_strcat( buf, bufsize, "/" );

		if( !announced )
		{
			announced = qtrue;
			fprintf( stderr, "Sys_AndroidHomePath: write root = \"%s\" (%s)\n",
				buf, reason );
			fflush( stderr );
			Sys_AndroidLog( ANDROID_LOG_INFO,
				va( "Sys_AndroidHomePath: write root = \"%s\" (%s)\n", buf, reason ) );
		}
	}

	return buf;
}

char *Sys_HomeConfigPath(void)
{
	static char homeConfigPath[ MAX_OSPATH ] = { 0 };

	return Sys_AndroidHomePath( homeConfigPath, sizeof( homeConfigPath ) );
}

char *Sys_HomeDataPath(void)
{
	static char homeDataPath[ MAX_OSPATH ] = { 0 };

	return Sys_AndroidHomePath( homeDataPath, sizeof( homeDataPath ) );
}

char *Sys_HomeStatePath(void)
{
	static char homeStatePath[ MAX_OSPATH ] = { 0 };

	return Sys_AndroidHomePath( homeStatePath, sizeof( homeStatePath ) );
}

char *Sys_DefaultHomeConfigPath(void) { return Sys_HomeConfigPath(); }
char *Sys_DefaultHomeDataPath(void)   { return Sys_HomeDataPath(); }
char *Sys_DefaultHomeStatePath(void)  { return Sys_HomeStatePath(); }

/* The display density in dpi, read from the Android Resources rather than from SDL's own DPI
   query: the JNI path behind SDL_GetDisplayDPI can fail on a device and leave a caller with
   nothing to size against. Returns 0 when the query is unavailable. */
int Sys_AndroidDensityDpi( void )
{
	JNIEnv *env;
	jobject activity;
	jclass	activityClass;
	jclass	resourcesClass;
	jclass	metricsClass;
	jobject resources;
	jobject metrics;
	jmethodID getResources;
	jmethodID getDisplayMetrics;
	jfieldID  densityDpiField;
	jint	dpi;

	env = (JNIEnv *)SDL_AndroidGetJNIEnv();
	activity = (jobject)SDL_AndroidGetActivity();
	if ( env == NULL || activity == NULL )
		return 0;

	activityClass = (*env)->GetObjectClass( env, activity );
	if ( activityClass == NULL )
		return 0;

	getResources = (*env)->GetMethodID( env, activityClass,
		"getResources", "()Landroid/content/res/Resources;" );
	if ( getResources == NULL )
		return 0;

	resources = (*env)->CallObjectMethod( env, activity, getResources );
	if ( resources == NULL )
		return 0;

	resourcesClass = (*env)->GetObjectClass( env, resources );
	getDisplayMetrics = (*env)->GetMethodID( env, resourcesClass,
		"getDisplayMetrics", "()Landroid/util/DisplayMetrics;" );
	if ( getDisplayMetrics == NULL )
		return 0;

	metrics = (*env)->CallObjectMethod( env, resources, getDisplayMetrics );
	if ( metrics == NULL )
		return 0;

	metricsClass = (*env)->GetObjectClass( env, metrics );
	densityDpiField = (*env)->GetFieldID( env, metricsClass, "densityDpi", "I" );
	if ( densityDpiField == NULL )
		return 0;

	dpi = (*env)->GetIntField( env, metrics, densityDpiField );

	return ( dpi > 0 ) ? (int)dpi : 0;
}

#else 

int Sys_AndroidDensityDpi( void )
{
	return 0;
}

char *Sys_HomeConfigPath(void)
{
	static char homeConfigPath[ MAX_OSPATH ] = { 0 };
	char *p;

	if( !*homeConfigPath )
	{
		if( ( p = getenv( "XDG_CONFIG_HOME" ) ) != NULL && *p != '\0' )
			Com_sprintf(homeConfigPath, sizeof(homeConfigPath), "%s%c", p, PATH_SEP);
		else if( ( p = getenv( "HOME" ) ) != NULL && *p != '\0' )
			Com_sprintf(homeConfigPath, sizeof(homeConfigPath), "%s%c.config%c", p, PATH_SEP, PATH_SEP);

		if( *homeConfigPath )
		{
			if( com_homepath && com_homepath->string[0] )
				Q_strcat(homeConfigPath, sizeof(homeConfigPath), com_homepath->string);
			else
				Q_strcat(homeConfigPath, sizeof(homeConfigPath), HOMEPATH_NAME);
		}
	}

	return homeConfigPath;
}

char *Sys_HomeDataPath(void)
{
	static char homeDataPath[ MAX_OSPATH ] = { 0 };
	char *p;

	if( !*homeDataPath )
	{
		if( ( p = getenv( "XDG_DATA_HOME" ) ) != NULL && *p != '\0' )
			Com_sprintf(homeDataPath, sizeof(homeDataPath), "%s%c", p, PATH_SEP);
		else if( ( p = getenv( "HOME" ) ) != NULL && *p != '\0' )
			Com_sprintf(homeDataPath, sizeof(homeDataPath), "%s%c.local%cshare%c", p, PATH_SEP, PATH_SEP, PATH_SEP);

		if( *homeDataPath )
		{
			if( com_homepath && com_homepath->string[0] )
				Q_strcat(homeDataPath, sizeof(homeDataPath), com_homepath->string);
			else
				Q_strcat(homeDataPath, sizeof(homeDataPath), HOMEPATH_NAME);
		}
	}

	return homeDataPath;
}

char *Sys_HomeStatePath(void)
{
	static char homeStatePath[ MAX_OSPATH ] = { 0 };
	char *p;

	if( !*homeStatePath )
	{
		if( ( p = getenv( "XDG_STATE_HOME" ) ) != NULL && *p != '\0' )
			Com_sprintf(homeStatePath, sizeof(homeStatePath), "%s%c", p, PATH_SEP);
		else if( ( p = getenv( "HOME" ) ) != NULL && *p != '\0' )
			Com_sprintf(homeStatePath, sizeof(homeStatePath), "%s%c.local%cstate%c", p, PATH_SEP, PATH_SEP, PATH_SEP);

		if( *homeStatePath )
		{
			if( com_homepath && com_homepath->string[0] )
				Q_strcat(homeStatePath, sizeof(homeStatePath), com_homepath->string);
			else
				Q_strcat(homeStatePath, sizeof(homeStatePath), HOMEPATH_NAME);
		}
	}

	return homeStatePath;
}

static char *Sys_LegacyHomePath(void)
{
	static char homePath[ MAX_OSPATH ] = { 0 };
	char *p;

	if( ( p = getenv( "FLATPAK_ID" ) ) != NULL && *p != '\0' )
	{
		return "";
	}

	if( !*homePath )
	{
		if( ( p = getenv( "HOME" ) ) != NULL && *p != '\0' )
		{
			Com_sprintf(homePath, sizeof(homePath), "%s%c%s",
				p, PATH_SEP, HOMEPATH_NAME_UNIX_LEGACY);
		}
	}

	return homePath;
}

qboolean Sys_MigrateToXDG(void)
{
	const char *scriptTemplate =
		"#!/bin/sh\n"

		"set -eu\n"

		"legacy_home=\"%s\"\n"
		"xdg_config_home=\"%s\"\n"
		"xdg_data_home=\"%s\"\n"
		"xdg_state_home=\"%s\"\n"

		"xdg_config_pattern=\"*.cfg\"\n"
		"xdg_data_pattern=\"demos/*.dm_* *.log *.pk3 *.txt \\\n"
		"    screenshots/*.jpg screenshots/*.tga videos/*.avi\"\n"
		"xdg_state_pattern=\"*.dat q3history q3key\"\n"

		"glob_copy() {\n"
		"    game_dir=${1:+$1/}\n"
		"    dst=\"$2\"\n"
		"    shift 2\n"
		"    for pattern in \"$@\"; do\n"
		"        subdir=$(dirname \"$pattern\")\n"
		"        [ \"$subdir\" = \".\" ] && subdir=\"\"\n"
		"        find \"$legacy_home/$game_dir\" \\\n"
		"            -path \"$legacy_home/$game_dir$pattern\" -type f \\\n"
		"            -exec mkdir -p \"$dst/$game_dir$subdir\" \\; \\\n"
		"            -exec cp -av {} \"$dst/$game_dir$subdir\" \\;\n"
		"    done\n"
		"}\n"

		"unmatched_copy() {\n"
		"    game_dir=${1:+$1/}\n"
		"    shift\n"
		"    find_args=\"\"\n"
		"    for pattern in \"$@\"; do\n"
		"        find_args=\"$find_args \\\n"
		"            -not -path \\\"$legacy_home/$game_dir$pattern\\\"\"\n"
		"    done\n"
		"    eval \"find '$legacy_home/$game_dir' -type f $find_args\" | \\\n"
		"    while IFS= read -r file; do\n"
		"        dst=\"$xdg_data_home${file#$legacy_home}\"\n"
		"        dst_dir=$(dirname \"$dst\")\n"
		"        mkdir -p \"$dst_dir\"\n"
		"        cp -av \"$file\" \"$dst\"\n"
		"    done\n"
		"}\n"

		"echo \"Starting XDG migration...\"\n"

		"glob_copy \"\" \"$xdg_state_home\" \"qkey\"\n"

		"for game_dir in \"$legacy_home\"/*; do\n"
		"    [ -d \"$game_dir\" ] || continue\n"
		"    game=$(basename \"$game_dir\")\n"
		"    glob_copy \"$game\" \"$xdg_config_home\" $xdg_config_pattern\n"
		"    glob_copy \"$game\" \"$xdg_data_home\" $xdg_data_pattern\n"
		"    glob_copy \"$game\" \"$xdg_state_home\" $xdg_state_pattern\n"
		"    unmatched_copy \"$game\" \\\n"
		"        $xdg_config_pattern \\\n"
		"        $xdg_data_pattern \\\n"
		"        $xdg_state_pattern\n"
		"done\n"

		"echo \"XDG migration complete!\"\n";

	char scriptBuffer[2048];
	int len = Com_sprintf( scriptBuffer, sizeof( scriptBuffer ), scriptTemplate,
		Sys_LegacyHomePath( ), Sys_HomeConfigPath( ),
		Sys_HomeDataPath( ), Sys_HomeStatePath( ) );

	if( len < 0 || len >= (int)sizeof( scriptBuffer ) )
	{
		Com_Printf( "XDG migration error: substitution failed.\n" );
		return qfalse;
	}

	char scriptPath[] = "/tmp/xdgmigrationXXXXXX";
	int fd = mkstemp( scriptPath );
	if( fd == -1 )
	{
		Com_Printf( "XDG migration error: script creation failed.\n" );
		return qfalse;
	}

	if( write( fd, scriptBuffer, len ) != len )
	{
		close( fd );
		unlink( scriptPath );
		Com_Printf( "XDG migration error: script write failed.\n" );
		return qfalse;
	}
	close( fd );

	if( chmod( scriptPath, 0700 ) == -1 )
	{
		unlink( scriptPath );
		Com_Printf( "XDG migration error: script chmod failed.\n" );
		return qfalse;
	}

	Sys_ClearExecBuffer( );
	Sys_AppendToExecBuffer( scriptPath );
	int result = Sys_Exec( );
	unlink( scriptPath );

	return result == 0;
}

static qboolean Sys_ShouldUseLegacyHomePath(void)
{
	if( access( Sys_HomeConfigPath( ), F_OK ) == 0 )
	{
		return qfalse;
	}

	if( ( com_homepath && com_homepath->string[0] ) ||
		Cvar_VariableString( "fs_homepath" )[0] )
	{
		return qfalse;
	}

	const char *legacyHomePath = Sys_LegacyHomePath();

	if( !*legacyHomePath || access( legacyHomePath, F_OK ) != 0 )
	{
		return qfalse;
	}

	char migrationRefusedPath[ MAX_OSPATH ];
	Com_sprintf( migrationRefusedPath, sizeof( migrationRefusedPath ),
		"%s/.xdgMigrationRefused", legacyHomePath );

	if( access( migrationRefusedPath, F_OK ) != 0 )
	{
		dialogResult_t result = Sys_Dialog( DT_YES_NO, va(
			"Modern games and applications store files in "
			"directories according to the Free Desktop standard. "
			"Here's what that would look like for %s:\n\n"
			"Configuration files:\n  %s\n\n"
			"Data files; pk3s, screenshots, logs, demos, etc.:\n  %s\n\n"
			"Internal runtime files:\n  %s\n\n"
			"At the moment all of these files are found here:\n  %s\n\n"
			"Do you want to copy your files to these new directories?",
			PRODUCT_NAME,
			Sys_HomeConfigPath( ), Sys_HomeDataPath( ), Sys_HomeStatePath( ),
			legacyHomePath ),
			"Home Directory Files Upgrade" );

		if( result == DR_YES )
			return !Sys_MigrateToXDG( );

		fclose( fopen( migrationRefusedPath, "w" ) );
	}

	return qtrue;
}

char *Sys_DefaultHomeConfigPath(void)
{
	if( Sys_ShouldUseLegacyHomePath( ) )
		return Sys_LegacyHomePath( );

	return Sys_HomeConfigPath( );
}

char *Sys_DefaultHomeDataPath(void)
{
	if( Sys_ShouldUseLegacyHomePath( ) )
		return Sys_LegacyHomePath( );

	return Sys_HomeDataPath( );
}

char *Sys_DefaultHomeStatePath(void)
{
	if( Sys_ShouldUseLegacyHomePath( ) )
		return Sys_LegacyHomePath( );

	return Sys_HomeStatePath( );
}

#endif 


char *Sys_SteamPath( void )
{
	return "";
}

char *Sys_GogPath( void )
{
	return "";
}

char* Sys_MicrosoftStorePath(void)
{
	return "";
}


unsigned long sys_timeBase = 0;
int curtime;
int Sys_Milliseconds (void)
{
	struct timeval tp;

	gettimeofday(&tp, NULL);

	if (!sys_timeBase)
	{
		sys_timeBase = tp.tv_sec;
		return tp.tv_usec/1000;
	}

	curtime = (tp.tv_sec - sys_timeBase)*1000 + tp.tv_usec/1000;

	return curtime;
}

qboolean Sys_RandomBytes( byte *string, int len )
{
	FILE *fp;

	fp = fopen( "/dev/urandom", "r" );
	if( !fp )
		return qfalse;

	setvbuf( fp, NULL, _IONBF, 0 ); 

	if( fread( string, sizeof( byte ), len, fp ) != len )
	{
		fclose( fp );
		return qfalse;
	}

	fclose( fp );
	return qtrue;
}

char *Sys_GetCurrentUser( void )
{
	struct passwd *p;

	if ( (p = getpwuid( getuid() )) == NULL ) {
		return "player";
	}
	return p->pw_name;
}

#define MEM_THRESHOLD 96*1024*1024

qboolean Sys_LowPhysicalMemory( void )
{
	return qfalse;
}

const char *Sys_Basename( char *path )
{
	return basename( path );
}

const char *Sys_Dirname( char *path )
{
	return dirname( path );
}

FILE *Sys_FOpen( const char *ospath, const char *mode ) {
	struct stat buf;

	if ( !stat( ospath, &buf ) && S_ISDIR( buf.st_mode ) )
		return NULL;

	return fopen( ospath, mode );
}

qboolean Sys_Mkdir( const char *path )
{
	int result = mkdir( path, 0750 );

	if( result != 0 )
	{
		int savedErrno = errno;

		if ( savedErrno == EEXIST ) {
			return qtrue;
		}

		{
			struct stat st;
			const char *kind = "absent";
			int have = ( stat( path, &st ) == 0 );

			if ( have ) {
				kind = S_ISDIR( st.st_mode ) ? "a directory"
					: S_ISREG( st.st_mode ) ? "a regular file" : "neither dir nor file";
			}

			fprintf( stderr,
				"Sys_Mkdir: mkdir(\"%s\", 0750) failed: errno=%d (%s); stat says the "
				"component is %s (mode %04o, uid %d, gid %d, exists=%d)\n",
				path, savedErrno, strerror( savedErrno ), kind,
				have ? (unsigned)( st.st_mode & 07777 ) : 0u,
				have ? (int)st.st_uid : -1, have ? (int)st.st_gid : -1, have );
			fflush( stderr );

#ifdef __ANDROID__
			Sys_AndroidLog( ANDROID_LOG_ERROR,
				va( "Sys_Mkdir: mkdir(\"%s\", 0750) failed: errno=%d (%s); stat: %s "
					"mode %04o uid %d gid %d exists=%d\n",
					path, savedErrno, strerror( savedErrno ), kind,
					have ? (unsigned)( st.st_mode & 07777 ) : 0u,
					have ? (int)st.st_uid : -1, have ? (int)st.st_gid : -1, have ) );
#endif
		}

		return qfalse;
	}

	return qtrue;
}

char *Sys_Cwd( void )
{
	static char cwd[MAX_OSPATH];

	char *result = getcwd( cwd, sizeof( cwd ) - 1 );
	if( result != cwd )
		return NULL;

	cwd[MAX_OSPATH-1] = 0;

	return cwd;
}

char *Sys_BinaryPathRelative(const char *relative)
{
	static char resolved[MAX_OSPATH];
	char combined[MAX_OSPATH];

	snprintf(combined, sizeof(combined), "%s/%s", Sys_BinaryPath(), relative);

	if (!realpath(combined, resolved))
		return NULL;

	return resolved;
}


#define MAX_FOUND_FILES 0x1000

void Sys_ListFilteredFiles(
    const char *basedir, const char *subdirs, const char *filter, qboolean wantsubs, char **list, int *numfiles
)
{
    char           search[MAX_OSPATH], newsubdirs[MAX_OSPATH];
    char           filename[MAX_OSPATH];
    DIR           *fdir;
    struct dirent *d;
    struct stat    st;

    if (*numfiles >= MAX_FOUND_FILES - 1) {
        return;
    }

    if (basedir[0] == '\0') {
        return;
    }

    if (strlen(subdirs)) {
        Com_sprintf(search, sizeof(search), "%s/%s", basedir, subdirs);
    } else {
        Com_sprintf(search, sizeof(search), "%s", basedir);
    }

    if ((fdir = opendir(search)) == NULL) {
        return;
    }

    while ((d = readdir(fdir)) != NULL) {
        // (retail Sys_ListFiles @0x00114D90) starts at catalog index 1 and never
        if (!(Q_stricmp(d->d_name, ".") && Q_stricmp(d->d_name, ".."))) {
            continue;
        }

        Com_sprintf(filename, sizeof(filename), "%s/%s", search, d->d_name);
        if (stat(filename, &st) == -1) {
            continue;
        }

        if ((st.st_mode & S_IFDIR) != 0 && wantsubs) {
            if (strlen(subdirs)) {
                Com_sprintf(newsubdirs, sizeof(newsubdirs), "%s/%s", subdirs, d->d_name);
            } else {
                Com_sprintf(newsubdirs, sizeof(newsubdirs), "%s", d->d_name);
            }

            Sys_ListFilteredFiles(basedir, newsubdirs, filter, wantsubs, list, numfiles);
        }

        if (*numfiles >= MAX_FOUND_FILES - 1) {
            break;
        }

        if (strlen(subdirs)) {
            Com_sprintf(filename, sizeof(filename), "%s/%s", subdirs, d->d_name);
        } else {
            Q_strncpyz(filename, d->d_name, sizeof(filename));
        }

        if (!Com_FilterPath(filter, filename, qfalse)) {
            continue;
        }

        list[*numfiles] = CopyString(filename);
        (*numfiles)++;
    }

    closedir(fdir);
}

char **Sys_ListFiles(const char *directory, const char *extension, const char *filter, int *numfiles, qboolean wantsubs)
{
    struct dirent *d;
    DIR           *fdir;
    char           search[MAX_OSPATH];
    int            nfiles;
    char         **listCopy;
    char          *list[MAX_FOUND_FILES];
    int            i;
    struct stat    st;
    char           buffer[64];

    if (directory[0] == '\0') {
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


    nfiles = 0;

    if ((fdir = opendir(directory)) == NULL) {
        *numfiles = 0;
        return NULL;
    }

    while ((d = readdir(fdir)) != NULL) {
        Com_sprintf(search, sizeof(search), "%s/%s", directory, d->d_name);
        if (stat(search, &st) == -1) {
            continue;
        }

        if (!(Q_stricmp(d->d_name, ".") && Q_stricmp(d->d_name, ".."))) {
            continue;
        }

        if ((st.st_mode & S_IFDIR) == 0) {
            continue;
        }

        if (nfiles == MAX_FOUND_FILES - 1) {
            break;
        }

        list[nfiles] = CopyString(d->d_name);
        nfiles++;
    }

    list[nfiles] = NULL;

    closedir(fdir);

    *numfiles = nfiles;

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
	int smpSleepStart;

	if( msec == 0 )
		return;

	smpSleepStart = Sys_Milliseconds();

	if( stdinIsATTY )
	{
		fd_set fdset;

		FD_ZERO(&fdset);
		FD_SET(STDIN_FILENO, &fdset);
		if( msec < 0 )
		{
			select(STDIN_FILENO + 1, &fdset, NULL, NULL, NULL);
		}
		else
		{
			struct timeval timeout;

			timeout.tv_sec = msec/1000;
			timeout.tv_usec = (msec%1000)*1000;
			select(STDIN_FILENO + 1, &fdset, NULL, NULL, &timeout);
		}
	}
	else
	{
		struct timespec req;

		if( msec < 0 )
			msec = 10;

		req.tv_sec = msec/1000;
		req.tv_nsec = (msec%1000)*1000000;
		nanosleep(&req, NULL);
	}

	SMP_BudgetAdd( SMP_ZONE_SLEEP, Sys_Milliseconds() - smpSleepStart );
}

void Sys_ErrorDialog( const char *error )
{
	char buffer[ 1024 ];
	unsigned int size;
	int f = -1;
	const char *homedatapath = Cvar_VariableString( "fs_homedatapath" );
	const char *gamedir = Cvar_VariableString( "fs_game" );
	const char *fileName = "crashlog.txt";
	char *ospath = FS_BuildOSPath( homedatapath, gamedir, fileName );

	Sys_Print( va( "%s\n", error ) );

#ifndef DEDICATED
	Sys_Dialog( DT_ERROR, va( "%s. See \"%s\" for details.", error, ospath ), "Error" );
#endif

	if( FS_CreatePath( homedatapath ) )
	{
		Com_Printf("ERROR: couldn't create path '%s' for crash log.\n", ospath);
		return;
	}

	f = open( ospath, O_CREAT | O_TRUNC | O_WRONLY, 0640 );
	if( f == -1 )
	{
		Com_Printf( "ERROR: couldn't open %s\n", fileName );
		return;
	}

	while( ( size = CON_LogRead( buffer, sizeof( buffer ) ) ) > 0 ) {
		if( write( f, buffer, size ) != size ) {
			Com_Printf( "ERROR: couldn't fully write to %s\n", fileName );
			break;
		}
	}

	close( f );
}

static void Sys_ZenityCommand( dialogType_t type, const char *message, const char *title )
{
	Sys_ClearExecBuffer( );
	Sys_AppendToExecBuffer( "zenity" );

	switch( type )
	{
		default:
		case DT_INFO:      Sys_AppendToExecBuffer( "--info" ); break;
		case DT_WARNING:   Sys_AppendToExecBuffer( "--warning" ); break;
		case DT_ERROR:     Sys_AppendToExecBuffer( "--error" ); break;
		case DT_YES_NO:
			Sys_AppendToExecBuffer( "--question" );
			Sys_AppendToExecBuffer( "--ok-label=Yes" );
			Sys_AppendToExecBuffer( "--cancel-label=No" );
			break;

		case DT_OK_CANCEL:
			Sys_AppendToExecBuffer( "--question" );
			Sys_AppendToExecBuffer( "--ok-label=OK" );
			Sys_AppendToExecBuffer( "--cancel-label=Cancel" );
			break;
	}

	Sys_AppendToExecBuffer( va( "--text=%s", message ) );
	Sys_AppendToExecBuffer( va( "--title=%s", title ) );
}

static void Sys_KdialogCommand( dialogType_t type, const char *message, const char *title )
{
	Sys_ClearExecBuffer( );
	Sys_AppendToExecBuffer( "kdialog" );

	switch( type )
	{
		default:
		case DT_INFO:      Sys_AppendToExecBuffer( "--msgbox" ); break;
		case DT_WARNING:   Sys_AppendToExecBuffer( "--sorry" ); break;
		case DT_ERROR:     Sys_AppendToExecBuffer( "--error" ); break;
		case DT_YES_NO:    Sys_AppendToExecBuffer( "--warningyesno" ); break;
		case DT_OK_CANCEL: Sys_AppendToExecBuffer( "--warningcontinuecancel" ); break;
	}

	Sys_AppendToExecBuffer( message );
	Sys_AppendToExecBuffer( va( "--title=%s", title ) );
}

static void Sys_XmessageCommand( dialogType_t type, const char *message, const char *title )
{
	Sys_ClearExecBuffer( );
	Sys_AppendToExecBuffer( "xmessage" );
	Sys_AppendToExecBuffer( "-buttons" );

	switch( type )
	{
		default:           Sys_AppendToExecBuffer( "OK:0" ); break;
		case DT_YES_NO:    Sys_AppendToExecBuffer( "Yes:0,No:1" ); break;
		case DT_OK_CANCEL: Sys_AppendToExecBuffer( "OK:0,Cancel:1" ); break;
	}

	Sys_AppendToExecBuffer( "-center" );
	Sys_AppendToExecBuffer( message );
}

dialogResult_t Sys_Dialog( dialogType_t type, const char *message, const char *title )
{
	typedef enum
	{
		NONE = 0,
		ZENITY,
		KDIALOG,
		XMESSAGE,
		NUM_DIALOG_PROGRAMS
	} dialogCommandType_t;
	typedef void (*dialogCommandBuilder_t)( dialogType_t, const char *, const char * );

	const char              *session = getenv( "DESKTOP_SESSION" );
	qboolean                tried[ NUM_DIALOG_PROGRAMS ] = { qfalse };
	dialogCommandBuilder_t  commands[ NUM_DIALOG_PROGRAMS ] = { NULL };
	dialogCommandType_t     preferredCommandType = NONE;
	int                     i;

	commands[ ZENITY ] = &Sys_ZenityCommand;
	commands[ KDIALOG ] = &Sys_KdialogCommand;
	commands[ XMESSAGE ] = &Sys_XmessageCommand;

	if( !Q_stricmp( session, "gnome" ) )
		preferredCommandType = ZENITY;
	else if( !Q_stricmp( session, "kde" ) )
		preferredCommandType = KDIALOG;

	for( i = NONE + 1; i < NUM_DIALOG_PROGRAMS; i++ )
	{
		if( preferredCommandType != NONE && preferredCommandType != i )
			continue;

		if( !tried[ i ] )
		{
			int exitCode;

			commands[ i ]( type, message, title );
			exitCode = Sys_Exec( );

			if( exitCode >= 0 )
			{
				switch( type )
				{
					case DT_YES_NO:    return exitCode ? DR_NO : DR_YES;
					case DT_OK_CANCEL: return exitCode ? DR_CANCEL : DR_OK;
					default:           return DR_OK;
				}
			}

			tried[ i ] = qtrue;

			if( preferredCommandType != NONE )
			{
				preferredCommandType = NONE;
				i = NONE + 1;
			}
		}
	}

	Com_DPrintf( S_COLOR_YELLOW "WARNING: failed to show a dialog\n" );
	return DR_OK;
}

void Sys_GLimpSafeInit( void )
{
}

void Sys_GLimpInit( void )
{
}

void Sys_SetFloatEnv(void)
{
	fesetround(FE_TONEAREST);
}

void Sys_PlatformInit( void )
{
	const char* term = getenv( "TERM" );

	signal( SIGHUP, Sys_SigHandler );
	signal( SIGQUIT, Sys_SigHandler );
	signal( SIGTRAP, Sys_SigHandler );
	signal( SIGABRT, Sys_SigHandler );
	signal( SIGBUS, Sys_SigHandler );

	Sys_SetFloatEnv();

	Sys_PlatformInit_New();

	stdinIsATTY = isatty( STDIN_FILENO ) &&
		!( term && ( !strcmp( term, "raw" ) || !strcmp( term, "dumb" ) ) );
}

void Sys_PlatformExit( void )
{
}


void Sys_SetEnv(const char *name, const char *value)
{
	if(value && *value)
		setenv(name, value, 1);
	else
		unsetenv(name);
}

int Sys_PID( void )
{
	return getpid( );
}

qboolean Sys_PIDIsRunning( int pid )
{
	return kill( pid, 0 ) == 0;
}

qboolean Sys_DllExtension( const char *name ) {
	const char *p;
	char c = 0;

	if ( COM_CompareExtension( name, DLL_EXT ) ) {
		return qtrue;
	}


	p = strstr( name, DLL_EXT "." );

	if ( p ) {
		p += strlen( DLL_EXT );

		while ( *p ) {
			c = *p;

			if ( !isdigit( c ) && c != '.' ) {
				return qfalse;
			}

			p++;
		}

		if ( c != '.' ) {
			return qtrue;
		}
	}

	return qfalse;
}

qboolean Sys_OpenFolderInPlatformFileManager( const char *path )
{
	Sys_ClearExecBuffer( );

	Sys_AppendToExecBuffer( "xdg-open" );

	Sys_AppendToExecBuffer( path );

	return Sys_Exec( ) == 0;
}

qboolean Sys_SetMaxFileLimit( void )
{
#ifdef RLIMIT_NOFILE
	struct rlimit limit;

	if( getrlimit( RLIMIT_NOFILE, &limit ) == 0 )
	{
		limit.rlim_cur = limit.rlim_max;
		if( setrlimit( RLIMIT_NOFILE, &limit ) == 0 )
			return qtrue;
		else
			Com_DPrintf( S_COLOR_YELLOW "WARNING: setrlimit (rlim_max) failed\n" );

#ifdef OPEN_MAX
		limit.rlim_cur = OPEN_MAX;
		if( setrlimit( RLIMIT_NOFILE, &limit ) == 0 )
			return qtrue;
		else
			Com_DPrintf( S_COLOR_YELLOW "WARNING: setrlimit (OPEN_MAX) failed\n" );
#endif
	}
	else
		Com_DPrintf( S_COLOR_YELLOW "WARNING: getrlimit failed\n" );

#endif 

	return qfalse;
}

char *Sys_DefaultCDPath( void ) {
	return "";
}

