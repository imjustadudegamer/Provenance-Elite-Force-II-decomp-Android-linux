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
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include <string.h>
#include <math.h>
#include "cmdlib.h"
#include "mathlib.h"
#include "imagelib.h"
#include "scriplib.h"

#include "../../../qcommon/qfiles.h"
#include "../../../qcommon/surfaceflags.h"

#include "shaders.h"
#ifdef _WIN32

#ifdef _TTIMOBUILD
#include "pakstuff.h"
#include "jpeglib.h"
#else
#include "libs/pakstuff.h"
#include "../libs/jpeglib.h"
#endif

#endif


#define	DEFAULT_BACKSPLASH_FRACTION		0.05
#define	DEFAULT_BACKSPLASH_DISTANCE		24


#define	MAX_SURFACE_INFO	4096

shaderInfo_t	defaultInfo;
shaderInfo_t	shaderInfo[MAX_SURFACE_INFO];
int				numShaderInfo;


typedef struct {
	char	*name;
	int		clearSolid, surfaceFlags, contents;
} infoParm_t;

infoParm_t	infoParms[] = {
	{"water",		1,	0,	CONTENTS_WATER },
	{"slime",		1,	0,	CONTENTS_SLIME },		
	{"lava",		1,	0,	CONTENTS_LAVA },		
	{"playerclip",	1,	0,	CONTENTS_PLAYERCLIP },
	{"monsterclip",	1,	0,	CONTENTS_MONSTERCLIP },
	{"nodrop",		1,	0,	CONTENTS_NODROP },		
	{"nonsolid",	1,	SURF_NONSOLID,	0},						

	{"origin",		1,	0,	CONTENTS_ORIGIN },		
	{"trans",		0,	0,	CONTENTS_TRANSLUCENT },	
	{"detail",		0,	0,	CONTENTS_DETAIL },		
	{"structural",	0,	0,	CONTENTS_STRUCTURAL },	
	{"areaportal",	1,	0,	CONTENTS_AREAPORTAL },	
#ifdef CONTENTS_CLUSTERPORTAL
	{"clusterportal",1, 0,  CONTENTS_CLUSTERPORTAL },
#endif
	{"donotenter",  1,  0,  CONTENTS_DONOTENTER },	
	{"botclip",     1,  0,  CONTENTS_BOTCLIP },		
	{"nobotclip",	0,	0,	CONTENTS_NOBOTCLIP },	

	{"fog",			1,	0,	CONTENTS_FOG},			
	{"sky",			0,	SURF_SKY,		0 },		
#ifdef SURF_LIGHTFILTER
	{"lightfilter",	0,	SURF_LIGHTFILTER, 0 },		
#endif 
	{"alphashadow",	0,	SURF_ALPHASHADOW, 0 },		
	{"hint",		0,	SURF_HINT,		0 },		

	{"slick",		0,	SURF_SLICK,		0 },
	{"noimpact",	0,	SURF_NOIMPACT,	0 },		
	{"nomarks",		0,	SURF_NOMARKS,	0 },		
	{"ladder",		0,	SURF_LADDER,	0 },
	{"nodamage",	0,	SURF_NODAMAGE,	0 },
#ifdef SURF_METALSTEPS
	{"metalsteps",	0,	SURF_METALSTEPS,0 },
#endif
#ifdef SURF_FLESH
	{"flesh",		0,	SURF_FLESH,		0 },
#endif
	{"nosteps",		0,	SURF_NOSTEPS,	0 },

	{"nodraw",		0,	SURF_NODRAW,	0 },		
#ifdef SURF_POINTLIGHT
	{"pointlight",	0,	SURF_POINTLIGHT, 0 },		
#endif
	{"nolightmap",	0,	SURF_NOLIGHTMAP,0 },		
	{"nodlight",	0,	SURF_NODLIGHT, 0 },			
#ifdef SURF_DUST
	{"dust",		0,	SURF_DUST, 0}				
#endif
};



