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


#include "q_shared.h"
#include "qcommon.h"
#include "unzip.h"

#include <errno.h>   

#ifdef __ANDROID__
#include <android/log.h>
extern "C" void Sys_AndroidLog( int priority, const char *msg );
#endif

#define PROBE_GET(probe_n, probe_d, probe_f) Cvar_Get(probe_n, probe_d, probe_f)
#define PROBE_PRINT                       Com_Printf
#include "qa_probe.h"

#ifndef _WIN32
#   include <sys/types.h>
#   include <sys/stat.h>
#   include <errno.h>
#   include <dirent.h>
#else
#   include <sys/types.h>
#   include <sys/stat.h>
#endif

#ifdef _MSC_VER
#   include <io.h>
#else
#   include <unistd.h>
#endif

#if defined(__linux__)
#   include <fcntl.h>
#   define EF2_HAVE_FADVISE 1
#endif


#ifndef STANDALONE
#define	DEMO_PAK0_CHECKSUM	2985612116u
[[maybe_unused]] static const unsigned pak_checksums[] = {
	301277748u,
	450555014u,
	3689002325u,
	755737592u,
	1893706666u,
	2666463710u
};
#endif


#define MAX_ZPATH			256
#define	MAX_SEARCH_PATHS	4096
#define MAX_FILEHASH_SIZE	1024

typedef struct fileInPack_s {
	char					*name;		
    unsigned long			pos;		
    unsigned long			len;		
	struct	fileInPack_s*	next;		
} fileInPack_t;

typedef struct {
	char			pakFilename[MAX_OSPATH];	
	char			pakBasename[MAX_OSPATH];	
	char			pakGamename[MAX_OSPATH];	
	unzFile			handle;						
	unsigned int	checksum;					
	int				pure_checksum;				
	int				numfiles;					
	int				referenced;					
	fileInPack_t*	hashTable[MAX_FILEHASH_SIZE];	
	fileInPack_t*	buildBuffer;				
	int*			crcWords;					
	int				numCrcWords;				
	long			fileSize;					
	long			fileTime;					
} pack_t;

typedef struct {
    char		path[MAX_OSPATH];		
    char		fullpath[MAX_OSPATH];		
	char		gamedir[MAX_OSPATH];	
} directory_t;

typedef struct searchpath_s {
	struct searchpath_s *next;

	pack_t		*pack;		
	directory_t	*dir;
} searchpath_t;

static	char		fs_gamedir[MAX_OSPATH];	
cvar_t				*fs_debug;
static	cvar_t		*fs_homeconfigpath;
static	cvar_t		*fs_homedatapath;
static	cvar_t		*fs_homestatepath;

static	cvar_t		*fs_apppath;
static	cvar_t		*fs_steampath;
static	cvar_t		*fs_gogpath;
static	cvar_t		*fs_microsoftstorepath;

static	cvar_t		*fs_basepath;
static	cvar_t		*fs_cdpath;
static	cvar_t		*fs_copyfiles;
static	searchpath_t	*fs_searchpaths;
static	int			fs_readCount;			
static	int			fs_loadCount;			
static	int			fs_loadStack;			
static	int			fs_packFiles = 0;		

static cvar_t *fs_pathVars[16];

static int fs_checksumFeed;

static int fs_pureChecksumExtra;

static cvar_t* fs_gamedirvar;
static cvar_t* fs_restrict;
static cvar_t* fs_filedir;
cvar_t* fs_mapdir;

typedef union qfile_gus {
	FILE*		o;
	unzFile		z;
} qfile_gut;

typedef struct qfile_us {
	qfile_gut	file;
	qboolean	unique;
} qfile_ut;

typedef struct {
	qfile_ut	handleFiles;
	qboolean	handleSync;
	int			baseOffset;
	int			fileSize;
	int			zipFilePos;
	int			zipFileLen;
	qboolean	zipFile;
	char		name[MAX_ZPATH];
} fileHandleData_t;

static fileHandleData_t	fsh[MAX_FILE_HANDLES];

static qboolean fs_reordered;

static int			fs_numServerPaks = 0;
static unsigned int	fs_serverPaks[MAX_SEARCH_PATHS];				
static char			*fs_serverPakNames[MAX_SEARCH_PATHS];			

static int			fs_numServerReferencedPaks;
static unsigned int	fs_serverReferencedPaks[MAX_SEARCH_PATHS];			
static char			*fs_serverReferencedPakNames[MAX_SEARCH_PATHS];		

static Q_UNUSED_VAR const char* fs_originalPaks_main[] =
{
	"Pak0.pk3",
	"Pak1.pk3",
	"Pak2.pk3",
	"Pak3.pk3",
	"Pak4.pk3",
	"Pak5.pk3",
	"Pak6.pk3"
};

static Q_UNUSED_VAR const char* fs_originalPaks_mainta[] =
{
	"pak1.pk3",
	"pak2.pk3",
	"pak3.pk3",
	"pak4.pk3",
	"pak5.pk3"
};

static Q_UNUSED_VAR const char* fs_originalPaks_maintt[] =
{
	"pak1.pk3",
	"pak2.pk3",
	"pak3.pk3",
	"pak4.pk3"
};

#ifdef FS_MISSING
FILE*		missingFiles = NULL;
#endif

#if __STDC_VERSION__ < 199901L
#  if __GNUC__ >= 2 || _MSC_VER >= 1300
#    define __func__ __FUNCTION__
#  else
#    define __func__ "(unknown)"
#  endif
#endif


qboolean FS_Initialized( void ) {
	return (fs_searchpaths != NULL);
}

qboolean FS_PakIsPure( pack_t *pack ) {
	int i;

	if ( fs_numServerPaks ) {
		for ( i = 0 ; i < fs_numServerPaks ; i++ ) {
			if ( pack->checksum == fs_serverPaks[i] ) {
				return qtrue;		
			}
		}
		return qfalse;	
	}
	return qtrue;
}


int FS_LoadStack( void )
{
	return fs_loadStack;
}

static long FS_HashFileName( const char *fname, int hashSize ) {
	int		i;
	long	hash;
	char	letter;

	hash = 0;
	i = 0;
	while (fname[i] != '\0') {
		letter = tolower(fname[i]);
		if (letter =='.') break;				
		if (letter =='\\') letter = '/';		
		if (letter == PATH_SEP) letter = '/';		
		hash+=(long)(letter)*(i+119);
		i++;
	}
	hash = (hash ^ (hash >> 10) ^ (hash >> 20));
	hash &= (hashSize-1);
	return hash;
}

static fileHandle_t	FS_HandleForFile(void) {
	int		i;

	for ( i = 1 ; i < MAX_FILE_HANDLES ; i++ ) {
		if ( fsh[i].handleFiles.file.o == NULL ) {
			return i;
		}
	}
	Com_Error( ERR_DROP, "FS_HandleForFile: none free" );
	return 0;
}

static FILE	*FS_FileForHandle( fileHandle_t f ) {
	if ( f < 1 || f >= MAX_FILE_HANDLES ) {
		Com_Error( ERR_DROP, "FS_FileForHandle: out of range" );
	}
	if (fsh[f].zipFile == qtrue) {
		Com_Error( ERR_DROP, "FS_FileForHandle: can't get FILE on zip file" );
	}
	if ( ! fsh[f].handleFiles.file.o ) {
		Com_Error( ERR_DROP, "FS_FileForHandle: NULL" );
	}
	
	return fsh[f].handleFiles.file.o;
}

void	FS_ForceFlush( fileHandle_t f ) {
	FILE *file;

	file = FS_FileForHandle(f);
	setvbuf( file, NULL, _IONBF, 0 );
}


long FS_fplength(FILE *h)
{
	long		pos;
	long		end;

	pos = ftell(h);
	fseek(h, 0, SEEK_END);
	end = ftell(h);
	fseek(h, pos, SEEK_SET);

	return end;
}

long FS_filelength(fileHandle_t f)
{
	FILE	*h;

	h = FS_FileForHandle(f);
	
	if(h == NULL)
		return -1;
	else
		return FS_fplength(h);
}

static void FS_ReplaceSeparators( char *path ) {
	char	*s;
	qboolean lastCharWasSep = qfalse;

	for ( s = path ; *s ; s++ ) {
		if ( *s == '/' || *s == '\\' ) {
			if ( !lastCharWasSep ) {
				*s = PATH_SEP;
				lastCharWasSep = qtrue;
			} else {
				memmove (s, s + 1, strlen (s));
			}
		} else {
			lastCharWasSep = qfalse;
		}
	}
}

static int check_filecase( char *Path ) {
    int retval;

#ifdef _WIN32
    retval = 0;
#else
	char *ptr;
	char *filename;
	const char *basedir;
    DIR* dir;

    retval = 0;
    ptr = strrchr(Path, '/');
    filename = Path;

    if (ptr) {
        filename = ptr + 1;
    }

    basedir = ".";
    if (ptr)
    {
        basedir = Path;
        *ptr = 0;
    }

    dir = opendir(basedir);
    if (dir)
    {
        dirent* ent;

        for (ent = readdir(dir); ent && !retval; ent = readdir(dir)) {
            if (!Q_stricmp(ent->d_name, filename)) {
                strcpy(filename, ent->d_name);
                retval = 1;
            }
        }

        closedir(dir);
    }

    if (ptr) {
        *ptr = '/';
    }
#endif
    return retval;
}

void FS_CorrectCase( char *path ) {
	int		getOut;
	char	*ptr;

	if( !access( path, 0 ) ) {
		return;
	}

	getOut = 0;

	if( *path == '/' ) {
		ptr = path + 1;
	} else {
		ptr = path;
	}

	do
	{
		ptr = strchr( ptr, '/' );

		if( ptr ) {
			*ptr = 0;
		}

		if( access(path, 0) ) {
			getOut = !check_filecase( path );
		}

		if( !ptr ) {
			break;
		}

		*ptr++ = '/';

	} while( !getOut );
}

char *FS_BuildOSPath( const char *base, const char *game, const char *qpath ) {
	char	temp[MAX_OSPATH];
	static char ospath[2][MAX_OSPATH];
	static int toggle;

	toggle ^= 1;		

	if( !game || !game[ 0 ] ) {
		game = fs_gamedir;
	}

	Com_sprintf( temp, sizeof( temp ), "/%s/%s", game, qpath );
	FS_ReplaceSeparators( temp );
	Com_sprintf( ospath[toggle], sizeof( ospath[0] ), "%s%s", base, temp );
	FS_CorrectCase( ospath[ toggle ] );

	return ospath[ toggle ];
}

char *FS_BaseDir_BuildOSPath( const char *base, const char *qpath ) {
	char *ospath = FS_BuildOSPath(base, qpath, "");
	ospath[strlen(ospath) - 1] = '\0';

	return ospath;
}


qboolean FS_CreatePath (const char *OSPath) {
	char	*ofs;
	char	path[MAX_OSPATH];
	
	if (!OSPath || !*OSPath) {
		return qfalse;
	}

	if ( strstr( OSPath, ".." ) || strstr( OSPath, "::" ) ) {
		Com_Printf( "WARNING: refusing to create relative path \"%s\"\n", OSPath );
		return qtrue;
	}

	Q_strncpyz( path, OSPath, sizeof( path ) );
	FS_ReplaceSeparators( path );

	ofs = strchr( path, PATH_SEP );
	if ( ofs != NULL ) {
		ofs++;
	}

	for (; ofs != NULL && *ofs ; ofs++) {
		if (*ofs == PATH_SEP) {
			*ofs = 0;
			if (!Sys_Mkdir (path)) {
				fprintf( stderr,
					"FS_CreatePath: failed to create path \"%s\" (component \"%s\", "
					"errno=%d (%s))\n", OSPath, path, errno, strerror( errno ) );
				fflush( stderr );
#ifdef __ANDROID__
				Sys_AndroidLog( ANDROID_LOG_ERROR,
					va( "FS_CreatePath: failed to create path \"%s\" (component \"%s\", "
						"errno=%d (%s))\n", OSPath, path, errno, strerror( errno ) ) );
#endif
				Com_Error( ERR_FATAL, "FS_CreatePath: failed to create path \"%s\"",
					path );
			}
			*ofs = PATH_SEP;
		}
	}

	return qfalse;
}