byte* LoadImageFile(char *filename, qboolean *bTGA)
{
  byte *buffer = NULL;
  int nLen = 0;
  *bTGA = qtrue;
  if (FileExists(filename))
  {
    LoadFileBlock(filename, &buffer);
  }
#ifdef _WIN32
  else
  {
    PakLoadAnyFile(filename, &buffer);
  }
#endif
  if ( buffer == NULL)
  {
    nLen = strlen(filename);
    filename[nLen-3] = 'j';
    filename[nLen-2] = 'p';
    filename[nLen-1] = 'g';
    if (FileExists(filename))
    {
      LoadFileBlock(filename, &buffer);
    }
#ifdef _WIN32
    else
    {
      PakLoadAnyFile(filename, &buffer);
    }
#endif
    if ( buffer )
    {
      *bTGA = qfalse;
    }
  }
  return buffer;
}

static void LoadShaderImage( shaderInfo_t *si ) {
	char			filename[1024];
	int				i, count;
	float			color[4];
  byte      *buffer;
  qboolean  bTGA = qtrue;

	if ( si->lightimage[0] ) {
		sprintf( filename, "%s%s", gamedir, si->lightimage );
		DefaultExtension( filename, ".tga" );
    buffer = LoadImageFile(filename, &bTGA);
    if ( buffer != NULL) {
      goto loadTga;
    }
  }

	if ( si->editorimage[0] ) {
		sprintf( filename, "%s%s", gamedir, si->editorimage );
		DefaultExtension( filename, ".tga" );
    buffer = LoadImageFile(filename, &bTGA);
    if ( buffer != NULL) {
      goto loadTga;
    }
  }

  sprintf( filename, "%s%s.tga", gamedir, si->shader );
  buffer = LoadImageFile(filename, &bTGA);
  if ( buffer != NULL) {
		goto loadTga;
	}

  sprintf( filename, "%s%s.TGA", gamedir, si->shader );
  buffer = LoadImageFile(filename, &bTGA);
  if ( buffer != NULL) {
		goto loadTga;
	}

	_printf("WARNING: Couldn't find image for shader %s\n", si->shader );

	si->color[0] = 1;
	si->color[1] = 1;
	si->color[2] = 1;
	si->width = 64;
	si->height = 64;
	si->pixels = malloc( si->width * si->height * 4 );
	memset ( si->pixels, 255, si->width * si->height * 4 );
	return;

loadTga:
  if ( bTGA) {
	  LoadTGABuffer( buffer, &si->pixels, &si->width, &si->height );
  }
  else {
#ifdef _WIN32
    LoadJPGBuff(buffer, &si->pixels, &si->width, &si->height );
#endif
  }

  free(buffer);

	count = si->width * si->height;

	VectorClear( color );
	color[ 3 ] = 0;
	for ( i = 0 ; i < count ; i++ ) {
		color[0] += si->pixels[ i * 4 + 0 ];
		color[1] += si->pixels[ i * 4 + 1 ];
		color[2] += si->pixels[ i * 4 + 2 ];
		color[3] += si->pixels[ i * 4 + 3 ];
	}
	ColorNormalize( color, si->color );
	VectorScale( color, 1.0/count, si->averageColor );
}

static shaderInfo_t	*AllocShaderInfo( void ) {
	shaderInfo_t	*si;

	if ( numShaderInfo == MAX_SURFACE_INFO ) {
		Error( "MAX_SURFACE_INFO" );
	}
	si = &shaderInfo[ numShaderInfo ];
	numShaderInfo++;


	si->contents = CONTENTS_SOLID;

	si->backsplashFraction = DEFAULT_BACKSPLASH_FRACTION;
	si->backsplashDistance = DEFAULT_BACKSPLASH_DISTANCE;

	si->lightmapSampleSize = 0;
	si->forceTraceLight = qfalse;
	si->forceVLight = qfalse;
	si->patchShadows = qfalse;
	si->vertexShadows = qfalse;
	si->noVertexShadows = qfalse;
	si->forceSunLight = qfalse;
	si->vertexScale = 1.0;
	si->notjunc = qfalse;

	return si;
}

shaderInfo_t	*ShaderInfoForShader( const char *shaderName ) {
	int				i;
	shaderInfo_t	*si;
	char			shader[MAX_QPATH];

	strcpy( shader, shaderName );
	StripExtension( shader );

	for ( i = 0 ; i < numShaderInfo ; i++ ) {
		si = &shaderInfo[ i ];
		if ( !Q_stricmp( shader, si->shader ) ) {
			if ( !si->width ) {
				LoadShaderImage( si );
			}
			return si;
		}
	}

	si = AllocShaderInfo();
	strcpy( si->shader, shader );

	LoadShaderImage( si );

	return si;
}

static void ParseShaderFile( const char *filename ) {
	int		i;
	int		numInfoParms = sizeof(infoParms) / sizeof(infoParms[0]);
	shaderInfo_t	*si;

	LoadScriptFile( filename );
	while ( 1 ) {
		if ( !GetToken( qtrue ) ) {
			break;
		}

		si = AllocShaderInfo();
		strcpy( si->shader, token );
		MatchToken( "{" );
		while ( 1 ) {
			if ( !GetToken( qtrue ) ) {
				break;
			}
			if ( !strcmp( token, "}" ) ) {
				break;
			}

			if ( !strcmp( token, "{" ) ) {
				si->hasPasses = qtrue;
				while ( 1 ) {
					if ( !GetToken( qtrue ) ) {
						break;
					}
					if ( !strcmp( token, "}" ) ) {
						break;
					}
				}
				continue;
			}

			if ( !Q_stricmp( token, "surfaceparm" ) ) {
				GetToken( qfalse );
				for ( i = 0 ; i < numInfoParms ; i++ ) {
					if ( !Q_stricmp( token, infoParms[i].name ) ) {
						si->surfaceFlags |= infoParms[i].surfaceFlags;
						si->contents |= infoParms[i].contents;
						if ( infoParms[i].clearSolid ) {
							si->contents &= ~CONTENTS_SOLID;
						}
						break;
					}
				}
				if ( i == numInfoParms ) {
					if ( Q_strncasecmp( token, "qer", 3 ) ) {
						_printf( "Unknown surfaceparm: \"%s\"\n", token );
					}
				}
				continue;
			}


			if ( !Q_stricmp( token, "qer_editorimage" ) ) {
				GetToken( qfalse );
				strcpy( si->editorimage, token );
				DefaultExtension( si->editorimage, ".tga" );
				continue;
			}

			if ( !Q_stricmp( token, "q3map_lightimage" ) ) {
				GetToken( qfalse );
				strcpy( si->lightimage, token );
				DefaultExtension( si->lightimage, ".tga" );
				continue;
			}

			if ( !Q_stricmp( token, "q3map_surfacelight" )  ) {
				GetToken( qfalse );
				si->value = atoi( token );
				continue;
			}

			if ( !Q_stricmp( token, "q3map_lightsubdivide" )  ) {
				GetToken( qfalse );
				si->lightSubdivide = atoi( token );
				continue;
			}

			if ( !Q_stricmp( token, "q3map_lightmapsamplesize" ) ) {
				GetToken( qfalse );
				si->lightmapSampleSize = atoi( token );
				continue;
			}

			if ( !Q_stricmp( token, "q3map_tracelight" ) ) {
				si->forceTraceLight = qtrue;
				continue;
			}

			if ( !Q_stricmp( token, "q3map_vlight" ) ) {
				si->forceVLight = qtrue;
				continue;
			}

			if ( !Q_stricmp( token, "q3map_patchshadows" ) ) {
				si->patchShadows = qtrue;
				continue;
			}

			if ( !Q_stricmp( token, "q3map_vertexshadows" ) ) {
				si->vertexShadows = qtrue;
				continue;
			}

			if ( !Q_stricmp( token, "q3map_novertexshadows" ) ) {
				si->noVertexShadows = qtrue;
				continue;
			}

			if ( !Q_stricmp( token, "q3map_forcesunlight" ) ) {
				si->forceSunLight = qtrue;
				continue;
			}

			if ( !Q_stricmp( token, "q3map_vertexscale" ) ) {
				GetToken( qfalse );
				si->vertexScale = atof(token);
				continue;
			}

			if ( !Q_stricmp( token, "q3map_notjunc" ) ) {
				si->notjunc = qtrue;
				continue;
			}

			if ( !Q_stricmp( token, "q3map_globaltexture" )  ) {
				si->globalTexture = qtrue;
				continue;
			}

			if ( !Q_stricmp( token, "q3map_backsplash" ) ) {
				GetToken( qfalse );
				si->backsplashFraction = atof( token ) * 0.01;
				GetToken( qfalse );
				si->backsplashDistance = atof( token );
				continue;
			}

			if ( !Q_stricmp( token, "q3map_backshader" ) ) {
				GetToken( qfalse );
				strcpy( si->backShader, token );
				continue;
			}

			if ( !Q_stricmp( token, "q3map_flare" ) ) {
				GetToken( qfalse );
				strcpy( si->flareShader, token );
				continue;
			}

			if ( !Q_stricmp( token, "light" ) ) {
				GetToken( qfalse );
				strcpy( si->flareShader, "flareshader" );
				continue;
			}

			if ( !Q_stricmp( token, "q3map_sun" ) ) {
				float	a, b;

				GetToken( qfalse );
				si->sunLight[0] = atof( token );
				GetToken( qfalse );
				si->sunLight[1] = atof( token );
				GetToken( qfalse );
				si->sunLight[2] = atof( token );
				
				VectorNormalize( si->sunLight, si->sunLight);

				GetToken( qfalse );
				a = atof( token );
				VectorScale( si->sunLight, a, si->sunLight);

				GetToken( qfalse );
				a = atof( token );
				a = a / 180 * Q_PI;

				GetToken( qfalse );
				b = atof( token );
				b = b / 180 * Q_PI;

				si->sunDirection[0] = cos( a ) * cos( b );
				si->sunDirection[1] = sin( a ) * cos( b );
				si->sunDirection[2] = sin( b );

				si->surfaceFlags |= SURF_SKY;
				continue;
			}

			if ( !Q_stricmp( token, "tesssize" ) ) {
				GetToken( qfalse );
				si->subdivisions = atof( token );
				continue;
			}

			if ( !Q_stricmp( token, "cull" ) ) {
				GetToken( qfalse );
				if ( !Q_stricmp( token, "none" ) ) {
					si->twoSided = qtrue;
				}
				continue;
			}


			if ( !Q_stricmp( token, "deformVertexes" ) ) {
				GetToken( qfalse );
				if ( !Q_strncasecmp( token, "autosprite", 10 ) ) {
					si->autosprite = qtrue;
          si->contents = CONTENTS_DETAIL;
				}
				continue;
			}



			while ( TokenAvailable() ) {
				GetToken( qfalse );
			}
		}			
	}
}