static void FS_CheckFilenameIsMutable( const char *filename,
		const char *function )
{
	if( Sys_DllExtension( filename )
		|| COM_CompareExtension( filename, ".qvm" )
		|| COM_CompareExtension( filename, ".pk3" ) )
	{
		Com_Error( ERR_FATAL, "%s: Not allowed to manipulate '%s' due "
			"to %s extension", function, filename, COM_GetExtension( filename ) );
	}
}

void FS_Remove( const char *osPath ) {
	FS_CheckFilenameIsMutable( osPath, __func__ );

	remove( osPath );
}

void FS_Remove_HomeData( const char *homePath ) {
	FS_CheckFilenameIsMutable( homePath, __func__ );

	remove( FS_BuildOSPath( fs_homedatapath->string,
			fs_gamedir, homePath ) );
}

qboolean FS_FileInPathExists(const char *testpath)
{
	FILE *filep;

	filep = Sys_FOpen(testpath, "rb");
	
	if(filep)
	{
		fclose(filep);
		return qtrue;
	}
	
	return qfalse;
}

qboolean FS_FileExists_HomeConfig(const char *file)
{
	return FS_FileInPathExists(FS_BuildOSPath(fs_homeconfigpath->string, fs_gamedir, file));
}

qboolean FS_FileExists_HomeData(const char *file)
{
	return FS_FileInPathExists(FS_BuildOSPath(fs_homedatapath->string, fs_gamedir, file));
}

qboolean FS_FileExists_HomeState(const char *file)
{
	return FS_FileInPathExists(FS_BuildOSPath(fs_homestatepath->string, fs_gamedir, file));
}

static qboolean FS_BaseDir_FileExists_HomeData( const char *file )
{
	return FS_FileInPathExists(FS_BaseDir_BuildOSPath(fs_homedatapath->string, file));
}

static fileHandle_t FS_OSPath_FOpenFileWrite( const char *ospath, const char *filename ) {
	fileHandle_t	f;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization\n" );
	}

	f = FS_HandleForFile();
	fsh[f].zipFile = qfalse;

	if ( fs_debug->integer ) {
		Com_Printf( "FS_OSPath_FOpenFileWrite: %s\n", ospath );
	}

	FS_CheckFilenameIsMutable( ospath, __func__ );

	if( FS_CreatePath( ospath ) ) {
		return 0;
	}

	fsh[f].handleFiles.file.o = Sys_FOpen( ospath, "wb" );

	Q_strncpyz( fsh[f].name, filename, sizeof( fsh[f].name ) );

	fsh[f].handleSync = qfalse;
	if (!fsh[f].handleFiles.file.o) {
		f = 0;
	}
	return f;
}

fileHandle_t FS_BaseDir_FOpenFileWrite_HomeConfig( const char *filename ) {
	return FS_OSPath_FOpenFileWrite(
		FS_BaseDir_BuildOSPath(fs_homeconfigpath->string, filename), filename);
}

fileHandle_t FS_BaseDir_FOpenFileWrite_HomeData( const char *filename ) {
	return FS_OSPath_FOpenFileWrite(
		FS_BaseDir_BuildOSPath(fs_homedatapath->string, filename), filename);
}

fileHandle_t FS_BaseDir_FOpenFileWrite_HomeState( const char *filename ) {
	return FS_OSPath_FOpenFileWrite(
		FS_BaseDir_BuildOSPath(fs_homestatepath->string, filename), filename);
}

long FS_BaseDir_FOpenFileRead(const char *filename, fileHandle_t *fp)
{
	char *ospath;
	fileHandle_t f = 0;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization\n" );
	}

	f = FS_HandleForFile();
	fsh[f].zipFile = qfalse;

	Q_strncpyz( fsh[f].name, filename, sizeof( fsh[f].name ) );

	S_ClearSoundBuffer();

	for(int i = 0; (size_t)i < ARRAY_LEN( fs_pathVars ) && !fsh[f].handleFiles.file.o; i++) {
		const cvar_t *pathVar = fs_pathVars[i];

		if (!pathVar || !pathVar->string[0]) {
			continue;
		}

		ospath = FS_BaseDir_BuildOSPath( pathVar->string, filename );

		if ( fs_debug->integer )
		{
			Com_Printf( "FS_BaseDir_FOpenFileRead (%s): %s\n", pathVar->name, ospath );
		}

		fsh[f].handleFiles.file.o = Sys_FOpen( ospath, "rb" );
		fsh[f].handleSync = qfalse;
	}

	if (!fsh[f].handleFiles.file.o)
	{
		f = 0;
	}

	*fp = f;
	if (f) {
		return FS_filelength(f);
	}

	return -1;
}


void FS_BaseDir_Rename_HomeData( const char *from, const char *to, qboolean safe ) {
	char			*from_ospath, *to_ospath;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization\n" );
	}

	S_ClearSoundBuffer();

	from_ospath = FS_BaseDir_BuildOSPath( fs_homedatapath->string, from );
	to_ospath = FS_BaseDir_BuildOSPath( fs_homedatapath->string, to );

	if ( fs_debug->integer ) {
		Com_Printf( "FS_BaseDir_Rename_HomeData: %s --> %s\n", from_ospath, to_ospath );
	}

	if ( safe ) {
		FS_CheckFilenameIsMutable( to_ospath, __func__ );
	}

	rename(from_ospath, to_ospath);
}

void FS_FCloseFile( fileHandle_t f ) {
	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization\n" );
	}

	if (fsh[f].zipFile == qtrue) {
		unzCloseCurrentFile( fsh[f].handleFiles.file.z );
		if ( fsh[f].handleFiles.unique ) {
			unzClose( fsh[f].handleFiles.file.z );
		}
		Com_Memset( &fsh[f], 0, sizeof( fsh[f] ) );
		return;
	}

	if (fsh[f].handleFiles.file.o) {
		fclose (fsh[f].handleFiles.file.o);
	}
	Com_Memset( &fsh[f], 0, sizeof( fsh[f] ) );
}

fileHandle_t FS_FOpenFileWrite_HomeConfig( const char *filename ) {
	return FS_OSPath_FOpenFileWrite(
		FS_BuildOSPath(fs_homeconfigpath->string, fs_gamedir, filename), filename);
}

fileHandle_t FS_FOpenFileWrite_HomeData( const char *filename ) {
	return FS_OSPath_FOpenFileWrite(
		FS_BuildOSPath(fs_homedatapath->string, fs_gamedir, filename), filename);
}

fileHandle_t FS_FOpenFileWrite_HomeState( const char *filename ) {
	return FS_OSPath_FOpenFileWrite(
		FS_BuildOSPath(fs_homestatepath->string, fs_gamedir, filename), filename);
}

fileHandle_t FS_FOpenFileAppend_HomeData( const char *filename ) {
	char			*ospath;
	fileHandle_t	f;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization\n" );
	}

	f = FS_HandleForFile();
	fsh[f].zipFile = qfalse;

	Q_strncpyz( fsh[f].name, filename, sizeof( fsh[f].name ) );

	S_ClearSoundBuffer();

	ospath = FS_BuildOSPath( fs_homedatapath->string, fs_gamedir, filename );

	if ( fs_debug->integer ) {
		Com_Printf( "FS_FOpenFileAppend_HomeData: %s\n", ospath );
	}

	FS_CheckFilenameIsMutable( ospath, __func__ );

	if( FS_CreatePath( ospath ) ) {
		return 0;
	}

	fsh[f].handleFiles.file.o = Sys_FOpen( ospath, "ab" );
	fsh[f].handleSync = qfalse;
	if (!fsh[f].handleFiles.file.o) {
		f = 0;
	}
	return f;
}


static fileHandle_t FS_OSPath_FOpenTextFileWrite( const char *ospath, const char *filename ) {
	fileHandle_t	f;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization\n" );
	}

	f = FS_HandleForFile();
	fsh[f].zipFile = qfalse;

	if ( fs_debug->integer ) {
		Com_Printf( "FS_OSPath_FOpenTextFileWrite: %s\n", ospath );
	}

	FS_CheckFilenameIsMutable( ospath, __func__ );

	if( FS_CreatePath( ospath ) ) {
		return 0;
	}

	fsh[f].handleFiles.file.o = Sys_FOpen( ospath, "wt" );

	Q_strncpyz( fsh[f].name, filename, sizeof( fsh[f].name ) );

	fsh[f].handleSync = qfalse;
	if (!fsh[f].handleFiles.file.o) {
		f = 0;
	}
	return f;
}

fileHandle_t FS_FOpenTextFileWrite_HomeConfig( const char *filename ) {
	return FS_OSPath_FOpenTextFileWrite(
		FS_BaseDir_BuildOSPath(fs_homeconfigpath->string, filename), filename);
}

fileHandle_t FS_FOpenTextFileWrite_HomeData( const char *filename ) {
	return FS_OSPath_FOpenTextFileWrite(
		FS_BaseDir_BuildOSPath(fs_homedatapath->string, filename), filename);
}

fileHandle_t FS_FOpenTextFileWrite_HomeState( const char *filename ) {
	return FS_OSPath_FOpenTextFileWrite(
		FS_BaseDir_BuildOSPath(fs_homestatepath->string, filename), filename);
}

qboolean FS_FilenameCompare( const char *s1, const char *s2 ) {
	int		c1, c2;

	do {
		c1 = *s1++;
		c2 = *s2++;

		if (c1 >= 'a' && c1 <= 'z') {
			c1 -= ('a' - 'A');
		}
		if (c2 >= 'a' && c2 <= 'z') {
			c2 -= ('a' - 'A');
		}

		if ( c1 == '\\' || c1 == ':' ) {
			c1 = '/';
		}
		if ( c2 == '\\' || c2 == ':' ) {
			c2 = '/';
		}

		if (c1 != c2) {
			return qtrue;		
		}
	} while (c1);

	return qfalse;		
}


qboolean FS_IsExt(const char *filename, const char *ext, int namelen)
{
	int extlen;

	extlen = strlen(ext);

	if(extlen > namelen)
		return qfalse;

	filename += namelen - extlen;

	return !Q_stricmp(filename, ext);
}


qboolean FS_IsDemoExt(const char *filename, int namelen)
{
	return FS_IsExt(filename, ".dm3", namelen);
}

#define FS_PREFETCH_MAX_BYTES (16 * 1024 * 1024)

static void FS_PrefetchRange( FILE *f, long offset, long len ) {
#ifdef EF2_HAVE_FADVISE
	if ( !f || len <= 0 || offset < 0 ) {
		return;
	}
	if ( len > FS_PREFETCH_MAX_BYTES ) {
		len = FS_PREFETCH_MAX_BYTES;
	}
	(void)posix_fadvise( fileno( f ), (off_t)offset, (off_t)len, POSIX_FADV_WILLNEED );
#else
	(void)f; (void)offset; (void)len;
#endif
}

static void FS_PrefetchZipDirectory( const char *zipfile ) {
#ifdef EF2_HAVE_FADVISE
	unsigned char eocd[22];
	long size;
	unsigned cdSize, cdOff;
	FILE *f;

	if ( !zipfile || !zipfile[0] ) {
		return;
	}
	f = Sys_FOpen( zipfile, "rb" );
	if ( !f ) {
		return;
	}
	if ( fseek( f, 0, SEEK_END ) == 0 ) {
		size = ftell( f );
		if ( size >= (long)sizeof( eocd ) && fseek( f, size - (long)sizeof( eocd ), SEEK_SET ) == 0
			&& fread( eocd, 1, sizeof( eocd ), f ) == sizeof( eocd )
			&& eocd[0] == 'P' && eocd[1] == 'K' && eocd[2] == 5 && eocd[3] == 6 ) {
			cdSize = (unsigned)eocd[12] | ( (unsigned)eocd[13] << 8 ) | ( (unsigned)eocd[14] << 16 ) | ( (unsigned)eocd[15] << 24 );
			cdOff  = (unsigned)eocd[16] | ( (unsigned)eocd[17] << 8 ) | ( (unsigned)eocd[18] << 16 ) | ( (unsigned)eocd[19] << 24 );
			if ( cdOff != 0xFFFFFFFFu && cdSize != 0xFFFFFFFFu && cdSize > 0 && (long)cdOff < size ) {
				FS_PrefetchRange( f, (long)cdOff, (long)cdSize );
			}
		}
	}
	fclose( f );
#else
	(void)zipfile;
#endif
}