#define	MAX_SHADER_FILES	512
#include <io.h>
void LoadShaderInfo( void ) {
	char			filename[1024];
	int				i;
	char			*shaderFiles[MAX_SHADER_FILES];
	int				numShaderFiles;
	FILE *f;

	sprintf( filename, "%sscripts/shaderlist.txt", gamedir );

	f = fopen(filename,"rb");
	if(f == 0) {
		struct _finddata_t fileinfo;
		char scriptsDir[1024];
		int handle;
		sprintf( scriptsDir, "%sscripts/*.shader", gamedir );	
		handle = _findfirst (scriptsDir, &fileinfo);
		if (handle != -1)
		{
			do
			{
				sprintf(filename, "%s\\%s", scriptsDir, fileinfo.name);
				shaderFiles[numShaderFiles] = malloc(MAX_OS_PATH);
				strcpy( shaderFiles[ numShaderFiles ], token );
				numShaderFiles++;
			} while (_findnext( handle, &fileinfo ) != -1);
			_findclose (handle);
		}
	} else {
		fclose(f);

		LoadScriptFile( filename );

		numShaderFiles = 0;
		while ( 1 ) {
			if ( !GetToken( qtrue ) ) {
				break;
			}
			shaderFiles[numShaderFiles] = malloc(MAX_OS_PATH);
			strcpy( shaderFiles[ numShaderFiles ], token );
			numShaderFiles++;
		}
	}

	for ( i = 0 ; i < numShaderFiles ; i++ ) {
		sprintf( filename, "%sscripts/%s.shader", gamedir, shaderFiles[i] );
		ParseShaderFile( filename );
    free(shaderFiles[i]);
	}

	qprintf( "%5i shaderInfo\n", numShaderInfo);
}