extern qboolean		com_fullyInitialized;

static int fs_existsProbe;

static qboolean FS_ShowReadDiagnostic(qboolean quietProbe) {
	if (quietProbe) {
		return qfalse;
	}
	if (developer && developer->integer == 2 && Com_DoneLoading()) {
		return qtrue;
	}
	return fs_debug && fs_debug->integer;
}

static void FS_ReadDiagnostic(const char *fmt, const char *filename, const char *where, const char *where2) {
	if (Com_DoneLoading()) {
		Com_WPrintf(fmt, filename, where, where2);
	} else {
		Com_Printf(fmt, filename, where, where2);
	}
}

long FS_FOpenFileReadDir(const char *filename, searchpath_t *search, fileHandle_t *file, qboolean uniqueFILE, qboolean unpure)
{
	long			hash;
	pack_t		*pak;
	fileInPack_t	*pakFile;
	directory_t	*dir;
	char		*netpath;
	FILE		*filep;
	int			len;

	if(filename == NULL)
		Com_Error(ERR_FATAL, "FS_FOpenFileRead: NULL 'filename' parameter passed");

	if(filename[0] == '/' || filename[0] == '\\')
		filename++;

	if(strstr(filename, ".." ) || strstr(filename, "::"))
	{
		if(file == NULL)
			return qfalse;

		*file = 0;
		return -1;
	}

	if(com_fullyInitialized && strstr(filename, "q3key"))
	{
		if(file == NULL)
			return qfalse;

		*file = 0;
		return -1;
	}

	if(file == NULL)
	{

		if(search->pack)
		{
			hash = FS_HashFileName(filename, MAX_FILEHASH_SIZE);

			if(search->pack->hashTable[hash])
			{
				pak = search->pack;
				pakFile = pak->hashTable[hash];

				do
				{
					if(!FS_FilenameCompare(pakFile->name, filename))
					{
						if(pakFile->len)
							return pakFile->len;
						else
						{
							return 1;
						}
					}

					pakFile = pakFile->next;
				} while(pakFile != NULL);
			}
		}
		else if(search->dir)
		{
			dir = search->dir;

			netpath = FS_BuildOSPath(dir->path, dir->gamedir, filename);
			filep = Sys_FOpen(netpath, "rb");

			if(filep)
			{
				len = FS_fplength(filep);
				fclose(filep);

				if(len)
					return len;
				else
					return 1;
			}
		}

		return 0;
	}

	*file = FS_HandleForFile();
	fsh[*file].handleFiles.unique = uniqueFILE;

	if(search->pack)
	{
		hash = FS_HashFileName(filename, MAX_FILEHASH_SIZE);

		if(search->pack->hashTable[hash])
		{
			if(!unpure && !FS_PakIsPure(search->pack))
			{
				*file = 0;
				return -1;
			}

			pak = search->pack;
			pakFile = pak->hashTable[hash];

			do
			{
				if(!FS_FilenameCompare(pakFile->name, filename))
				{

					pak->referenced = FS_GENERAL_REF;

					if(uniqueFILE)
					{
						fsh[*file].handleFiles.file.z = unzOpen(pak->pakFilename);

						if(fsh[*file].handleFiles.file.z == NULL)
						{
							Com_DPrintf(
								"FS_FOpenFileRead: unzOpen( %s ) failed, errno %d (%s)\n",
								pak->pakFilename,
								errno,
								strerror(errno)
							);
							Com_Error(ERR_FATAL, "Couldn't open %s", pak->pakFilename);
						}
					}
					else
						fsh[*file].handleFiles.file.z = pak->handle;

					Q_strncpyz(fsh[*file].name, filename, sizeof(fsh[*file].name));
					fsh[*file].zipFile = qtrue;

					unzSetOffset(fsh[*file].handleFiles.file.z, pakFile->pos);

					unzOpenCurrentFile(fsh[*file].handleFiles.file.z);
					fsh[*file].zipFilePos = pakFile->pos;
					fsh[*file].zipFileLen = pakFile->len;

					if (FS_ShowReadDiagnostic(qfalse)) {
						FS_ReadDiagnostic("FS_FOpenFileRead: %s (found in '%s')\n",
								filename, pak->pakFilename, "");
					}

					return pakFile->len;
				}

				pakFile = pakFile->next;
			} while(pakFile != NULL);
		}
	}
	else if(search->dir)
	{

		len = strlen(filename);
		if(!unpure && fs_numServerPaks)
		{
			if(!FS_IsExt(filename, ".cfg", len) &&		
			   !FS_IsExt(filename, ".menu", len) &&		
			   !FS_IsExt(filename, ".game", len) &&		
			   !FS_IsExt(filename, ".dat", len) &&		
			   !FS_IsExt(filename, ".wav", len) &&		
			   !FS_IsExt(filename, ".mp3", len) &&		
			   !FS_IsExt(filename, ".ogg", len) &&		
			   !FS_IsDemoExt(filename, len)			
				)
			{
				*file = 0;
				return -1;
			}
		}

		dir = search->dir;

		netpath = FS_BuildOSPath(dir->path, dir->gamedir, filename);
		filep = Sys_FOpen(netpath, "rb");

		if (filep == NULL)
		{
			*file = 0;
			return -1;
		}

		Q_strncpyz(fsh[*file].name, filename, sizeof(fsh[*file].name));
		fsh[*file].zipFile = qfalse;

		if (FS_ShowReadDiagnostic(fs_existsProbe != 0)) {
			FS_ReadDiagnostic("FS_FOpenFileRead: %s (found in '%s/%s')\n", filename,
					dir->path, dir->gamedir);
		}

		if (fs_copyfiles && fs_copyfiles->integer && !fs_existsProbe && fs_cdpath
			&& !strcmp(dir->path, fs_cdpath->string)) {
			FS_CopyFile(netpath,
					FS_BuildOSPath(fs_homeconfigpath->string, dir->gamedir, filename));
		}

		fsh[*file].handleFiles.file.o = filep;
		{
			long fplen = FS_fplength( filep );

			FS_PrefetchRange( filep, 0, fplen );
			return fplen;
		}
	}

	*file = 0;
	return -1;
}

long FS_FOpenFileRead(const char *filename, fileHandle_t *file, qboolean uniqueFILE, qboolean quiet)
{
	searchpath_t *search;
	long len;
	qboolean isLocalConfig;
	char locbuf[MAX_QPATH];

	if(!fs_searchpaths)
		Com_Error(ERR_FATAL, "Filesystem call made without initialization\n");

	if (!Q_stricmpn(filename, "localization/", 13)) {
		const char *lang = Cvar_VariableString("local_language")  ;
		if (!lang || !lang[0]) {
			lang = "Eng";
		}
		Com_sprintf(locbuf, sizeof(locbuf), "loc/%s/%s", lang, filename + 13);
		filename = locbuf;
	}

	isLocalConfig = !strcmp(filename, "autoexec.cfg") || !strcmp(filename, Q3CONFIG_CFG);
	for(search = fs_searchpaths; search; search = search->next)
	{
		if (isLocalConfig && search->pack)
			continue;

		len = FS_FOpenFileReadDir(filename, search, file, uniqueFILE, qfalse);

		if(file == NULL)
		{
			if(len > 0)
				return len;
		}
		else if (*file)
		{
			if(len >= 0)
				return len;
			FS_FCloseFile(*file);
		}
	}
	
#ifdef FS_MISSING
	if(missingFiles)
		fprintf(missingFiles, "%s\n", filename);
#endif

    if (!quiet) {
        Com_WDPrintf("^~^~^ Can't find %s\n", filename);
    }

	if(file)
	{
		*file = 0;
		return -1;
	}
	else
	{
		return 0;
	}
}

size_t FS_Read( void *buffer, size_t len, fileHandle_t f ) {
	size_t	block, remaining;
	size_t	read;
	byte	*buf;
	int		tries;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization\n" );
	}

	if ( !f ) {
		return 0;
	}

	buf = (byte *)buffer;
	fs_readCount += len;

	if (fsh[f].zipFile == qfalse) {
		remaining = len;
		tries = 0;
		while (remaining) {
			block = remaining;
			read = fread (buf, 1, block, fsh[f].handleFiles.file.o);
			if (read == 0) {
				if (!tries) {
					tries = 1;
				} else {
					return len-remaining;	
				}
			}

			if (read == (size_t)-1) {
				Com_Error (ERR_FATAL, "FS_Read: -1 bytes read");
			}

			remaining -= read;
			buf += read;
		}
		return len;
	} else {
		if (!unzIsCurrentFileOpen(fsh[f].handleFiles.file.z)) {
			Com_Error(ERR_FATAL, "Tried to read from a zip file %s that was not open\n", fsh[f].name);
		}
		return unzReadCurrentFile(fsh[f].handleFiles.file.z, buffer, ( unsigned int )len);
	}
}

size_t FS_Write( const void *buffer, size_t len, fileHandle_t h ) {
	size_t	block, remaining;
	size_t	written;
	byte	*buf;
	int		tries;
	FILE	*f;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization\n" );
	}

	if ( !h ) {
		return 0;
	}

	f = FS_FileForHandle(h);
	buf = (byte *)buffer;

	remaining = len;
	tries = 0;
	while (remaining) {
		block = remaining;
		written = fwrite (buf, 1, block, f);
		if (written == 0) {
			if (!tries) {
				tries = 1;
			} else {
				Com_WPrintf( "FS_Write: 0 bytes written\n" );
				return 0;
			}
		}

		if (written == (size_t)-1) {
			Com_WPrintf( "FS_Write: -1 bytes written\n" );
			return 0;
		}

		remaining -= written;
		buf += written;
	}
	if ( fsh[h].handleSync ) {
		fflush( f );
	}
	return len;
}

void QDECL FS_Printf( fileHandle_t h, const char *fmt, ... ) {
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start (argptr,fmt);
	Q_vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	FS_Write(msg, strlen(msg), h);
}

#define PK3_SEEK_BUFFER_SIZE 65536

int FS_Seek( fileHandle_t f, long offset, int origin ) {
	int		whence;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization\n" );
		return -1;
	}

	switch( origin ) {
		case FS_SEEK_END:	whence = SEEK_END;	break;
		case FS_SEEK_SET:	whence = SEEK_SET;	break;
		case FS_SEEK_CUR:	whence = SEEK_CUR;	break;
		default:
			whence = SEEK_CUR;
			Com_Error( ERR_FATAL, "Bad origin in FS_Seek\n" );
			break;
	}

	if ( fsh[f].zipFile == qtrue ) {
		if ( !unzIsCurrentFileOpen( fsh[f].handleFiles.file.z ) ) {
			Com_Error( ERR_FATAL, "Tried to read from a zip file %s that was not open\n", fsh[f].name );
		}
		return unzseek( fsh[f].handleFiles.file.z, offset, whence );
	}

	return fseek( FS_FileForHandle( f ), offset, whence );
}



int	FS_FileIsInPAK(const char *filename, int *pChecksum ) {
	searchpath_t	*search;
	pack_t			*pak;
	fileInPack_t	*pakFile;
	long			hash = 0;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization\n" );
	}

	if ( !filename ) {
		Com_Error( ERR_FATAL, "FS_FOpenFileRead: NULL 'filename' parameter passed\n" );
	}

	if ( filename[0] == '/' || filename[0] == '\\' ) {
		filename++;
	}

	if ( strstr( filename, ".." ) || strstr( filename, "::" ) ) {
		return -1;
	}


	for ( search = fs_searchpaths ; search ; search = search->next ) {
		if (search->pack) {
			hash = FS_HashFileName(filename, MAX_FILEHASH_SIZE);
		}
		if ( search->pack && search->pack->hashTable[hash] ) {
			if ( !FS_PakIsPure(search->pack) ) {
				continue;
			}

			pak = search->pack;
			pakFile = pak->hashTable[hash];
			do {
				if ( !FS_FilenameCompare( pakFile->name, filename ) ) {
					if (pChecksum) {
						*pChecksum = pak->pure_checksum;
					}
					return 1;
				}
				pakFile = pakFile->next;
			} while(pakFile != NULL);
		}
	}
	return -1;
}

long FS_ReadFileEx( const char *qpath, void **buffer, qboolean quiet ) {
	fileHandle_t	h;
	byte*			buf;
	qboolean		isConfig;
	int				len;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization\n" );
	}

	if ( !qpath || !qpath[0] ) {

        Com_DPrintf("FS_ReadFile with empty name\n");
        return -1;
	}

	buf = NULL;	

	if ( strstr( qpath, ".cfg" ) ) {
		isConfig = qtrue;
		if ( com_journal && com_journal->integer == 2 ) {
			size_t r;

			Com_DPrintf( "Loading %s from journal file.\n", qpath );
			r = FS_Read( &len, sizeof( len ), com_journalDataFile );
			if ( r != sizeof( len ) ) {
				if (buffer != NULL) *buffer = NULL;
				return -1;
			}
			if (!len) {
				if (buffer == NULL) {
					return 1;			
				}
				*buffer = NULL;
				return -1;
			}
			if (buffer == NULL) {
				return len;
			}

			buf = (byte*)Hunk_AllocateTempMemory(len+1);
			*buffer = buf;

			r = FS_Read( buf, len, com_journalDataFile );
			if ( r != (size_t)len ) {
				Com_Error( ERR_FATAL, "Read from journalDataFile failed" );
			}

			fs_loadCount++;
			fs_loadStack++;

			buf[len] = 0;

			return len;
		}
	} else {
		isConfig = qfalse;
	}

	{
		SMP_LOAD_BEGIN( fs );
		len = FS_FOpenFileRead( qpath, &h, qfalse, quiet );
		SMP_LOAD_END( fs );
	}
	if ( h == 0 ) {
		if ( buffer ) {
			*buffer = NULL;
		}
		if ( isConfig && com_journal && com_journal->integer == 1 ) {
			Com_DPrintf( "Writing zero for %s to journal file.\n", qpath );
			len = 0;
			FS_Write( &len, sizeof( len ), com_journalDataFile );
			FS_Flush( com_journalDataFile );
		}
		return -1;
	}

	if ( !buffer ) {
		if ( isConfig && com_journal && com_journal->integer == 1 ) {
			Com_DPrintf( "Writing len for %s to journal file.\n", qpath );
			FS_Write( &len, sizeof( len ), com_journalDataFile );
			FS_Flush( com_journalDataFile );
		}
		FS_FCloseFile( h);
		return len;
	}

	fs_loadCount++;
	fs_loadStack++;

	buf = (byte*)Hunk_AllocateTempMemory(len+1);
	*buffer = buf;

	{
		SMP_LOAD_BEGIN( fs );
		FS_Read (buf, len, h);

		buf[len] = 0;
		FS_FCloseFile( h );
		SMP_LOAD_END( fs );
	}

	if ( isConfig && com_journal && com_journal->integer == 1 ) {
		Com_DPrintf( "Writing %s to journal file.\n", qpath );
		FS_Write( &len, sizeof( len ), com_journalDataFile );
		FS_Write( buf, len, com_journalDataFile );
		FS_Flush( com_journalDataFile );
	}
	return len;
}

void FS_GetDirectoryNames( const char *dirname, void *buf, int len ) {
    (void)dirname;
    (void)buf;
    (void)len;
}

long FS_ReadFile( const char *qpath, void **buffer ) {
	return FS_ReadFileEx( qpath, buffer, qfalse );
}

qboolean FS_Exists( const char *qpath ) {
	qboolean found;

	fs_existsProbe = 1;
	found = FS_ReadFileEx( qpath, NULL, qtrue ) >= 0;
	fs_existsProbe = 0;

	return found;
}

void FS_CopyFile( const char *from, const char *to ) {
	FILE	*fsrc;
	FILE	*fdst;
	char	*buf;
	long	len;
	char	part[MAX_OSPATH];
	char	*p;

	Com_Printf( "copy %s to %s\n", from, to );

	fsrc = Sys_FOpen( from, "rb" );
	if ( !fsrc ) {
		return;
	}

	fseek( fsrc, 0, SEEK_END );
	len = ftell( fsrc );
	fseek( fsrc, 0, SEEK_SET );

	buf = (char *)Z_Malloc( (size_t)len + 1 );
	if ( fread( buf, 1, (size_t)len, fsrc ) != (size_t)len ) {
		Com_Error( ERR_FATAL, "FS_CopyFile: short read on %s\n", from );
	}
	fclose( fsrc );

	if ( strstr( to, ".." ) || strstr( to, "::" ) ) {
		Com_WPrintf( "illegal path name %s\n", to );
		Z_Free( buf );
		return;
	}

	Q_strncpyz( part, to, sizeof( part ) );
	for ( p = part + 1; *p; p++ ) {
		if ( *p == PATH_SEP ) {
			*p = 0;
			if ( part[0] ) {
				Sys_Mkdir( part );
			}
			*p = PATH_SEP;
		}
	}

	fdst = Sys_FOpen( to, "wb" );
	if ( !fdst ) {
		Z_Free( buf );
		return;
	}
	if ( fwrite( buf, 1, (size_t)len, fdst ) != (size_t)len ) {
		Com_Error( ERR_FATAL, "FS_CopyFile: short write on %s\n", to );
	}
	fclose( fdst );

	Z_Free( buf );
}

void Com_LocalizeFilePath( const char *path, char *localizedPath )
{
	cvar_t			*lang;
	const char		*l;
	const char		*rest;
	char			candidate[72];

	if ( !path || !path[0] || !localizedPath ) {
		return;
	}

	if ( strnicmp( path, "localization", 12 ) ) {
		strcpy( localizedPath, path );
		return;
	}

	lang = Cvar_Get( "local_language", "Eng", 1 );
	if ( !lang ) {
		Com_WPrintf( "WARNING: No language defined for localization." );
		return;
	}

	rest = strstr( path, "/" );
	if ( !rest ) {
		Com_WPrintf( "WARNING: Invalid path %s\n", path );
		return;
	}

	l = lang->string[0] ? lang->string : lang->resetString;

	if ( (int)strlen( path ) >= (int)strlen( "loc/" ) &&
			!strnicmp( path + strlen( "loc/" ), l, strlen( l ) ) ) {
		strcpy( localizedPath, path );
		return;
	}

	if ( ( strlen( path ) + strlen( l ) + 1 ) < 0x40 ) {
		Com_sprintf( candidate, sizeof( candidate ), "loc/%s%s", l, rest );
		strcpy( localizedPath, candidate );
		return;
	}

	Com_WPrintf( "WARNING: path %s is too long when converting to localization directory.", path );
	localizedPath[0] = '\0';
}

const char *FS_Gamedir( void )
{
	return fs_gamedir;
}

void FS_FreeFile( void *buffer ) {
	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization\n" );
	}
	if ( !buffer ) {
		Com_Error( ERR_FATAL, "FS_FreeFile( NULL )" );
	}
	fs_loadStack--;

	Hunk_FreeTempMemory( buffer );

}

const char *FS_PrepFileWrite( const char *filename ) {
	static char ospath[ 256 ];

	Com_sprintf( ospath, sizeof( ospath ), "%s/%s", fs_gamedir, filename );
	FS_ReplaceSeparators( ospath );
	FS_CreatePath( ospath );
	return ospath;
}

int FS_WriteFile( const char *qpath, const void *buffer, int size ) {
	fileHandle_t f;
	size_t len;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization\n" );
	}

	if ( !qpath || !buffer ) {
		Com_Error( ERR_FATAL, "FS_WriteFile: NULL parameter" );
	}

	f = FS_FOpenFileWrite_HomeData( qpath );
	if ( !f ) {
		Com_Printf( "Failed to open %s\n", qpath );
		return 0;
	}

	len = FS_Write( buffer, size, f );

	FS_FCloseFile( f );

	return len;
}

void FS_WriteTextFile( const char *qpath, const void *buffer, int size ) {
	fileHandle_t f;

	f = FS_FOpenTextFileWrite_HomeData( qpath );
	if( !f ) {
		Com_Printf( "Failed to open %s\n", qpath );
		return;
	}

	FS_Write( buffer, size, f );
	FS_FCloseFile( f );
}



/* Retail parses each pak's central directory once per FS_Startup; a restart
 * re-walks the same unchanged paks.  A retired pack keeps its parsed directory,
 * hash table and CRC words, so only the two feed-dependent checksums are redone. */
#define FS_PAKCACHE_MAX 64

static pack_t	*fs_pakCache[FS_PAKCACHE_MAX];

static qboolean FS_ZipIdentity( const char *zipfile, long *sizeOut, long *timeOut ) {
#ifdef _WIN32
	struct _stat st;

	if ( _stat( zipfile, &st ) != 0 ) {
		return qfalse;
	}
#else
	struct stat st;

	if ( stat( zipfile, &st ) != 0 ) {
		return qfalse;
	}
#endif
	*sizeOut = (long)st.st_size;
	*timeOut = (long)st.st_mtime;
	return qtrue;
}

static void FS_PakSetBasename( pack_t *pack, const char *basename ) {
	Q_strncpyz( pack->pakBasename, basename, sizeof( pack->pakBasename ) );

	if ( strlen( pack->pakBasename ) > 4
		 && !Q_stricmp( pack->pakBasename + strlen( pack->pakBasename ) - 4, ".pk3" ) ) {
		pack->pakBasename[strlen( pack->pakBasename ) - 4] = 0;
	}
}

static void FS_PakSetChecksums( pack_t *pack ) {
	if ( !pack->crcWords || pack->numCrcWords < 1 ) {
		return;
	}

	pack->crcWords[0]   = LittleLong( fs_checksumFeed );
	pack->checksum      = LittleLong( Com_BlockChecksum( pack->crcWords, 4 * pack->numCrcWords ) );
	pack->pure_checksum = LittleLong( Com_BlockChecksumKey( pack->crcWords, 4 * pack->numCrcWords,
															LittleLong( fs_checksumFeed ) ) );
}

static pack_t *FS_PakCacheTake( const char *pakFilename, long fileSize, long fileTime ) {
	int i;

	for ( i = 0; i < FS_PAKCACHE_MAX; i++ ) {
		pack_t *pack = fs_pakCache[i];

		if ( !pack ) {
			continue;
		}
		if ( !Q_stricmp( pack->pakFilename, pakFilename )
			 && pack->fileSize == fileSize && pack->fileTime == fileTime ) {
			fs_pakCache[i] = NULL;
			return pack;
		}
	}
	return NULL;
}

static qboolean FS_PakCachePut( pack_t *pack ) {
	int i;

	for ( i = 0; i < FS_PAKCACHE_MAX; i++ ) {
		if ( fs_pakCache[i] == pack ) {
			return qtrue;
		}
	}
	for ( i = 0; i < FS_PAKCACHE_MAX; i++ ) {
		if ( !fs_pakCache[i] ) {
			fs_pakCache[i] = pack;
			return qtrue;
		}
	}
	return qfalse;
}

static void FS_FreePack( pack_t *pack ) {
	if ( !pack ) {
		return;
	}
	if ( pack->handle ) {
		unzClose( pack->handle );
	}
	Z_Free( pack->buildBuffer );
	Z_Free( pack->crcWords );
	Z_Free( pack );
}

static void FS_PakCacheRelease( void ) {
	int i;

	for ( i = 0; i < FS_PAKCACHE_MAX; i++ ) {
		FS_FreePack( fs_pakCache[i] );
		fs_pakCache[i] = NULL;
	}
}

static pack_t *FS_LoadZipFile( const char *zipfile, const char *basename )
{
	fileInPack_t	*buildBuffer;
	pack_t			*pack;
	unzFile			uf;
	int				err;
	unz_global_info gi;
	char			filename_inzip[MAX_ZPATH];
	unz_file_info	file_info;
	int				i;
	size_t			len;
	long			hash;
	int				fs_numHeaderLongs;
	int				*fs_headerLongs;
	char			*namePtr;
	long			fileSize, fileTime;

	if ( FS_ZipIdentity( zipfile, &fileSize, &fileTime ) ) {
		pack = FS_PakCacheTake( zipfile, fileSize, fileTime );
		if ( pack ) {
			FS_PakSetBasename( pack, basename );
			FS_PakSetChecksums( pack );
			return pack;
		}
	} else {
		fileSize = fileTime = 0;
	}

	fs_numHeaderLongs = 0;

	uf = unzOpen(zipfile);
	FS_PrefetchZipDirectory( zipfile );
	err = unzGetGlobalInfo (uf,&gi);

	if (err != UNZ_OK) {
		if (uf) {
			unzClose(uf);
		}
		return NULL;
	}

	len = 0;
	unzGoToFirstFile(uf);
	for (i = 0; (uLong)i < gi.number_entry; i++)
	{
		err = unzGetCurrentFileInfo(uf, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0);
		if (err != UNZ_OK) {
			break;
		}
		len += strlen(filename_inzip) + 1;
		unzGoToNextFile(uf);
	}

	buildBuffer = (fileInPack_t*)Z_Malloc( (gi.number_entry * sizeof( fileInPack_t )) + len );
	namePtr = ((char *) buildBuffer) + gi.number_entry * sizeof( fileInPack_t );
	fs_headerLongs = (int*)Z_Malloc( ( gi.number_entry + 1 ) * sizeof(int) );
	fs_headerLongs[ fs_numHeaderLongs++ ] = LittleLong( fs_checksumFeed );

	pack = (pack_t*)Z_Malloc( sizeof( pack_t ) );
	for(i = 0; i < MAX_FILEHASH_SIZE; i++) {
		pack->hashTable[i] = NULL;
	}

	Q_strncpyz( pack->pakFilename, zipfile, sizeof( pack->pakFilename ) );
	FS_PakSetBasename( pack, basename );

	pack->handle = uf;
	pack->numfiles = gi.number_entry;
	pack->buildBuffer = NULL;
	pack->crcWords = fs_headerLongs;
	pack->numCrcWords = 0;
	pack->fileSize = fileSize;
	pack->fileTime = fileTime;
	unzGoToFirstFile(uf);

	for (i = 0; (uLong)i < gi.number_entry; i++)
	{
		err = unzGetCurrentFileInfo(uf, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0);
		if (err != UNZ_OK) {
			break;
		}
		if (file_info.uncompressed_size > 0) {
			fs_headerLongs[fs_numHeaderLongs++] = LittleLong(file_info.crc);
		}
		Q_strlwr( filename_inzip );
		hash = FS_HashFileName(filename_inzip, MAX_FILEHASH_SIZE);
		buildBuffer[i].name = namePtr;
		strcpy( buildBuffer[i].name, filename_inzip );
		namePtr += strlen(filename_inzip) + 1;
		buildBuffer[i].pos = unzGetOffset(uf);
		buildBuffer[i].len = file_info.uncompressed_size;
		buildBuffer[i].next = pack->hashTable[hash];
		pack->hashTable[hash] = &buildBuffer[i];
		unzGoToNextFile(uf);
	}

	pack->numCrcWords = fs_numHeaderLongs;
	FS_PakSetChecksums( pack );

	pack->buildBuffer = buildBuffer;
	return pack;
}


static void FS_FreePak(pack_t *thepak)
{
	FS_FreePack(thepak);
}

qboolean FS_CompareZipChecksum(const char *zipfile)
{
	pack_t *thepak;
	int index;
	unsigned int checksum;

	thepak = FS_LoadZipFile(zipfile, "");

	if(!thepak)
		return qfalse;

	checksum = thepak->checksum;
	FS_FreePak(thepak);

	for(index = 0; index < fs_numServerReferencedPaks; index++)
	{
		if(checksum == fs_serverReferencedPaks[index])
			return qtrue;
	}

	return qfalse;
}


#define	MAX_FOUND_FILES	0x1000

static int FS_ReturnPath( const char *zname, char *zpath, int *depth ) {
	int len, at, newdep;

	newdep = 0;
	zpath[0] = 0;
	len = 0;
	at = 0;

	while(zname[at] != 0)
	{
		if (zname[at]=='/' || zname[at]=='\\') {
			len = at;
			if (zname[at + 1]) {
				newdep++;
			}
		}
		at++;
	}
	strcpy(zpath, zname);
	zpath[len] = 0;
	*depth = newdep;

	return len;
}

static int FS_AddFileToList( char *name, char **list, int nfiles ) {
	int		i;

	if ( nfiles == MAX_FOUND_FILES - 1 ) {
		return nfiles;
	}
	for ( i = 0 ; i < nfiles ; i++ ) {
		if ( !strcmp( name, list[i] ) ) {
			return nfiles;		
		}
	}
	list[nfiles] = CopyString( name );
	nfiles++;

	return nfiles;
}

char **FS_ListFilteredFiles( const char *path, const char *extension, const char *filter, qboolean wantSubs, int *numfiles, qboolean allowNonPureFilesOnDisk ) {
	int				nfiles;
	char			**listCopy;
	char			*list[MAX_FOUND_FILES];
	searchpath_t	*search;
	int				i;
	int				pathLength;
	int				extensionLength;
	int				length, pathDepth, temp;
	pack_t			*pak;
	fileInPack_t	*buildBuffer;
	char			zpath[MAX_ZPATH];
	qboolean		bDirSearch;
	const char		*pExtToken;
	const char		*pExtension;
	char			currentExtension[MAX_ZPATH];

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization\n" );
	}

	if ( !path ) {
		*numfiles = 0;
		return NULL;
	}
	pExtension = extension;
	if ( !pExtension ) {
		pExtension = "";
	}
	extension = pExtension;

	pathLength = strlen( path );
	if ( path[pathLength-1] == '\\' || path[pathLength-1] == '/' ) {
		pathLength--;
	}
	extensionLength = strlen( extension );
	nfiles = 0;
	FS_ReturnPath(path, zpath, &pathDepth);

	do {
		pExtToken = strstr(pExtension, ";");
		if (pExtToken) {
			extensionLength = pExtToken - pExtension;
			strncpy(currentExtension, pExtension, extensionLength);
			currentExtension[extensionLength] = 0;
			pExtToken++;
		}
		else {
			strcpy(currentExtension, pExtension);
			extensionLength = strlen(currentExtension);
		}

		bDirSearch = qfalse;
		if (currentExtension[0] == '/') {
			bDirSearch = extension[1] == 0;
		}

		for (search = fs_searchpaths ; search ; search = search->next) {
			if (search->pack) {

				if ( !FS_PakIsPure(search->pack) ) {
					continue;
				}

				pak = search->pack;
				buildBuffer = pak->buildBuffer;
				for (i = 0; i < pak->numfiles; i++) {
					char	*name;
					int		zpathLen, depth;

					name = buildBuffer[i].name;
					
					zpathLen = FS_ReturnPath(name, zpath, &depth);

					if ( pathLength > (zpathLen + 1) || Q_stricmpn( name, path, pathLength ) || ((!wantSubs || bDirSearch) && depth - pathDepth != 1) ) {
						continue;
					}

					if (filter) {
						if (zpathLen) {
							strcpy(zpath, name + zpathLen + 1);
						}
						else {
							strcpy(zpath, name);
						}
						if (!Com_FilterPath( filter, zpath, qfalse ))
							continue;
						nfiles = FS_AddFileToList( zpath, list, nfiles );
					}
					else {
						length = strlen( name );
						if ( length < extensionLength ) {
							continue;
						}

						if ( Q_stricmp( name + length - extensionLength, extension ) ) {
							continue;
						}

						temp = pathLength;
						if (pathLength) {
							temp++; 
						}

						if (bDirSearch) {
							strcpy(zpath, name + temp);
							zpath[length - temp - 1] = 0;
							nfiles = FS_AddFileToList(zpath, list, nfiles);
						}
						else {
							nfiles = FS_AddFileToList(name + temp, list, nfiles);
						}
					}
				}
			} else if (search->dir) { 
				char	*netpath;
				int		numSysFiles;
				char	**sysFiles;
				char	*name;

				if ( fs_numServerPaks && !allowNonPureFilesOnDisk ) {
					continue;
				} else {
					netpath = FS_BuildOSPath( search->dir->path, search->dir->gamedir, path );
					sysFiles = Sys_ListFiles( netpath, extension, filter, &numSysFiles, wantSubs );
					for ( i = 0 ; i < numSysFiles ; i++ ) {
						name = sysFiles[i];
						nfiles = FS_AddFileToList( name, list, nfiles );
					}
					Sys_FreeFileList( sysFiles );
				}
			}
		}

		pExtension = pExtToken;
	} while (pExtToken);

	*numfiles = nfiles;

	if ( !nfiles ) {
		return NULL;
	}

	listCopy = (char**)Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ) );
	for ( i = 0 ; i < nfiles ; i++ ) {
		listCopy[i] = list[i];
	}
	listCopy[i] = NULL;

	return listCopy;
}

char **FS_ListFiles( const char *path, const char *extension, qboolean wantSubs, int *numfiles ) {
	return FS_ListFilteredFiles( path, extension, NULL, wantSubs, numfiles, qtrue );
}

void FS_FreeFileList( char **list ) {
	int		i;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization\n" );
	}

	if ( !list ) {
		return;
	}

	for ( i = 0 ; list[i] ; i++ ) {
		Z_Free( list[i] );
	}

	Z_Free( list );
}


int	FS_GetFileList(  const char *path, const char *extension, char *listbuf, int bufsize ) {
	int		nFiles, i;
	size_t	nTotal, nLen;
	char **pFiles = NULL;

	*listbuf = 0;
	nFiles = 0;
	nTotal = 0;

	if (Q_stricmp(path, "$modlist") == 0) {
		return FS_GetModList(listbuf, bufsize);
	}

	pFiles = FS_ListFiles(path, extension, qfalse, &nFiles);

	for (i =0; i < nFiles; i++) {
		nLen = strlen(pFiles[i]) + 1;
		if (nTotal + nLen + 1 < (size_t)bufsize) {
			strcpy(listbuf, pFiles[i]);
			listbuf += nLen;
			nTotal += nLen;
		}
		else {
			nFiles = i;
			break;
		}
	}

	FS_FreeFileList(pFiles);

	return nFiles;
}

static unsigned int Sys_CountFileList(char **list)
{
	int i = 0;

	if (list)
	{
		while (*list)
		{
			list++;
			i++;
		}
	}
	return i;
}

static char** Sys_ConcatenateFileLists( char **list0, char **list1 )
{
	int totalLength = 0;
	char** cat = NULL, **dst, **src;

	totalLength += Sys_CountFileList(list0);
	totalLength += Sys_CountFileList(list1);

	dst = cat = (char**)Z_Malloc( ( totalLength + 1 ) * sizeof( char* ) );

	if (list0)
	{
		for (src = list0; *src; src++, dst++)
			*dst = *src;
	}
	if (list1)
	{
		for (src = list1; *src; src++, dst++)
			*dst = *src;
	}

	*dst = NULL;

	// NOTE: not freeing their content, it's been merged in dst and still being used
	if (list0) Z_Free( list0 );
	if (list1) Z_Free( list1 );

	return cat;
}

void FS_GetModDescription( const char *modDir, char *description, int descriptionLen ) {
	fileHandle_t	descHandle;
	char			descPath[MAX_QPATH];
	int				nDescLen;
	FILE			*file;

	Com_sprintf( descPath, sizeof ( descPath ), "%s%cdescription.txt", modDir, PATH_SEP );
	nDescLen = FS_BaseDir_FOpenFileRead( descPath, &descHandle );

	if ( nDescLen > 0 ) {
		file = FS_FileForHandle(descHandle);
		Com_Memset( description, 0, descriptionLen );
		nDescLen = fread(description, 1, descriptionLen, file);
		if (nDescLen >= 0) {
			description[nDescLen] = '\0';
		}
	} else {
		Q_strncpyz( description, modDir, descriptionLen );
	}

	if ( descHandle ) {
		FS_FCloseFile( descHandle );
	}
}

int	FS_GetModList( char *listbuf, int bufsize ) {
	int nMods, i, j, k, nTotal, nLen, nPaks = 0, nDirs = 0, nPakDirs = 0, nPotential, nDescLen;
	char **pFiles = NULL;
	char **pPaks = NULL;
	char **pDirs = NULL;
	char *name, *path;
	char description[MAX_OSPATH];

	int dummy;
	char **pFiles0 = NULL;
	qboolean bDrop = qfalse;

	*listbuf = 0;
	nMods = nTotal = 0;

	for (i = 0; (size_t)i < ARRAY_LEN(fs_pathVars); i++) {
		const cvar_t *pathVar = fs_pathVars[i];

		if (!pathVar || !pathVar->string[0]) {
			continue;
		}

		pFiles0 = Sys_ListFiles(pathVar->string, NULL, NULL, &dummy, qtrue);
		pFiles = Sys_ConcatenateFileLists(pFiles, pFiles0);
	}

	nPotential = Sys_CountFileList(pFiles);

	for (i = 0; i < nPotential; i++) {
		name = pFiles[i];
		if (i != 0) {
			bDrop = qfalse;
			for (j = 0; j < i; j++) {
				if (Q_stricmp(pFiles[j], name) == 0) {
					bDrop = qtrue;
					break;
				}
			}
		}
		if (bDrop || Q_stricmp(name, BASEGAME) == 0 || Q_stricmpn(name, ".", 1) == 0) {
			continue;
		}

		for (j = 0; (size_t)j < ARRAY_LEN(fs_pathVars); j++) {
			const cvar_t *pathVar = fs_pathVars[j];

			if (!pathVar || !pathVar->string[0]) {
				continue;
			}

			path = FS_BaseDir_BuildOSPath(pathVar->string, name);
			nPaks = nDirs = nPakDirs = 0;
			pPaks = Sys_ListFiles(path, ".pk3", NULL, &nPaks, qfalse);
			pDirs = Sys_ListFiles(path, "/", NULL, &nDirs, qfalse);
			for (k = 0; k < nDirs; k++) {
				if (FS_IsExt(pDirs[k], ".pk3dir", strlen(pDirs[k]))) {
					nPakDirs++;
				}
			}
			Sys_FreeFileList(pPaks);
			Sys_FreeFileList(pDirs);

			if (nPaks > 0 || nPakDirs > 0) {
				break;
			}
		}

		if (nPaks > 0 || nPakDirs > 0) {
			nLen = strlen(name) + 1;
			FS_GetModDescription(name, description, sizeof(description));
			nDescLen = strlen(description) + 1;

			if (nTotal + nLen + 1 + nDescLen + 1 < bufsize) {
				strcpy(listbuf, name);
				listbuf += nLen;
				strcpy(listbuf, description);
				listbuf += nDescLen;
				nTotal += nLen + nDescLen;
				nMods++;
			} else {
				break;
			}
		}
	}
	Sys_FreeFileList( pFiles );

	return nMods;
}



void FS_ConvertPath( char *s ) {
	while (*s) {
		if ( *s == '\\' || *s == ':' ) {
			*s = '/';
		}
		s++;
	}
}

int FS_PathCmp( const char *s1, const char *s2 ) {
	int		c1, c2;

	do {
		c1 = *s1++;
		c2 = *s2++;

		if (c1 >= 'A' && c1 <= 'Z') {
			c1 += ('a' - 'A');
		}
		if (c2 >= 'A' && c2 <= 'Z') {
			c2 += ('a' - 'A');
		}

		if ( c1 == '\\' || c1 == ':' ) {
			c1 = '/';
		}
		if ( c2 == '\\' || c2 == ':' ) {
			c2 = '/';
		}

		if (c1 < c2) {
			return -1;		
		}
		if (c1 > c2) {
			return 1;
		}
	} while (c1);

	return 0;		
}

void FS_SortFileList(char **filelist, int numfiles) {
	int i, j, k, numsortedfiles;
	char **sortedlist;

	sortedlist = (char**)Z_Malloc( ( numfiles + 1 ) * sizeof( *sortedlist ) );
	sortedlist[0] = NULL;
	numsortedfiles = 0;
	for (i = 0; i < numfiles; i++) {
		for (j = 0; j < numsortedfiles; j++) {
			if (FS_PathCmp(filelist[i], sortedlist[j]) < 0) {
				break;
			}
		}
		for (k = numsortedfiles; k > j; k--) {
			sortedlist[k] = sortedlist[k-1];
		}
		sortedlist[j] = filelist[i];
		numsortedfiles++;
	}
	Com_Memcpy(filelist, sortedlist, numfiles * sizeof( *filelist ) );
	Z_Free(sortedlist);
}

void FS_Path_f( void ) {
	searchpath_t	*s;
	int				i;

	Com_Printf( "Current search path:\n" );
	for( s = fs_searchpaths; s; s = s->next ) {
		if( s->pack ) {
			Com_Printf( "%s (%i files)\n", s->pack->pakFilename, s->pack->numfiles );
			if( fs_numServerPaks ) {
				if( !FS_PakIsPure( s->pack ) ) {
					Com_Printf( "    not on the pure list\n" );
				}
				else {
					Com_Printf( "    on the pure list\n" );
				}
			}
		}
		else {
			Com_Printf( "%s/%s\n", s->dir->path, s->dir->gamedir );
		}
	}


	Com_Printf( "\n" );
	for( i = 1; i < MAX_FILE_HANDLES; i++ ) {
		if( fsh[ i ].handleFiles.file.o ) {
			Com_Printf( "handle %i: %s\n", i, fsh[ i ].name );
		}
	}
}

void FS_TouchFile_f( void ) {
	fileHandle_t	f;

	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "Usage: touchFile <file>\n" );
		return;
	}

	FS_FOpenFileRead( Cmd_Argv( 1 ), &f, qfalse, qfalse );
	if ( f ) {
		FS_FCloseFile( f );
	}
}



// but the pak mount order must follow retail's, so this comparator is strcmp.
static int QDECL paksort(const void *a, const void *b) {
	char	*aa, *bb;

	aa = *(char **)a;
	bb = *(char **)b;

	return strcmp( aa, bb );
}

static Q_UNUSED_VAR void FS_SortOriginalPaks(char **pakfiles, int numfiles, const char** originalPaks, int numOriginalPaks)
{
	int i, j;
	int numSorted = numfiles;
	char* tmp;

	for (i = numfiles - 1; i >= 0; i--)
	{
		for (j = 0; j < numOriginalPaks; j++)
		{
			if (!stricmp(pakfiles[i], originalPaks[j]))
			{
				tmp = pakfiles[numSorted - 1];
				pakfiles[numSorted - 1] = pakfiles[i];
				pakfiles[i] = tmp;
				numSorted--;
			}
		}
	}
}

void FS_AddGameDirectory(const char *path, const char *dir) {
	searchpath_t	*sp;
	searchpath_t	*search;
	pack_t			*pak;
	char			curpath[MAX_OSPATH + 1], *pakfile;
	int				numfiles;
	char			**pakfiles;
	int				pakfilesi;
	char			**pakfilestmp;
	int				numdirs;
	char			**pakdirs;
	int				pakdirsi;
	char			**pakdirstmp;

	int				pakwhich;
	int				len;

	for ( sp = fs_searchpaths ; sp ; sp = sp->next ) {
		if ( sp->dir && !Q_stricmp(sp->dir->path, path) && !Q_stricmp(sp->dir->gamedir, dir)) {
			return;			
		}
	}

	Q_strncpyz( fs_gamedir, dir, sizeof( fs_gamedir ) );

	Q_strncpyz(curpath, FS_BaseDir_BuildOSPath(path, dir), sizeof(curpath));

	pakfiles = Sys_ListFiles(curpath, ".pk3", NULL, &numfiles, qfalse);

	if ( pakfiles ) {
		qsort( pakfiles, numfiles, sizeof(char*), paksort );
	}

	if ( fs_numServerPaks ) {
		numdirs = 0;
		pakdirs = NULL;
	} else {
		pakdirs = Sys_ListFiles(curpath, "/", NULL, &numdirs, qfalse);

		qsort( pakdirs, numdirs, sizeof(char *), paksort );
	}

	pakfilesi = 0;
	pakdirsi = 0;

	while((pakfilesi < numfiles) || (pakdirsi < numdirs))
	{
		if (pakfilesi >= numfiles) {
			pakwhich = 0;
		}
		else if (pakdirsi >= numdirs) {
			pakwhich = 1;
		}
		else {
			pakfilestmp = &pakfiles[pakfilesi];
			pakdirstmp = &pakdirs[pakdirsi];
			pakwhich = (paksort(pakfilestmp, pakdirstmp) < 0);
		}

		if (pakwhich) {
			pakfile = FS_BuildOSPath(path, dir, pakfiles[pakfilesi]);
			if ((pak = FS_LoadZipFile(pakfile, pakfiles[pakfilesi])) == 0) {
				pakfilesi++;
				continue;
			}

			Q_strncpyz(pak->pakGamename, dir, sizeof(pak->pakGamename));

			fs_packFiles += pak->numfiles;

			search = (searchpath_t*)Z_Malloc(sizeof(searchpath_t));
			search->pack = pak;
			search->next = fs_searchpaths;
			fs_searchpaths = search;

			pakfilesi++;
		}
		else {
			len = strlen(pakdirs[pakdirsi]);
			if (!FS_IsExt(pakdirs[pakdirsi], ".pk3dir", len)) {
				pakdirsi++;
				continue;
			}

			pakfile = FS_BuildOSPath(path, dir, pakdirs[pakdirsi]);

			search = (searchpath_t*)Z_Malloc(sizeof(searchpath_t));
			search->dir = (directory_t*)Z_Malloc(sizeof(*search->dir));

			Q_strncpyz(search->dir->path, curpath, sizeof(search->dir->path));	
			Q_strncpyz(search->dir->fullpath, pakfile, sizeof(search->dir->fullpath));	
			Q_strncpyz(search->dir->gamedir, pakdirs[pakdirsi], sizeof(search->dir->gamedir)); 

			search->next = fs_searchpaths;
			fs_searchpaths = search;

			pakdirsi++;
		}
	}

	Sys_FreeFileList( pakfiles );
	Sys_FreeFileList( pakdirs );

	search = (searchpath_t*)Z_Malloc (sizeof(searchpath_t));
	search->dir = (directory_t*)Z_Malloc( sizeof( *search->dir ) );

	Q_strncpyz(search->dir->path, path, sizeof(search->dir->path));
	Q_strncpyz(search->dir->fullpath, curpath, sizeof(search->dir->fullpath));
	Q_strncpyz(search->dir->gamedir, dir, sizeof(search->dir->gamedir));

	search->next = fs_searchpaths;
	fs_searchpaths = search;
}

static void FS_AddGameDirectories(const char *dir)
{
	for(int i = ARRAY_LEN(fs_pathVars) - 1; i >= 0; i--) {
		const cvar_t *pathVar = fs_pathVars[i];

		if (!pathVar || !pathVar->string[0]) {
			continue;
		}

		FS_AddGameDirectory(pathVar->string, dir);
	}
}

qboolean FS_idPak(const char *pak, const char *base ) {
	int i;

	for (i = 0; i < NUM_ID_PAKS; i++) {
		if ( !FS_FilenameCompare(pak, va("%s/pak%d", base, i)) ) {
			break;
		}
	}
	if (i < NUM_ID_PAKS) {
		return qtrue;
	}
	return qfalse;
}


qboolean FS_CheckDirTraversal(const char *checkdir)
{
	if(strstr(checkdir, "../") || strstr(checkdir, "..\\"))
		return qtrue;
	
	return qfalse;
}

qboolean FS_InvalidGameDir( const char *gamedir ) {
	if ( !strcmp( gamedir, "." ) || !strcmp( gamedir, ".." )
		|| strchr( gamedir, '/' ) || strchr( gamedir, '\\' ) ) {
		return qtrue;
	}

	return qfalse;
}

qboolean FS_ComparePaks( char *neededpaks, int len, qboolean dlstring ) {
	searchpath_t	*sp;
	qboolean havepak;
	char *origpos = neededpaks;
	int i;

	if (!fs_numServerReferencedPaks)
		return qfalse; 

	*neededpaks = 0;

	for ( i = 0 ; i < fs_numServerReferencedPaks ; i++ )
	{
		havepak = qfalse;

		if ( FS_idPak(fs_serverReferencedPakNames[i], BASEGAME) || FS_idPak(fs_serverReferencedPakNames[i], "missionpack") ) {
			continue;
		}

		if(FS_CheckDirTraversal(fs_serverReferencedPakNames[i]))
		{
			Com_Printf("WARNING: Invalid download name %s\n", fs_serverReferencedPakNames[i]);
			continue;
		}

		for ( sp = fs_searchpaths ; sp ; sp = sp->next ) {
			if ( sp->pack && sp->pack->checksum == fs_serverReferencedPaks[i] ) {
				havepak = qtrue; 
				break;
			}
		}

		if ( !havepak && fs_serverReferencedPakNames[i] && *fs_serverReferencedPakNames[i] ) {

			if (dlstring)
			{

				origpos += strlen(origpos);

				Q_strcat( neededpaks, len, "@");
				Q_strcat( neededpaks, len, fs_serverReferencedPakNames[i] );
				Q_strcat( neededpaks, len, ".pk3" );

				Q_strcat( neededpaks, len, "@");
				if ( FS_BaseDir_FileExists_HomeData( va( "%s.pk3", fs_serverReferencedPakNames[i] ) ) )
				{
					char st[MAX_ZPATH];
					Com_sprintf( st, sizeof( st ), "%s.%08x.pk3", fs_serverReferencedPakNames[i], fs_serverReferencedPaks[i] );
					Q_strcat( neededpaks, len, st );
				}
				else
				{
					Q_strcat( neededpaks, len, fs_serverReferencedPakNames[i] );
					Q_strcat( neededpaks, len, ".pk3" );
				}

				if(strlen(origpos) + (origpos - neededpaks) >= (size_t)(len - 1))
				{
					*origpos = '\0';
					break;
				}
			}
			else
			{
				Q_strcat( neededpaks, len, fs_serverReferencedPakNames[i] );
				Q_strcat( neededpaks, len, ".pk3" );
				if ( FS_BaseDir_FileExists_HomeData( va( "%s.pk3", fs_serverReferencedPakNames[i] ) ) )
				{
					Q_strcat( neededpaks, len, " (local file exists with wrong checksum)");
				}
				Q_strcat( neededpaks, len, "\n");
			}
		}
	}

	if ( *neededpaks ) {
		return qtrue;
	}

	return qfalse; 
}

void FS_Shutdown( qboolean closemfp ) {
	searchpath_t	*p, *next;
	int	i;

	for(i = 0; i < MAX_FILE_HANDLES; i++) {
		if (fsh[i].fileSize) {
			FS_FCloseFile(i);
		}
	}

	for ( p = fs_searchpaths ; p ; p = next ) {
		next = p->next;

		if ( p->pack ) {
			/* A restart rebuilds the same paks from the same files; keep the
			 * parsed directory instead of walking it again. */
			if ( closemfp || !FS_PakCachePut( p->pack ) ) {
				FS_FreePack( p->pack );
			}
		}
		if ( p->dir ) {
			Z_Free( p->dir );
		}
		Z_Free( p );
	}

	fs_searchpaths = NULL;

	if ( closemfp ) {
		FS_PakCacheRelease();
	}

	// so the third name retail removes is "cd" -- NOT the port's "fdir".  Retail
	// never registers or removes "fdir"; this restores retail's list.
	Cmd_RemoveCommand( "path" );
	Cmd_RemoveCommand( "dir" );
	Cmd_RemoveCommand( "cd" );
	Cmd_RemoveCommand( "touchFile" );

#ifdef FS_MISSING
	if (closemfp) {
		fclose(missingFiles);
	}
#endif
}

#ifndef STANDALONE
extern "C" void Com_AppendCDKey( const char *filename );
extern "C" void Com_ReadCDKey( const char *filename );
#endif

static void FS_ReorderPurePaks( void )
{
	searchpath_t *s;
	int i;
	searchpath_t **p_insert_index, 
		**p_previous; 

	if ( !fs_numServerPaks )
		return;

	fs_reordered = qfalse;

	p_insert_index = &fs_searchpaths; 
	for ( i = 0 ; i < fs_numServerPaks ; i++ ) {
		p_previous = p_insert_index; 
		for (s = *p_insert_index; s; s = s->next) {
			if (s->pack && fs_serverPaks[i] == s->pack->checksum) {
				fs_reordered = qtrue;
				*p_previous = s->next;
				s->next = *p_insert_index;
				*p_insert_index = s;
				p_insert_index = &s->next;
				break; 
			}
			p_previous = &s->next;
		}
	}
}

static void FS_AddPathVar( cvar_t *pathVar ) {
	if( !pathVar || !*pathVar->string ) {
		return;
	}

	for( int i = 0; (size_t)i < ARRAY_LEN( fs_pathVars ); i++ ) {
		if ( fs_pathVars[i] && !Q_stricmp( fs_pathVars[i]->string, pathVar->string ) ) {
			return;
		}
	}

	for( int i = 0; (size_t)i < ARRAY_LEN( fs_pathVars ); i++ ) {
		if ( !fs_pathVars[i] ) {
			fs_pathVars[i] = pathVar;
			return;
		}
	}

	Com_Error( ERR_FATAL, "FS_AddPathVar: exhausted fs_pathVars array" );
}

static void FS_InitPathVars( void ) {
	memset( fs_pathVars, 0, sizeof( fs_pathVars ) );

	FS_AddPathVar( fs_homeconfigpath );
	FS_AddPathVar( fs_homedatapath );
	FS_AddPathVar( fs_homestatepath );
	FS_AddPathVar( fs_basepath );
	FS_AddPathVar( fs_apppath );
	FS_AddPathVar( fs_steampath );
	FS_AddPathVar( fs_gogpath );
	FS_AddPathVar( fs_microsoftstorepath );
}

static void FS_Startup(const char* gameName)
{
	cvar_t *fs_homepath = Cvar_Get("fs_homepath", "", CVAR_INIT|CVAR_PROTECTED);
	const char *configPath = Sys_DefaultHomeConfigPath();
	const char *dataPath = Sys_DefaultHomeDataPath();
	const char *statePath = Sys_DefaultHomeStatePath();

	if(*(fs_homepath)->string) {
		configPath = dataPath = statePath = fs_homepath->string;
	} else if(!*configPath || !*dataPath || !*statePath) {
		configPath = dataPath = statePath = Sys_DefaultInstallPath();
	}

	Com_Printf( "----- FS_Startup -----\n" );

	fs_debug = Cvar_Get( "fs_debug", "0", 0 );
	fs_cdpath      = Cvar_Get( "fs_cdpath", Sys_DefaultCDPath(), CVAR_INIT );
	fs_copyfiles   = Cvar_Get( "fs_copyfiles", "0", CVAR_INIT );
	fs_basepath = Cvar_Get("fs_basepath", Sys_DefaultInstallPath(), CVAR_INIT | CVAR_PROTECTED);
	fs_homeconfigpath = Cvar_Get ("fs_homeconfigpath", configPath, CVAR_INIT|CVAR_PROTECTED );
	fs_homedatapath = Cvar_Get ("fs_homedatapath", dataPath, CVAR_INIT|CVAR_PROTECTED );
	fs_homestatepath = Cvar_Get ("fs_homestatepath", statePath, CVAR_INIT|CVAR_PROTECTED );
	fs_gamedirvar = Cvar_Get ("fs_game", "", CVAR_INIT|CVAR_SYSTEMINFO );
	fs_restrict = Cvar_Get( "fs_restrict", "", CVAR_INIT );
	fs_steampath = Cvar_Get ("fs_steampath", Sys_SteamPath(), CVAR_INIT|CVAR_PROTECTED );
	fs_gogpath = Cvar_Get ("fs_gogpath", Sys_GogPath(), CVAR_INIT|CVAR_PROTECTED );
	fs_microsoftstorepath = Cvar_Get ("fs_microsoftstorepath", Sys_MicrosoftStorePath(), CVAR_INIT|CVAR_PROTECTED );


	FS_InitPathVars( );

	if (!FS_FilenameCompare(fs_gamedirvar->string, gameName)) {
		Cvar_ForceReset( "fs_game" );
	}

	if (FS_InvalidGameDir(fs_gamedirvar->string)) {
		Com_Error( ERR_DROP, "Invalid fs_game '%s'", fs_gamedirvar->string );
	}

	FS_CreatePath(fs_homeconfigpath->string);
	FS_CreatePath(fs_homedatapath->string);
	FS_CreatePath(fs_homestatepath->string);
	FS_AddGameDirectories(gameName);

	if ( fs_gamedirvar->string[0] && Q_stricmp( fs_gamedirvar->string, gameName ) ) {
		FS_AddGameDirectories(fs_gamedirvar->string);
	}

#ifndef STANDALONE
	Com_ReadCDKey(BASEGAME);
	if (fs_gamedirvar->string[0]) {
		Com_AppendCDKey(fs_gamedirvar->string);
	}
#endif

	fs_mapdir = Cvar_Get( "mapdir", "", 0 );
	fs_filedir = Cvar_Get( "fs_filedir", FS_BuildOSPath( fs_basepath->string, fs_gamedir, "/maps/" ), 0 );

	Cmd_AddCommand ("path", FS_Path_f);
	Cmd_AddCommand ("touchFile", FS_TouchFile_f );

	/* The port splits the old single home into config/data/state, all created
	   above; the legacy single var stays empty here, and mkdir("") is not a
	   directory request. */
	if ( fs_homepath->string[0] ) {
		Sys_Mkdir(fs_homepath->string);
	}

	FS_ReorderPurePaks();

	{
		searchpath_t	*uivk_sp;
		int				uivk_i = 0;

		for ( uivk_sp = fs_searchpaths; uivk_sp; uivk_sp = uivk_sp->next ) {
			if ( uivk_sp->pack ) {
				UIVK_PROBE( "pakmount", "slot=%d pak=%s files=%d", uivk_i,
							uivk_sp->pack->pakBasename, uivk_sp->pack->numfiles );
			} else if ( uivk_sp->dir ) {
				UIVK_PROBE( "pakmount", "slot=%d dir=%s/%s", uivk_i,
							uivk_sp->dir->path, uivk_sp->dir->gamedir );
			}
			uivk_i++;
		}
	}

	FS_Path_f();

	fs_gamedirvar->modified = qfalse; 

	Com_Printf( "----------------------\n" );

#ifdef FS_MISSING
	if (missingFiles == NULL) {
		missingFiles = Sys_FOpen( "\\missing.txt", "ab" );
	}
#endif
	Com_Printf( "%d files in pk3 files\n", fs_packFiles );

	SaveRegistryInfo( qtrue, "basepath", fs_basepath->string, 128 );
}


void FS_SetRestrictions( void ) {

}

const char *FS_LoadedPakChecksums( void ) {
	static char	info[BIG_INFO_STRING];
	searchpath_t	*search;

	info[0] = 0;

	for ( search = fs_searchpaths ; search ; search = search->next ) {
		if ( !search->pack ) {
			continue;
		}

		Q_strcat( info, sizeof( info ), va("%i ", search->pack->checksum ) );
	}

	return info;
}

const char *FS_LoadedPakNames( void ) {
	static char	info[BIG_INFO_STRING];
	searchpath_t	*search;

	info[0] = 0;

	for ( search = fs_searchpaths ; search ; search = search->next ) {
		if ( !search->pack ) {
			continue;
		}

		if (*info) {
			Q_strcat(info, sizeof( info ), " " );
		}
		Q_strcat( info, sizeof( info ), search->pack->pakBasename );
	}

	return info;
}

const char *FS_LoadedPakPureChecksums( void ) {
	static char	info[BIG_INFO_STRING];
	searchpath_t	*search;

	info[0] = 0;

	for ( search = fs_searchpaths ; search ; search = search->next ) {
		if ( !search->pack ) {
			continue;
		}

		Q_strcat( info, sizeof( info ), va("%i ", search->pack->pure_checksum ) );
	}

	return info;
}

const char *FS_ReferencedPakPureChecksums( void ) {
	static char	info[BIG_INFO_STRING];
	searchpath_t	*search;
	int numPaks, checksum, isServerPak, i;

	info[0] = 0;

	checksum = fs_checksumFeed;
	numPaks = 0;
	for ( search = fs_searchpaths ; search ; search = search->next ) {
		if ( search->pack ) {
			isServerPak = 0;
			for ( i = 0; i < fs_numServerPaks; i++ ) {
				if ( !stricmp( search->pack->pakBasename, fs_serverPakNames[i] ) ) {
					isServerPak = 1;
					break;
				}
			}
			if ( search->pack->referenced || isServerPak ) {
				if ( strnicmp( search->pack->pakBasename, "locpak", 6 ) ||
					 strnicmp( search->pack->pakGamename, "base", strlen( "base" ) ) ) {
					Q_strcat( info, sizeof( info ), va("%i ", search->pack->pure_checksum ) );
					numPaks++;
					checksum ^= search->pack->pure_checksum;
				}
			}
		}
	}
	if ( fs_pureChecksumExtra ) {
		Q_strcat( info, sizeof( info ), va("%i ", fs_pureChecksumExtra ) );
	}
	checksum ^= numPaks;
	Q_strcat( info, sizeof( info ), va("%i ", checksum ) );

	return info;
}



void FS_PureServerSetLoadedPaks( const char *pakSums, const char *pakNames ) {
	int		i, c, d;

	Cmd_TokenizeString( pakSums );

	c = Cmd_Argc();
	if ( c > MAX_SEARCH_PATHS ) {
		c = MAX_SEARCH_PATHS;
	}

	fs_numServerPaks = c;

	for ( i = 0 ; i < c ; i++ ) {
		fs_serverPaks[i] = atoi( Cmd_Argv( i ) );
	}

	if (fs_numServerPaks) {
		Com_DPrintf( "Connected to a pure server.\n" );
	}
	else
	{
		if (fs_reordered)
		{
			Com_DPrintf( "FS search reorder is required\n" );
			FS_Restart(fs_checksumFeed);
			return;
		}
	}

	for ( i = 0 ; i < c ; i++ ) {
		if (fs_serverPakNames[i]) {
			Z_Free(fs_serverPakNames[i]);
		}
		fs_serverPakNames[i] = NULL;
	}
	if ( pakNames && *pakNames ) {
		Cmd_TokenizeString( pakNames );

		d = Cmd_Argc();
		if ( d > MAX_SEARCH_PATHS ) {
			d = MAX_SEARCH_PATHS;
		}

		for ( i = 0 ; i < d ; i++ ) {
			fs_serverPakNames[i] = CopyString( Cmd_Argv( i ) );
		}
	}
}

void FS_PureServerSetReferencedPaks( const char *pakSums, const char *pakNames ) {
	int		i, c, d = 0;

	Cmd_TokenizeString( pakSums );

	c = Cmd_Argc();
	if ( c > MAX_SEARCH_PATHS ) {
		c = MAX_SEARCH_PATHS;
	}

	for ( i = 0 ; i < c ; i++ ) {
		fs_serverReferencedPaks[i] = atoi( Cmd_Argv( i ) );
	}

	for (i = 0 ; (size_t)i < sizeof(fs_serverReferencedPakNames) / sizeof(*fs_serverReferencedPakNames); i++)
	{
		if(fs_serverReferencedPakNames[i])
			Z_Free(fs_serverReferencedPakNames[i]);

		fs_serverReferencedPakNames[i] = NULL;
	}

	if ( pakNames && *pakNames ) {
		Cmd_TokenizeString( pakNames );

		d = Cmd_Argc();

		if(d > c)
			d = c;

		for ( i = 0 ; i < d ; i++ ) {
			fs_serverReferencedPakNames[i] = CopyString( Cmd_Argv( i ) );
		}
	}

	if(d < c)
		c = d;

	fs_numServerReferencedPaks = c;
}

void FS_InitFilesystem( void ) {
	Com_StartupVariable("fs_basepath");
	Com_StartupVariable("fs_homepath");
	Com_StartupVariable("fs_homeconfigpath");
	Com_StartupVariable("fs_homedatapath");
	Com_StartupVariable("fs_homestatepath");
	Com_StartupVariable("fs_game");
    Com_StartupVariable("fs_restrict");

	if(!FS_FilenameCompare(Cvar_VariableString("fs_game"), BASEGAME))
		Cvar_Set("fs_game", "");

	FS_Startup(BASEGAME);
	FS_SetRestrictions();

	/* No `global/default.cfg' check.  The launch no longer execs a global cfg:
	   the defaults are folded in once from ef2_android_defaults.h, and the one
	   config it loads is config.cfg, which Com_Init seeds when it is absent.
	   Requiring a file nothing reads would abort a correct tree. */
}

void FS_Restart( int checksumFeed ) {


	SMP_LOAD_BEGIN( fs );
	FS_Shutdown(qfalse);

	fs_checksumFeed = checksumFeed;

	FS_Startup(BASEGAME);
	SMP_LOAD_END( fs );
}


qboolean FS_ConditionalRestart(int checksumFeed, qboolean disconnect)
{
	// parameter retail does not have, a Com_GameRestart() teardown (client + local
	(void)disconnect;

	if (!fs_gamedirvar->modified) {
		if (checksumFeed == fs_checksumFeed) {
			return qfalse;
		}
	}

	FS_Restart(checksumFeed);
	return qtrue;
}


int		FS_FOpenFileByMode( const char *qpath, fileHandle_t *f, fsMode_t mode ) {
	int		r;
	qboolean	sync;

	sync = qfalse;

	switch( mode ) {
		case FS_READ:
			r = FS_FOpenFileRead( qpath, f, qtrue, qfalse );
			break;
		case FS_WRITE:
			*f = FS_FOpenFileWrite_HomeData( qpath );
			r = 0;
			if (*f == 0) {
				r = -1;
			}
			break;
		case FS_APPEND_SYNC:
			sync = qtrue;
		case FS_APPEND:
			*f = FS_FOpenFileAppend_HomeData( qpath );
			r = 0;
			if (*f == 0) {
				r = -1;
			}
			break;
		default:
			Com_Error( ERR_FATAL, "FS_FOpenFileByMode: bad mode" );
			return -1;
	}

	if (!f) {
		return r;
	}

	if ( *f ) {
		fsh[*f].fileSize = r;
	}
	fsh[*f].handleSync = sync;

	return r;
}

int		FS_FTell( fileHandle_t f ) {
	int pos;
	if (fsh[f].zipFile == qtrue) {
		if (!unzIsCurrentFileOpen(fsh[f].handleFiles.file.z)) {
			Com_Error(ERR_FATAL, "Tried to read from a zip file %s that was not open\n", fsh[f].name);
		}
		pos = unztell(fsh[f].handleFiles.file.z);
	} else {
		pos = ftell(fsh[f].handleFiles.file.o);
	}
	return pos;
}

void	FS_Flush( fileHandle_t f ) {
	fflush(fsh[f].handleFiles.file.o);
}

void	FS_FilenameCompletion( const char *dir, const char *ext,
		qboolean stripExt, void(*callback)(const char *s), qboolean allowNonPureFilesOnDisk ) {
	char	**filenames;
	int		nfiles;
	int		i;
	char	filename[ MAX_STRING_CHARS ];

	filenames = FS_ListFilteredFiles( dir, ext, NULL, qfalse, &nfiles, allowNonPureFilesOnDisk );

	FS_SortFileList( filenames, nfiles );

	for( i = 0; i < nfiles; i++ ) {
		FS_ConvertPath( filenames[ i ] );
		Q_strncpyz( filename, filenames[ i ], MAX_STRING_CHARS );

		if( stripExt ) {
			COM_StripExtension(filename, filename, sizeof(filename));
		}

		callback( filename );
	}
	FS_FreeFileList( filenames );
}

const char *FS_GetCurrentGameDir(void)
{
	if(fs_gamedir[0])
		return fs_gamedir;

	return BASEGAME;
}

#define ABSOLUTE_NAME_START 3

void FS_GetRelativeFilename( const char *currentDirectory, const char *absoluteFilename, char *out, size_t destlen )
{
	int afMarker = 0, rfMarker = 0;
	size_t cdLen = 0, afLen = 0;
	int i = 0;
	int levels = 0;
	cdLen = strlen( currentDirectory );
	afLen = strlen( absoluteFilename );

	if( cdLen > destlen || cdLen < ABSOLUTE_NAME_START + 1 ||
		afLen > destlen || afLen < ABSOLUTE_NAME_START + 1 )
	{
		return;
	}

	if( currentDirectory[ 0 ] != absoluteFilename[ 0 ] )
	{
		strncpy( out, absoluteFilename, destlen );
		return;
	}

	i = ABSOLUTE_NAME_START;
	while( (size_t)i < afLen && (size_t)i < cdLen )
	{
		if( currentDirectory[ i ] == absoluteFilename[ i ]
			|| ( currentDirectory[ i ] == '\\' && absoluteFilename[ i ] == '/' )
			|| ( currentDirectory[ i ] == '/' && absoluteFilename[ i ] == '\\' ) )
		{
			i++;
		}
		else
		{
			break;
		}
	}
	if( (size_t)i == cdLen )
	{
		if( absoluteFilename[ i ] == '\\' || absoluteFilename[ i - 1 ] == '\\'
			|| absoluteFilename[ i ] == '/' || absoluteFilename[ i - 1 ] == '/' )
		{
			if( absoluteFilename[ i ] == '\\' || absoluteFilename[ i ] == '/' )
			{
				i++;
			}

			strncpy( out, &absoluteFilename[ i ], destlen );
			return;
		}
	}
	afMarker = i;
	levels = 1;
	while( (size_t)i < cdLen )
	{
		i++;
		if( currentDirectory[ i ] == '\\' || currentDirectory[ i ] == '/' )
		{
			i++;
			if( currentDirectory[ i ] != '\0' )
			{
				levels++;
			}
		}
	}
	while( afMarker > 0 && absoluteFilename[ afMarker - 1 ] != '\\' && absoluteFilename[ afMarker - 1 ] != '/' )
	{
		afMarker--;
	}
	if( levels * 3 + afLen - afMarker > destlen )
	{
		return;
	}

	rfMarker = 0;
	for( i = 0; i < levels; i++ )
	{
		out[ rfMarker++ ] = '.';
		out[ rfMarker++ ] = '.';
		out[ rfMarker++ ] = PATH_SEP;
	}

	strcpy( &out[ rfMarker ], &absoluteFilename[ afMarker ] );
}

qboolean FS_FileNewer(const char* source, const char* destination)
{
    return qfalse;
}


void FS_CanonicalFilename(char* filename)
{
	char* source;
	char* dest;

	dest = filename;

    for(source = filename; *source; source++)
    {
		switch (*source) {
		case '\t':
		case ' ':
			break;
		case '\n':
		case '\r':
			*filename = 0;
			break;
		case '/':
		case '\\':
			if (dest == filename) {
				*filename = 0;
				return;
			}

			if (*(dest - 1) != '/') {
				*dest = '/';
				dest++;
			}
			break;
		default:
			*dest = tolower(*source);
			dest++;
			break;
		}
    }

	*dest = 0;
}

// stat()/localtime() body this used to carry showed data retail does not have,
void FS_FileTime(const char *filename, char *date, char *size)
{
    // (cl_uifilepicker.cpp:191,208) showed data retail does not have.
    (void)filename;
    strcpy(date, "N/A");
    strcpy(size, "N/A");
}
