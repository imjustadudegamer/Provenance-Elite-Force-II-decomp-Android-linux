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
// SPDX-License-Identifier: GPL-2.0-or-later
#include "tr_local.h"
#include "tr_hwtex.h"
#include "tr_texcache.h"



static void LoadTGA( const char *name, byte **pic, int *width, int *height );

static byte			 s_intensitytable[256];
static unsigned char s_gammatable[256];

int		gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int		gl_filter_max = GL_LINEAR;

#define FILE_HASH_SIZE		1024
static	image_t*		hashTable[FILE_HASH_SIZE];

extern qboolean scr_initialized;

void R_GammaCorrect( byte *buffer, int bufSize ) {
	int i;

	for ( i = 0; i < bufSize; i++ ) {
		buffer[i] = s_gammatable[buffer[i]];
	}
}

typedef struct {
	char *name;
	int	minimize, maximize;
} textureMode_t;

textureMode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

static long generateHashValue( const char *fname ) {
	int		i;
	long	hash;
	char	letter;

	hash = 0;
	i = 0;
	while (fname[i] != '\0') {
		letter = tolower(fname[i]);
		if (letter =='.') break;				
		if (letter =='\\') letter = '/';		
		hash+=(long)(letter)*(i+119);
		i++;
	}
	hash &= (FILE_HASH_SIZE-1);
	return hash;
}

void GL_TextureMode( const char *string ) {
	int		i;
	image_t	*glt;

	for ( i=0 ; i< 6 ; i++ ) {
		if ( !Q_stricmp( modes[i].name, string ) ) {
			break;
		}
	}

	if ( i == 5 && glConfig.hardwareType == GLHW_3DFX_2D3D ) {
		ri.Printf( PRINT_ALL, "Refusing to set trilinear on a voodoo.\n" );
		i = 3;
	}


	if ( i == 6 ) {
		ri.Printf (PRINT_ALL, "bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	for ( i = 0 ; i < tr.numImages ; i++ ) {
		glt = &tr.images[ i ];
		if ( glt->numMipmaps && glt->texnum ) {
			GL_Bind (glt);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		}
	}
}

int R_SumOfUsedImages( void ) {
	int	total;
	int i;

	total = 0;
	for ( i = 0; i < tr.numImages; i++ ) {
		if ( tr.images[i].frameUsed == tr.frameCount ) {
			total += tr.images[i].uploadWidth * tr.images[i].uploadHeight;
		}
	}

	return total;
}

void R_ImageList_f( void ) {
	int		i;
	image_t	*image;
	int		texels;
	const char *yesno[] = {
		"no ", "yes"
	};

	ri.Printf (PRINT_ALL, "\n      -w-- -h-- -mm- -TMU- -if-- wrap --name-------\n");
	texels = 0;

	for ( i = 0 ; i < tr.numImages ; i++ ) {
		image = &tr.images[ i ];

		texels += image->uploadWidth*image->uploadHeight;
		ri.Printf (PRINT_ALL,  "%4i: %4i %4i  %s   %d   ",
			i, image->uploadWidth, image->uploadHeight, yesno[image->numMipmaps], image->TMU );
		switch ( image->internalFormat ) {
		case 1:
			ri.Printf( PRINT_ALL, "I    " );
			break;
		case 2:
			ri.Printf( PRINT_ALL, "IA   " );
			break;
		case 3:
			ri.Printf( PRINT_ALL, "RGB  " );
			break;
		case 4:
			ri.Printf( PRINT_ALL, "RGBA " );
			break;
		case GL_RGBA8:
			ri.Printf( PRINT_ALL, "RGBA8" );
			break;
		case GL_RGB8:
			ri.Printf( PRINT_ALL, "RGB8" );
			break;
		case GL_RGB4_S3TC:
			ri.Printf( PRINT_ALL, "S3TC " );
			break;
		case GL_RGBA4:
			ri.Printf( PRINT_ALL, "RGBA4" );
			break;
		case GL_RGB5:
			ri.Printf( PRINT_ALL, "RGB5 " );
			break;
		default:
			ri.Printf( PRINT_ALL, "???? " );
		}

		switch ( image->wrapClampModeX ) {
		case GL_REPEAT:
			ri.Printf( PRINT_ALL, "rept " );
			break;
		case GL_CLAMP:
			ri.Printf( PRINT_ALL, "clmp " );
			break;
		default:
			ri.Printf( PRINT_ALL, "%4i ", image->wrapClampModeX );
			break;
		}
		
		ri.Printf( PRINT_ALL, " %s\n", image->imgName );
	}
	ri.Printf (PRINT_ALL, " ---------\n");
	ri.Printf (PRINT_ALL, " %i total texels (not including mipmaps)\n", texels);
	ri.Printf (PRINT_ALL, " %i total images\n\n", tr.numImages );
}

int R_SumOfTextureMemory( int type, int usedOnly, int estimated ) {
	int total;
	int i;

	total = 0;

	for ( i = 0; i < tr.numImages; i++ ) {
		image_t *image = &tr.images[i];
		int      bpp;
		int      size;
		qboolean keep;

		keep = qtrue;

		if ( usedOnly ) {
			keep = ( image->frameUsed == tr.frameCount );
		}

		if ( type ) {
			if ( !Q_stricmpn( image->imgName, "*lightmap", 9 ) ) {
				if ( type != 3 ) {
					keep = qfalse;
				}
			} else if ( !Q_stricmpn( image->imgName, "sysimg", 6 )
					|| !Q_stricmpn( image->imgName, "textures/menu", 13 )
					|| !Q_stricmpn( image->imgName, "textures\\menu", 13 ) ) {
				if ( type != 2 ) {
					keep = qfalse;
				}
			} else {
				if ( type != 1 ) {
					keep = qfalse;
				}
			}
		}

		if ( !keep ) {
			continue;
		}

		size = image->uploadWidth * image->uploadHeight;

		if ( image->internalFormat == 0x8056 ) {                    
			bpp = 2;
		} else if ( image->internalFormat >= 0x8056 ) {
			if ( image->internalFormat == 0x8059 ) {                
				bpp = 8;
			} else if ( image->internalFormat < 0x8059 ) {
				if ( image->internalFormat == 0x8057 ) {            
					bpp = 4;
				} else if ( image->internalFormat > 0x8057 ) {
					bpp = 6;
				} else if ( image->internalFormat >= 0x8058 ) {     
					bpp = 4;
				} else {
					bpp = 2;
				}
			} else if ( image->internalFormat == 0x83F2 ) {         
				bpp = 1;
			} else if ( image->internalFormat >= 0x83F2 ) {
				if ( image->internalFormat >= 0x83F4 ) {
					bpp = 4;
				} else {
					bpp = 1;
				}
			} else if ( image->internalFormat >= 0x83F1 ) {         
				bpp = 1;
			} else {
				bpp = 4;
			}
		} else {
			if ( image->internalFormat == 0x8050 ) {                
				bpp = 2;
			} else if ( image->internalFormat >= 0x8050 ) {
				if ( image->internalFormat == 0x8053 ) {            
					bpp = 5;
				} else if ( image->internalFormat > 0x8053 ) {
					if ( image->internalFormat >= 0x8055 ) {        
						bpp = 1;
					} else {
						bpp = 6;
					}
				} else if ( image->internalFormat >= 0x8052 ) {     
					bpp = 4;
				} else {
					bpp = 3;
				}
			} else if ( image->internalFormat == 4 ) {              
				bpp = 4;
			} else if ( image->internalFormat < 4 ) {
				if ( image->internalFormat < 3 ) {
					bpp = 4;
				} else {
					bpp = 3;
				}
			} else if ( image->internalFormat >= 0x804F ) {         
				bpp = 2;
			} else {
				bpp = 4;
			}
		}

		if ( image->force32bit ) {
			bpp = 4;
		}

		if ( ( estimated == 1 || estimated == 2 ) && bpp == 3 ) {
			bpp = 4;
		}

		if ( image->numMipmaps ) {
			total = (int)( ( 1.3333333f * (float)( bpp * size ) ) + (float)total );
		} else {
			total += bpp * size;
		}

		if ( estimated == 1 ) {
			total += 4096;
		}
	}

	return total;
}

void R_TotalTextureMemInfo_f( void ) {
	int ui, normal, lightmap, all;
	int uiEst, normalEst, lightmapEst, allEst;
	int dynamic;

	ui        = R_SumOfTextureMemory( 1, 0, 0 );
	normal    = R_SumOfTextureMemory( 2, 0, 0 );
	lightmap  = R_SumOfTextureMemory( 3, 0, 0 );
	all       = R_SumOfTextureMemory( 0, 0, 0 );
	uiEst     = R_SumOfTextureMemory( 1, 0, 1 );
	normalEst = R_SumOfTextureMemory( 2, 0, 1 );
	lightmapEst = R_SumOfTextureMemory( 3, 0, 1 );
	allEst    = R_SumOfTextureMemory( 0, 0, 1 );

	dynamic = 0;

	ri.Printf( PRINT_ALL, " ---------\n" );
	ri.Printf( PRINT_ALL, " Specified Memory Usage\n" );
	ri.Printf( PRINT_ALL, " ---------\n" );
	ri.Printf( PRINT_ALL, " %5.2fMB of UI textures\n", ui / 1048576.0f );
	ri.Printf( PRINT_ALL, " %5.2fMB of normal textures\n", normal / 1048576.0f );
	ri.Printf( PRINT_ALL, " %5.2fMB of lightmaps\n", lightmap / 1048576.0f );
	ri.Printf( PRINT_ALL, " %5.2fMB total texture bytes (that video card is handling)\n", all / 1048576.0f );
	ri.Printf( PRINT_ALL, " ---------\n" );
	ri.Printf( PRINT_ALL, " %5.2fMB of dynamic lightmap data\n", dynamic / 1048576.0f );
	ri.Printf( PRINT_ALL, "\n ---------\n" );
	ri.Printf( PRINT_ALL, " Estimated Actual Memory Usage\n" );
	ri.Printf( PRINT_ALL, " ---------\n" );
	ri.Printf( PRINT_ALL, " %5.2fMB of UI textures\n", uiEst / 1048576.0f );
	ri.Printf( PRINT_ALL, " %5.2fMB of normal textures\n", normalEst / 1048576.0f );
	ri.Printf( PRINT_ALL, " %5.2fMB of lightmaps\n", lightmapEst / 1048576.0f );
	ri.Printf( PRINT_ALL, " %5.2fMB total texture bytes (that video card is handling)\n", allEst / 1048576.0f );
	ri.Printf( PRINT_ALL, " ---------\n" );
	ri.Printf( PRINT_ALL, " %5.2fMB of dynamic lightmap data\n", dynamic / 1048576.0f );
}


typedef struct {
	const unsigned	*in;
	unsigned		*out;
	const unsigned	*p1, *p2;
	int				inwidth, inheight, outwidth, outheight;
	unsigned		fracstep;
} resampleRows_t;

static void ResampleTextureRows( void *vctx, int begin, int end ) {
	const resampleRows_t	*c = (const resampleRows_t *)vctx;
	int		i, j;
	const unsigned	*in = c->in;
	unsigned		*out = c->out + c->outwidth * begin;
	unsigned		*inrow, *inrow2;
	byte			*pix1, *pix2, *pix3, *pix4;

	for (i=begin ; i<end ; i++, out += c->outwidth) {
		inrow = (unsigned *)in + c->inwidth*(int)((i+0.25)*c->inheight/c->outheight);
		inrow2 = (unsigned *)in + c->inwidth*(int)((i+0.75)*c->inheight/c->outheight);
		for (j=0 ; j<c->outwidth ; j++) {
			pix1 = (byte *)inrow + c->p1[j];
			pix2 = (byte *)inrow + c->p2[j];
			pix3 = (byte *)inrow2 + c->p1[j];
			pix4 = (byte *)inrow2 + c->p2[j];
			((byte *)(out+j))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0])>>2;
			((byte *)(out+j))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1])>>2;
			((byte *)(out+j))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2])>>2;
			((byte *)(out+j))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3])>>2;
		}
	}
}

static void ResampleTexture( unsigned *in, int inwidth, int inheight, unsigned *out,
							int outwidth, int outheight ) {
	int		i;
	unsigned	frac, fracstep;
	unsigned	p1[2048], p2[2048];
	resampleRows_t	ctx;

	if (outwidth>2048)
		ri.Error(ERR_DROP, "ResampleTexture: max width");

	fracstep = inwidth*0x10000/outwidth;

	frac = fracstep>>2;
	for ( i=0 ; i<outwidth ; i++ ) {
		p1[i] = 4*(frac>>16);
		frac += fracstep;
	}
	frac = 3*(fracstep>>2);
	for ( i=0 ; i<outwidth ; i++ ) {
		p2[i] = 4*(frac>>16);
		frac += fracstep;
	}

	ctx.in = in;
	ctx.out = out;
	ctx.p1 = p1;
	ctx.p2 = p2;
	ctx.inwidth = inwidth;
	ctx.inheight = inheight;
	ctx.outwidth = outwidth;
	ctx.outheight = outheight;
	ctx.fracstep = fracstep;

	JobPool_ParallelRange( outheight, ( 65536 / ( outwidth ? outwidth : 1 ) ) + 1,
						   ResampleTextureRows, &ctx );
}

typedef struct {
	byte	*pixels;
	int		count;
	int		arm;			
} lightScale_t;

static void R_LightScaleTextureRange( void *vctx, int begin, int end ) {
	const lightScale_t	*c = (const lightScale_t *)vctx;
	int		i;
	byte	*p = c->pixels + 4 * begin;

	switch ( c->arm ) {
	case 0:
		for (i = begin ; i < end ; i++, p += 4)
		{
			p[0] = s_gammatable[p[0]];
			p[1] = s_gammatable[p[1]];
			p[2] = s_gammatable[p[2]];
		}
		break;
	case 1:
		for (i = begin ; i < end ; i++, p += 4)
		{
			p[0] = s_intensitytable[p[0]];
			p[1] = s_intensitytable[p[1]];
			p[2] = s_intensitytable[p[2]];
		}
		break;
	default:
		for (i = begin ; i < end ; i++, p += 4)
		{
			p[0] = s_gammatable[s_intensitytable[p[0]]];
			p[1] = s_gammatable[s_intensitytable[p[1]]];
			p[2] = s_gammatable[s_intensitytable[p[2]]];
		}
		break;
	}
}

void R_LightScaleTexture (unsigned *in, int inwidth, int inheight, qboolean only_gamma )
{
	lightScale_t	ctx;

	if (!tr.needsLightScale) {
		return;
	}

	ctx.pixels = (byte *)in;
	ctx.count  = inwidth*inheight;

	if ( only_gamma ) {
		ctx.arm = ( !glConfig.deviceSupportsGamma ) ? 0 : -1;
	} else {
		ctx.arm = ( glConfig.deviceSupportsGamma ) ? 1 : 2;
	}

	if ( ctx.arm < 0 ) {
		return;
	}

	JobPool_ParallelRange( ctx.count, 16384, R_LightScaleTextureRange, &ctx );
}

static void R_MipMap (byte *in, int width, int height) {
	SMP_MipMap( (unsigned char *)in, width, height );
}


typedef struct {
	byte	*data;
	int		inverseAlpha;
	int		premult[3];
} blendOver_t;

static void R_BlendOverTextureRange( void *vctx, int begin, int end ) {
	const blendOver_t	*c = (const blendOver_t *)vctx;
	int		i;
	byte	*data = c->data + 4 * begin;

	for ( i = begin ; i < end ; i++, data += 4 ) {
		data[0] = ( data[0] * c->inverseAlpha + c->premult[0] ) >> 9;
		data[1] = ( data[1] * c->inverseAlpha + c->premult[1] ) >> 9;
		data[2] = ( data[2] * c->inverseAlpha + c->premult[2] ) >> 9;
	}
}

static void R_BlendOverTexture( byte *data, int pixelCount, byte blend[4] ) {
	blendOver_t	c;

	c.inverseAlpha = 255 - blend[3];
	c.premult[0] = blend[0] * blend[3];
	c.premult[1] = blend[1] * blend[3];
	c.premult[2] = blend[2] * blend[3];
	c.data = data;

	JobPool_ParallelRange( pixelCount, 16384, R_BlendOverTextureRange, &c );
}

byte	mipBlendColors[16][4] = {
	{0,0,0,0},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
};


static GLenum R_ES3InternalFormat( int fmt ) {
	switch ( fmt ) {
	case 1:  return GL_RGBA8;           
	case 2:  return GL_RGBA8;           
	case 3:  return GL_RGBA;            
	case 4:  return GL_RGBA;
	case GL_RGB4:   return GL_RGB5_A1;  
	case GL_RGB5:   return GL_RGB5_A1;
	case GL_RGB8:   return GL_RGBA8;    
	case GL_RGB10:  return GL_RGBA8;    
	case GL_RGB12:  return GL_RGBA8;
	case 0x8054:    return GL_RGBA8;    
	case GL_RGBA2:  return GL_RGBA4;
	case 0x805A:    return GL_RGBA8;    
	case 0x805B:    return GL_RGBA8;    
	case GL_RGBA:   return GL_RGBA;
	case GL_RGBA4:  return GL_RGBA4;
	case GL_RGBA8:  return GL_RGBA8;
	case GL_RGB5_A1:return GL_RGB5_A1;
	default:        return GL_RGBA8;    
	}
}

/* TexStorage2D takes a sized internal format (ES 3.0 spec 3.8, p.137), so
 * R_ES3InternalFormat's unsized GL_RGBA cases keep the TexImage2D path. */
static GLenum R_ES3StorageFormat( GLenum fmt ) {
	switch ( fmt ) {
	case GL_RGBA8:   return GL_RGBA8;
	case GL_RGBA4:   return GL_RGBA4;
	case GL_RGB5_A1: return GL_RGB5_A1;
	default:         return 0;
	}
}


extern qboolean charSet;

static int	s_gles3UploadSamples = 0;
static int	s_gles3UploadFormat  = 0;

/* The software path re-specifies every mip level with TexImage2D.  One
 * TexStorage2D for the whole chain plus a TexSubImage2D per level is the same
 * chain built once, but the storage is immutable afterwards -- so the builtin
 * '*' images, which Draw_StretchRaw and RE_UploadCinematic re-specify, stay on
 * TexImage2D. */
static qboolean	s_gles3UploadStored = qfalse;
static int		s_gles3UploadLevels = 1;

static void R_UploadLevel( GLenum sizedFormat, int level, int w, int h, const void *data ) {
	if ( s_gles3UploadStored ) {
		qglTexSubImage2D( GL_TEXTURE_2D, level, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, data );
	} else {
		qglTexImage2D( GL_TEXTURE_2D, level, sizedFormat, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
	}
}

static int	s_gles3PathKind = 0;
static int	s_gles3PathFmt  = 0;
static int	s_gles3PathSrcDxt = SMP_DXT3;

static unsigned long	s_smpUploadSum[3] = { 0, 0, 0 };
static int				s_smpUploadWhite = 0;
static int				s_smpUploadPixels = 0;
static qboolean			s_smpWantStats = qfalse;

#define SMP_UPLOAD_LEVELS 8
static int				s_smpUploadLvlW[SMP_UPLOAD_LEVELS] = { 0 };
static int				s_smpUploadLvlH[SMP_UPLOAD_LEVELS] = { 0 };
static int				s_smpUploadLvlN = 0;

static void SMP_RecordUploadLevel( int level, int w, int h )
{
	if ( !s_smpWantStats || level < 0 || level >= SMP_UPLOAD_LEVELS ) {
		return;
	}
	s_smpUploadLvlW[level] = w;
	s_smpUploadLvlH[level] = h;
	if ( level + 1 > s_smpUploadLvlN ) {
		s_smpUploadLvlN = level + 1;
	}
}

static void Upload32(
	unsigned int* data,
	int width,
	int height,
	int numMipmaps,
	qboolean picmip, 
	qboolean dynamicallyUpdated,
	qboolean force32bit,
	qboolean hasAlpha,
	int* format,
	int* pUploadWidth,
	int* pUploadHeight,
	int* bytesUsed,
	qboolean bIsLightmap,
	qboolean allowStorage
)
{
	int			samples;
	unsigned	*scaledBuffer = NULL;
	unsigned	*resampledBuffer = NULL;
	int			scaled_width, scaled_height;
	int			i, c;
	int			levels;
	GLenum		sizedFormat;
	byte		*scan;
	GLenum		internalFormat = GL_RGB;

	for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1)
		;
	for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1)
		;
	if ( r_roundImagesDown->integer && scaled_width > width )
		scaled_width >>= 1;
	if ( r_roundImagesDown->integer && scaled_height > height )
		scaled_height >>= 1;

	if ( scaled_width != width || scaled_height != height ) {
		resampledBuffer = ri.Hunk_AllocateTempMemory( scaled_width * scaled_height * 4 );
		ResampleTexture (data, width, height, resampledBuffer, scaled_width, scaled_height);
		data = resampledBuffer;
		width = scaled_width;
		height = scaled_height;
	}

	if ( picmip ) {
		scaled_width >>= picmip;
		scaled_height >>= picmip;
	}

	if (scaled_width < 1) {
		scaled_width = 1;
	}
	if (scaled_height < 1) {
		scaled_height = 1;
	}

	while ( scaled_width > glConfig.maxTextureSize
		|| scaled_height > glConfig.maxTextureSize ) {
		scaled_width >>= 1;
		scaled_height >>= 1;
	}

	scaledBuffer = data;

	c = width*height;
	scan = ((byte *)data);
	samples = 3;
	if (!bIsLightmap) {
		/* Only the alpha byte decides the sample count; the per-channel maxima
		 * that the retail loop accumulated alongside it are never read. */
		for ( i = 0; i < c; i++ )
		{
			if ( scan[i*4 + 3] != 255 ) 
			{
				samples = 4;
				break;
			}
		}

		s_smpUploadSum[0] = s_smpUploadSum[1] = s_smpUploadSum[2] = 0;
		s_smpUploadWhite = 0;
		s_smpUploadPixels = 0;
		{
			int	lvl;
			for ( lvl = 0; lvl < SMP_UPLOAD_LEVELS; lvl++ ) {
				s_smpUploadLvlW[lvl] = s_smpUploadLvlH[lvl] = 0;
			}
			s_smpUploadLvlN = 0;
		}
		if ( s_smpWantStats ) {
			for ( i = 0; i < c; i++ ) {
				s_smpUploadSum[0] += scan[i*4+0];
				s_smpUploadSum[1] += scan[i*4+1];
				s_smpUploadSum[2] += scan[i*4+2];
				if ( scan[i*4+0] >= 200 && scan[i*4+1] >= 200 && scan[i*4+2] >= 200 ) {
					s_smpUploadWhite++;
				}
			}
			s_smpUploadPixels = c;
		}

		if ( samples == 3 )
		{
			if (!force32bit)
			{
				if (r_texturebits->integer == 16)
				{
					internalFormat = GL_RGB5;
				}
				else if (r_texturebits->integer == 32)
				{
					internalFormat = GL_RGB8;
				}
				else if (r_colorbits->integer == 16)
				{
					internalFormat = GL_RGBA4;
				}
				else if (r_colorbits->integer == 32)
				{
					internalFormat = GL_RGBA8;
				}
				else
				{
					internalFormat = 3;
				}
			}
			else
			{
				internalFormat = GL_RGB8;
			}
		}
		else if ( samples == 4 )
		{
			if (!force32bit)
			{
				if (r_texturebits->integer == 16)
				{
					internalFormat = GL_RGBA4;
				}
				else if (r_texturebits->integer == 32)
				{
					internalFormat = GL_RGBA8;
				}
				else if (r_colorbits->integer == 16)
				{
					internalFormat = GL_RGBA4;
				}
				else if (r_colorbits->integer == 32)
				{
					internalFormat = GL_RGBA8;
				}
				else
				{
					internalFormat = 4;
				}
			}
			else
			{
				internalFormat = GL_RGBA8;
			}
		}
	} else {
		internalFormat = GL_RGB;
	}
	s_gles3UploadSamples = samples;
	s_gles3UploadFormat  = internalFormat;

	/* The chain the mip loop below will build, counted before it mutates
	 * scaled_width/height. */
	levels = SMP_MipLevelCount( scaled_width, scaled_height );

	sizedFormat = R_ES3StorageFormat( R_ES3InternalFormat( internalFormat ) );
	s_gles3UploadStored = qfalse;
	s_gles3UploadLevels = numMipmaps ? levels : 1;
	if ( qglTexStorage2D && allowStorage && sizedFormat != 0 ) {
		int	drain;

		for ( drain = 0; drain < 16 && qglGetError() != GL_NO_ERROR; drain++ ) {
			;
		}
		qglTexStorage2D( GL_TEXTURE_2D, s_gles3UploadLevels, sizedFormat,
						 scaled_width, scaled_height );
		/* a driver that refuses the immutable storage keeps the TexImage2D path */
		if ( qglGetError() == GL_NO_ERROR ) {
			s_gles3UploadStored = qtrue;
		}
	}

	if ( ( scaled_width == width ) && 
		( scaled_height == height ) ) {
		if (!numMipmaps)
		{
			R_UploadLevel( R_ES3InternalFormat( internalFormat ), 0, scaled_width, scaled_height, data );
			SMP_RecordUploadLevel( 0, scaled_width, scaled_height );
			*pUploadWidth = scaled_width;
			*pUploadHeight = scaled_height;
			*format = internalFormat;
			*bytesUsed = samples * scaled_width * scaled_height;

			goto done;
		}
	}
	else
	{
		while ( width > scaled_width || height > scaled_height ) {
			R_MipMap( (byte *)data, width, height );
			width >>= 1;
			height >>= 1;
			if ( width < 1 ) {
				width = 1;
			}
			if ( height < 1 ) {
				height = 1;
			}
		}
	}

	R_LightScaleTexture (scaledBuffer, scaled_width, scaled_height, !numMipmaps);

	*pUploadWidth = scaled_width;
	*pUploadHeight = scaled_height;
	*format = internalFormat;

	R_UploadLevel( R_ES3InternalFormat( internalFormat ), 0, scaled_width, scaled_height, scaledBuffer );
	SMP_RecordUploadLevel( 0, scaled_width, scaled_height );

	*bytesUsed = samples * scaled_width * scaled_height;

	if (numMipmaps)
	{
		int		miplevel;

		miplevel = 0;
		while (scaled_width > 1 || scaled_height > 1)
		{
			R_MipMap( (byte *)scaledBuffer, scaled_width, scaled_height );
			scaled_width >>= 1;
			scaled_height >>= 1;
			if (scaled_width < 1)
				scaled_width = 1;
			if (scaled_height < 1)
				scaled_height = 1;
			miplevel++;

			if ( r_colorMipLevels->integer ) {
				R_BlendOverTexture( (byte *)scaledBuffer, scaled_width * scaled_height, mipBlendColors[miplevel] );
			}

			R_UploadLevel( R_ES3InternalFormat( internalFormat ), miplevel, scaled_width, scaled_height, scaledBuffer );
			SMP_RecordUploadLevel( miplevel, scaled_width, scaled_height );
		}
	}
done:

	if (numMipmaps)
	{
		if ( textureFilterAnisotropic )
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
					(GLint)maxAnisotropy );

		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		if (textureFilterAnisotropic)
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);

		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	}

	GL_CheckErrors();

	if ( resampledBuffer != 0 )
		ri.Hunk_FreeTempMemory( resampledBuffer );
}

static void UploadCompressed(
	byte* data,
	int width,
	int height,
	int numMipmaps,
	int iMipmapsAvailable,
	qboolean picmip,
	int glCompressMode,
	int* format,
	int* pUploadWidth,
	int* pUploadHeight,
	int* pBytesUsed
) {
	byte* offset;
	int i;
	int iStartImage;
	int size;
	int blockSize;
	int scaled_width, scaled_height;

	offset = data;
	iStartImage = 0;
	if (picmip > 0) {
		iStartImage = picmip;
	}

	if (iStartImage > 0 && iStartImage >= iMipmapsAvailable) {
		iStartImage = iMipmapsAvailable - 1;
	}

	scaled_width = width;
	scaled_height = height;
	if (iStartImage)
	{
		scaled_width = width >> iStartImage;
		scaled_height = height >> iStartImage;
	}
	while (scaled_width > glConfig.maxTextureSize || scaled_height > glConfig.maxTextureSize)
	{
		scaled_width >>= 1;
		scaled_height >>= 1;
		++iStartImage;
	}
	if (iStartImage > 0 && iStartImage >= iMipmapsAvailable)
	{
		iStartImage = iMipmapsAvailable - 1;
		scaled_width = width >> (iMipmapsAvailable - 1);
		scaled_height = height >> (iMipmapsAvailable - 1);
	}

	*pUploadWidth = scaled_width;
	*pUploadHeight = scaled_height;
	*format = glCompressMode;
	blockSize = 16;

	if (glCompressMode == GL_COMPRESSED_RGB_S3TC_DXT1_EXT || glCompressMode == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) {
		blockSize = 8;
	}

	scaled_width = width;
	scaled_height = height;

	for (i = 0; i < iStartImage; i++) {
		int w, h;

		w = (scaled_width + 3) / 4;
		if (w > 1) {
			h = (scaled_height + 3) / 4;
			if (h > 1) {
				offset += blockSize * w * h;
			}
		}

		scaled_width >>= 1;
		scaled_height >>= 1;

		if (!scaled_width) scaled_width = 1;
		if (!scaled_height) scaled_height = 1;
	}

	*pBytesUsed = 0;

	while (scaled_width || scaled_height) {
		int w, h;

		if (!scaled_width) scaled_width = 1;
		if (!scaled_height) scaled_height = 1;

		h = (scaled_height + 3) / 4;
		w = (scaled_width + 3) / 4;
		size = blockSize * w * h;

		if (w > 1 && h > 1) {
			*pBytesUsed += size;
		}

		qglCompressedTexImage2D(GL_TEXTURE_2D, i - iStartImage, glCompressMode, scaled_width, scaled_height, 0, size, offset);
		GL_CheckErrors();

		if (i < iMipmapsAvailable && w > 1 && h > 1) {
			offset += size;
		}

		scaled_width >>= 1;
		scaled_height >>= 1;
		i++;
	}

	if (numMipmaps > 1) {
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	} else {
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	GL_CheckErrors();
}

static void SMP_TranscodeRunJobPool(void* ud, smpTranscodeRows_t* ctx, int bh, int bw)
{
	(void)ud;
	JobPool_ParallelRange(bh, (2048 / (bw ? bw : 1)) + 1, SMP_TranscodeRows, ctx);
}

static cvar_t* smp_texCache;
static cvar_t* smp_texCacheMaxMB;

#define SMP_TEXCACHE_PROBE_MAX 512
static void SMP_TexCacheProbeRow(const char* imgName, const smpTexCacheResult_t* loadRes,
								  const smpTexCacheResult_t* storeRes,
								  int target, int base, int levels, int bytes)
{
	static char		s_seen[SMP_TEXCACHE_PROBE_MAX][64];
	static int		s_count;
	static qboolean	s_notice;
	const char*		load;

	/* the cache is a feature; its rows are an instrument like any other */
	if (!R_ProbeStatsOn()) {
		return;
	}
	const char*		store;
	int				i;

	for (i = 0; i < s_count; i++) {
		if (!strcmp(s_seen[i], imgName)) {
			return;
		}
	}
	if (s_count >= SMP_TEXCACHE_PROBE_MAX) {
		if (!s_notice) {
			s_notice = qtrue;
			ri.Printf(PRINT_ALL, "[SMP:texcache] rows capped at %d distinct images; the rest are not recorded\n",
					  SMP_TEXCACHE_PROBE_MAX);
		}
		return;
	}
	Q_strncpyz(s_seen[s_count], imgName, sizeof(s_seen[0]));
	s_count++;

	load = (loadRes && loadRes->stat == SMP_TEXCACHE_STAT_HIT) ? "hit"
		 : (loadRes && loadRes->stat == SMP_TEXCACHE_STAT_CORRUPT) ? "corrupt"
		 : (loadRes && loadRes->stat == SMP_TEXCACHE_STAT_MISS) ? "miss"
															      : "disabled";
	if (!storeRes) {
		store = "n/a";
	} else if (storeRes->stat == SMP_TEXCACHE_STAT_STORED) {
		store = "ok";
	} else if (storeRes->stat == SMP_TEXCACHE_STAT_SKIP) {
		store = "skip";
	} else if (storeRes->stat == SMP_TEXCACHE_STAT_WRITE_FAIL) {
		store = "fail";
	} else {
		store = "off";
	}

	ri.Printf(PRINT_ALL,
			  "[SMP:texcache] path=%s name=%s target=%s base=%d levels=%d bytes=%d load=%s store=%s evicted=%d\n",
			  loadRes ? loadRes->path : "-", imgName,
			  (target == SMP_TEXC_ASTC_4x4) ? "astc" : "etc2",
			  base, levels, bytes, load, store,
			  (loadRes ? loadRes->evicted : 0) + (storeRes ? storeRes->evicted : 0));
}

static void UploadTranscoded(
	const char* name,
	byte* data,
	int width,
	int height,
	int numMipmaps,
	int iMipmapsAvailable,
	qboolean picmip,
	int srcDxt,
	int target,
	int* format,
	int* pUploadWidth,
	int* pUploadHeight,
	int* bytesUsed
) {
	int	base, upW, upH, levels, lvl;
	int	w, h, totalBytes, off;
	byte*	blocksBuf = NULL;
	smpTexCacheKey_t key;
	smpTexCacheResult_t loadRes, storeRes;
	unsigned int srcLo, srcHi;
	int srcBytes;
	int fromCache = 0;

	SMP_RLOAD_BEGIN( transcode );

	SMP_InitCodecTables(); 
	memset(&loadRes, 0, sizeof(loadRes));
	memset(&storeRes, 0, sizeof(storeRes));

	base = (picmip > 0) ? (int)picmip : 0;
	if (iMipmapsAvailable > 0 && base >= iMipmapsAvailable) {
		base = iMipmapsAvailable - 1;
	}
	if (base < 0) {
		base = 0;
	}
	while (base < 30 && ((width >> base) > glConfig.maxTextureSize ||
						 (height >> base) > glConfig.maxTextureSize)) {
		base++;
	}
	if (iMipmapsAvailable > 0 && base >= iMipmapsAvailable) {
		base = iMipmapsAvailable - 1;
	}
	if (base < 0) {
		base = 0;
	}

	upW = width >> base;
	upH = height >> base;
	if (upW < 1) upW = 1;
	if (upH < 1) upH = 1;

	*pUploadWidth = upW;
	*pUploadHeight = upH;
	*format = (srcDxt == SMP_DXT1) ? GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
			: (srcDxt == SMP_DXT5) ? GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
									: GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;

	levels = 1;
	{
		int tw = upW, th = upH;

		while (numMipmaps && (tw > 1 || th > 1)) {
			if (tw > 1) tw >>= 1;
			if (th > 1) th >>= 1;
			levels++;
		}
	}

	totalBytes = SMP_TranscodeChainBytes(upW, upH, levels);

	SMP_TexCache_SourceHash((const unsigned char*)data, width, height, iMipmapsAvailable,
							 srcDxt, &srcLo, &srcHi, &srcBytes);
	SMP_TexCache_FillKey(&key, name, width, height, numMipmaps, (int)picmip,
						  glConfig.maxTextureSize, srcDxt, target,
						  base, upW, upH, levels, srcLo, srcHi, srcBytes);

	if (SMP_TexCache_Enabled()) {
		int cachedBytes = 0;

		if (SMP_TexCache_Load(&key, &blocksBuf, &cachedBytes, ri.Malloc, ri.Free, &loadRes) &&
			blocksBuf && cachedBytes == totalBytes) {
			fromCache = 1;
		} else if (blocksBuf) {
			ri.Free(blocksBuf);
			blocksBuf = NULL;
		}
	}

	if (!fromCache) {
		blocksBuf = ri.Malloc(totalBytes);
		SMP_TranscodeChain((const unsigned char*)data, srcDxt, width, height,
							iMipmapsAvailable, base, upW, upH, levels, target,
							(unsigned char*)blocksBuf, ri.Malloc, ri.Free,
							SMP_TranscodeRunJobPool, NULL);
		if (SMP_TexCache_Enabled()) {
			SMP_TexCache_Store(&key, blocksBuf, totalBytes, &storeRes);
		}
	}

	*bytesUsed = 0;
	off = 0;
	w = upW;
	h = upH;
	for (lvl = 0; lvl < levels; lvl++) {
		int bw = (w + 3) / 4;
		int bh = (h + 3) / 4;
		int size = bw * bh * SMP_TEXC_BLOCK_BYTES;

		qglCompressedTexImage2D(GL_TEXTURE_2D, lvl,
								(target == SMP_TEXC_ASTC_4x4) ? GL_COMPRESSED_RGBA_ASTC_4x4_KHR
															   : GL_COMPRESSED_RGBA8_ETC2_EAC,
								w, h, 0, size, blocksBuf + off);
		SMP_RecordUploadLevel(lvl, w, h);
		GL_CheckErrors();
		*bytesUsed += size;
		off += size;

		if (w > 1) w >>= 1;
		if (h > 1) h >>= 1;
	}

	ri.Free(blocksBuf);
	blocksBuf = NULL;

	SMP_TexCacheProbeRow(name, &loadRes, fromCache ? NULL : &storeRes,
						  target, base, levels, *bytesUsed);

	if (numMipmaps) {
		if (textureFilterAnisotropic) {
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, (GLint)maxAnisotropy);
		}
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	} else {
		if (textureFilterAnisotropic) {
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);
		}
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	GL_CheckErrors();

	SMP_RLOAD_END( transcode );
}

image_t* R_CreateImageOld(
	const char* name,
	byte* pic,
	int width,
	int height,
	int numMipmaps,
	int iMipmapsAvailable,
	qboolean allowPicmip,
	qboolean force32bit,
	qboolean hasAlpha,
	int glCompressMode,
	int textureMode
) {
	image_t		*image;
	qboolean	isLightmap = qfalse;
	qboolean	dynamicallyUpdated = qfalse;
	int i;
	long		hash;

	if (strlen(name) >= MAX_QPATH ) {
		ri.Error (ERR_DROP, "R_CreateImage: \"%s\" is too long\n", name);
	}
	if (!strncmp(name, "*lightmap", 9)) {
		isLightmap = qtrue;
	}
	else if (!strncmp(name, "*lightmapD", 10)) {
		dynamicallyUpdated = qtrue;
	}

	for (i = 0; i < tr.numImages; i++)
	{
		if (!tr.images[i].imgName[0])
		{
			break;
		}
	}

	if (i == tr.numImages)
    {
        if (tr.numImages == MAX_DRAWIMAGES) {
            ri.Error(ERR_DROP, "R_CreateImage: MAX_DRAWIMAGES hit\n");
		} else {
			tr.numImages++;
		}
	}

	/* Every GL call below belongs to the texture-upload class of driver-resource
	 * ops, so under r_smp this thread must own the context before the first of
	 * them.  Without the park the context stays with the render thread, gen
	 * returns 0 and the whole upload silently no-ops -- the image is then bound
	 * as texture 0 and samples as a flat square. */
	R_IssuePendingRenderCommands();

	image = &tr.images[i];
    qglGenTextures(1, &image->texnum);

	image->numMipmaps = numMipmaps;
	image->allowPicmip = allowPicmip;
	image->dynamicallyUpdated = dynamicallyUpdated;
	image->force32bit = force32bit;

	Q_strncpyz (image->imgName, name, sizeof(image->imgName));

	image->width = width;
	image->height = height;
	image->textureMode = textureMode;
	image->wrapClampModeX = getGLClampType( GL_TEXTURE_WRAP_S, textureMode );
	image->wrapClampModeY = getGLClampType( GL_TEXTURE_WRAP_T, textureMode );

	if ( qglActiveTextureARB && isLightmap ) {
		image->TMU = 1;
	} else {
		image->TMU = 0;
	}

	if ( qglActiveTextureARB ) {
		GL_SelectTexture( image->TMU );
	}

	GL_Bind(image);

	if (r_developer->integer) {
		verifyProperTextureSize( name, image->width, image->height );
	}

	SMP_RLOAD_BEGIN( upload );

	int be_imgT0 = BE_On() ? BE_Microseconds() : 0;

	if (glCompressMode) {
		if (glConfig.textureCompression != TC_NONE) {
			s_gles3PathKind = 1; 
			s_gles3PathFmt = glCompressMode;
			s_gles3PathSrcDxt = (glCompressMode == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) ? SMP_DXT1
							 : (glCompressMode == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT) ? SMP_DXT5
																					 : SMP_DXT3;
			UploadCompressed(
				(byte*)pic,
				image->width,
				image->height,
				image->numMipmaps,
				iMipmapsAvailable,
				allowPicmip,
				glCompressMode,
				&image->internalFormat,
				&image->uploadWidth,
				&image->uploadHeight,
				&image->bytesUsed
			);
		} else if (smp_texTranscode != SMP_TEXC_NONE) {
			s_gles3PathKind = (smp_texTranscode == SMP_TEXC_ASTC_4x4) ? 3 : 2;
			s_gles3PathFmt = (smp_texTranscode == SMP_TEXC_ASTC_4x4)
								? GL_COMPRESSED_RGBA_ASTC_4x4_KHR
								: GL_COMPRESSED_RGBA8_ETC2_EAC;
			s_gles3PathSrcDxt = (glCompressMode == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) ? SMP_DXT1
							 : (glCompressMode == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT) ? SMP_DXT5
																					 : SMP_DXT3;
			UploadTranscoded(
				name,
				(byte*)pic,
				image->width,
				image->height,
				image->numMipmaps,
				iMipmapsAvailable,
				allowPicmip,
				s_gles3PathSrcDxt,
				smp_texTranscode,
				&image->internalFormat,
				&image->uploadWidth,
				&image->uploadHeight,
				&image->bytesUsed
			);
		} else {
			ri.Error(ERR_DROP, "R_CreateImage: compressed payload with no upload path\n");
		}
	}
	else {
		s_gles3PathKind = 0; 
		s_smpWantStats = ( strstr( name, "common/shadow" ) != NULL ) ? qtrue : qfalse;

		Upload32(
			(unsigned int*)pic,
			image->width,
			image->height,
			image->numMipmaps,
			allowPicmip,
			image->dynamicallyUpdated,
			force32bit,
			hasAlpha,
			&image->internalFormat,
			&image->uploadWidth,
			&image->uploadHeight,
			&image->bytesUsed,
			isLightmap,
			(qboolean)( name[0] != '*' )
		);

		s_smpWantStats = qfalse;
	}

	SMP_RLOAD_END( upload );

#define SMP_IMGUP_MAX 512
	if ( R_ProbeStatsOn() )
	{
		static char		*s_smpImgupSeen[SMP_IMGUP_MAX];
		static int		 s_smpImgupCount;
		static qboolean	 s_smpImgupNotice;
		static const char *s_gles3PathName[4] = { "software", "s3tc", "etc2", "astc" };
		GLenum			 err = qglGetError();
		int				 iSeen;
		qboolean		 seen = qfalse;
		int				 rowFmt;

		rowFmt = ( s_gles3PathKind == 0 ) ? (int)R_ES3InternalFormat( s_gles3UploadFormat )
										 : s_gles3PathFmt;

		for ( iSeen = 0; iSeen < s_smpImgupCount; iSeen++ ) {
			if ( !strcmp( s_smpImgupSeen[iSeen], image->imgName ) ) {
				seen = qtrue;
				break;
			}
		}
		if ( !seen ) {
			if ( s_smpImgupCount >= SMP_IMGUP_MAX ) {
				if ( !s_smpImgupNotice ) {
					s_smpImgupNotice = qtrue;
					ri.Printf( PRINT_ALL,
						"[GLES3:imgup] NAME BUDGET reached (%d distinct images recorded); "
						"further uploads are NOT recorded from here on\n", SMP_IMGUP_MAX );
				}
			} else {
				s_smpImgupSeen[s_smpImgupCount] = ri.Malloc( (int)strlen( image->imgName ) + 1 );
				strcpy( s_smpImgupSeen[s_smpImgupCount], image->imgName );
				s_smpImgupCount++;

				ri.Printf( PRINT_ALL,
					"[GLES3:imgup] name=%s w=%d h=%d path=%s fmt=0x%04x samples=%d literal=0x%04x "
					"mips=%d bytes=%d lightmap=%d dynamic=%d err=0x%04x usec=%d\n",
					image->imgName, image->uploadWidth, image->uploadHeight,
					s_gles3PathName[s_gles3PathKind & 3], (unsigned)rowFmt,
					( s_gles3PathKind == 0 ) ? s_gles3UploadSamples : 0,
					( s_gles3PathKind == 0 ) ? (unsigned)s_gles3UploadFormat : 0u,
					image->numMipmaps, image->bytesUsed, isLightmap ? 1 : 0,
					image->dynamicallyUpdated ? 1 : 0, (unsigned)err,
					be_imgT0 ? ( BE_Microseconds() - be_imgT0 ) : 0 );

				if ( s_gles3PathKind == 0 && strstr( image->imgName, "common/shadow" ) != NULL ) {
					ri.Printf( PRINT_ALL,
						"[SMP:decalsrc] name=%s px=%d rgb=%lu,%lu,%lu mean=%.1f,%.1f,%.1f"
						" nearwhite=%d (of %d; r,g,b all >= 200)"
						" issuedLevels=%d lvl0..7=%dx%d,%dx%d,%dx%d,%dx%d,%dx%d,%dx%d,%dx%d,%dx%d\n",
						image->imgName, s_smpUploadPixels,
						s_smpUploadSum[0], s_smpUploadSum[1], s_smpUploadSum[2],
						s_smpUploadPixels ? (double)s_smpUploadSum[0] / s_smpUploadPixels : 0.0,
						s_smpUploadPixels ? (double)s_smpUploadSum[1] / s_smpUploadPixels : 0.0,
						s_smpUploadPixels ? (double)s_smpUploadSum[2] / s_smpUploadPixels : 0.0,
						s_smpUploadWhite, s_smpUploadPixels,
						s_smpUploadLvlN,
						s_smpUploadLvlW[0], s_smpUploadLvlH[0],
						s_smpUploadLvlW[1], s_smpUploadLvlH[1],
						s_smpUploadLvlW[2], s_smpUploadLvlH[2],
						s_smpUploadLvlW[3], s_smpUploadLvlH[3],
						s_smpUploadLvlW[4], s_smpUploadLvlH[4],
						s_smpUploadLvlW[5], s_smpUploadLvlH[5],
						s_smpUploadLvlW[6], s_smpUploadLvlH[6],
						s_smpUploadLvlW[7], s_smpUploadLvlH[7] );
				}
			}
		}
	}

	{
		GLenum wrapS = (GLenum)image->wrapClampModeX;
		GLenum wrapT = (GLenum)image->wrapClampModeY;

		if ( !Q_stricmpn( glConfig.renderer_string, "OpenGL ES", 9 ) ) {
			if ( wrapS == GL_CLAMP ) {
				wrapS = GL_CLAMP_TO_EDGE;
			}
			if ( wrapT == GL_CLAMP ) {
				wrapT = GL_CLAMP_TO_EDGE;
			}
		}

		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT );
		image->wrapSetX = (int)wrapS;
		image->wrapSetY = (int)wrapT;
	}

	qglBindTexture( GL_TEXTURE_2D, 0 );

	glState.currenttextures[image->TMU] = -1;
	if ( image->TMU == 1 ) {
		GL_SelectTexture( 0 );
	}

	hash = generateHashValue(name);
	image->next = hashTable[hash];
	hashTable[hash] = image;
	image->r_sequence = r_sequencenumber;

	return image;
}



typedef unsigned int   ui32_t;

typedef struct ddsHeader_s
{
	ui32_t headerSize;
	ui32_t flags;
	ui32_t height;
	ui32_t width;
	ui32_t pitchOrFirstMipSize;
	ui32_t volumeDepth;
	ui32_t numMips;
	ui32_t reserved1[11];
	ui32_t always_0x00000020;
	ui32_t pixelFormatFlags;
	ui32_t fourCC;
	ui32_t rgbBitCount;
	ui32_t rBitMask;
	ui32_t gBitMask;
	ui32_t bBitMask;
	ui32_t aBitMask;
	ui32_t caps;
	ui32_t caps2;
	ui32_t caps3;
	ui32_t caps4;
	ui32_t reserved2;
}
ddsHeader_t;

#define _DDSFLAGS_REQUIRED     0x001007
#define _DDSFLAGS_PITCH        0x8
#define _DDSFLAGS_MIPMAPCOUNT  0x20000
#define _DDSFLAGS_FIRSTMIPSIZE 0x80000
#define _DDSFLAGS_VOLUMEDEPTH  0x800000

#define DDSPF_ALPHAPIXELS 0x1
#define DDSPF_ALPHA       0x2
#define DDSPF_FOURCC      0x4
#define DDSPF_RGB         0x40
#define DDSPF_YUV         0x200
#define DDSPF_LUMINANCE   0x20000

#define DDSCAPS_COMPLEX  0x8
#define DDSCAPS_MIPMAP   0x400000
#define DDSCAPS_REQUIRED 0x1000

#define DDSCAPS2_CUBEMAP 0xFE00
#define DDSCAPS2_VOLUME  0x200000

typedef struct ddsHeaderDxt10_s
{
	ui32_t dxgiFormat;
	ui32_t dimensions;
	ui32_t miscFlags;
	ui32_t arraySize;
	ui32_t miscFlags2;
}
ddsHeaderDxt10_t;

typedef enum DXGI_FORMAT {
	DXGI_FORMAT_UNKNOWN = 0,
	DXGI_FORMAT_R32G32B32A32_TYPELESS = 1,
	DXGI_FORMAT_R32G32B32A32_FLOAT = 2,
	DXGI_FORMAT_R32G32B32A32_UINT = 3,
	DXGI_FORMAT_R32G32B32A32_SINT = 4,
	DXGI_FORMAT_R32G32B32_TYPELESS = 5,
	DXGI_FORMAT_R32G32B32_FLOAT = 6,
	DXGI_FORMAT_R32G32B32_UINT = 7,
	DXGI_FORMAT_R32G32B32_SINT = 8,
	DXGI_FORMAT_R16G16B16A16_TYPELESS = 9,
	DXGI_FORMAT_R16G16B16A16_FLOAT = 10,
	DXGI_FORMAT_R16G16B16A16_UNORM = 11,
	DXGI_FORMAT_R16G16B16A16_UINT = 12,
	DXGI_FORMAT_R16G16B16A16_SNORM = 13,
	DXGI_FORMAT_R16G16B16A16_SINT = 14,
	DXGI_FORMAT_R32G32_TYPELESS = 15,
	DXGI_FORMAT_R32G32_FLOAT = 16,
	DXGI_FORMAT_R32G32_UINT = 17,
	DXGI_FORMAT_R32G32_SINT = 18,
	DXGI_FORMAT_R32G8X24_TYPELESS = 19,
	DXGI_FORMAT_D32_FLOAT_S8X24_UINT = 20,
	DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS = 21,
	DXGI_FORMAT_X32_TYPELESS_G8X24_UINT = 22,
	DXGI_FORMAT_R10G10B10A2_TYPELESS = 23,
	DXGI_FORMAT_R10G10B10A2_UNORM = 24,
	DXGI_FORMAT_R10G10B10A2_UINT = 25,
	DXGI_FORMAT_R11G11B10_FLOAT = 26,
	DXGI_FORMAT_R8G8B8A8_TYPELESS = 27,
	DXGI_FORMAT_R8G8B8A8_UNORM = 28,
	DXGI_FORMAT_R8G8B8A8_UNORM_SRGB = 29,
	DXGI_FORMAT_R8G8B8A8_UINT = 30,
	DXGI_FORMAT_R8G8B8A8_SNORM = 31,
	DXGI_FORMAT_R8G8B8A8_SINT = 32,
	DXGI_FORMAT_R16G16_TYPELESS = 33,
	DXGI_FORMAT_R16G16_FLOAT = 34,
	DXGI_FORMAT_R16G16_UNORM = 35,
	DXGI_FORMAT_R16G16_UINT = 36,
	DXGI_FORMAT_R16G16_SNORM = 37,
	DXGI_FORMAT_R16G16_SINT = 38,
	DXGI_FORMAT_R32_TYPELESS = 39,
	DXGI_FORMAT_D32_FLOAT = 40,
	DXGI_FORMAT_R32_FLOAT = 41,
	DXGI_FORMAT_R32_UINT = 42,
	DXGI_FORMAT_R32_SINT = 43,
	DXGI_FORMAT_R24G8_TYPELESS = 44,
	DXGI_FORMAT_D24_UNORM_S8_UINT = 45,
	DXGI_FORMAT_R24_UNORM_X8_TYPELESS = 46,
	DXGI_FORMAT_X24_TYPELESS_G8_UINT = 47,
	DXGI_FORMAT_R8G8_TYPELESS = 48,
	DXGI_FORMAT_R8G8_UNORM = 49,
	DXGI_FORMAT_R8G8_UINT = 50,
	DXGI_FORMAT_R8G8_SNORM = 51,
	DXGI_FORMAT_R8G8_SINT = 52,
	DXGI_FORMAT_R16_TYPELESS = 53,
	DXGI_FORMAT_R16_FLOAT = 54,
	DXGI_FORMAT_D16_UNORM = 55,
	DXGI_FORMAT_R16_UNORM = 56,
	DXGI_FORMAT_R16_UINT = 57,
	DXGI_FORMAT_R16_SNORM = 58,
	DXGI_FORMAT_R16_SINT = 59,
	DXGI_FORMAT_R8_TYPELESS = 60,
	DXGI_FORMAT_R8_UNORM = 61,
	DXGI_FORMAT_R8_UINT = 62,
	DXGI_FORMAT_R8_SNORM = 63,
	DXGI_FORMAT_R8_SINT = 64,
	DXGI_FORMAT_A8_UNORM = 65,
	DXGI_FORMAT_R1_UNORM = 66,
	DXGI_FORMAT_R9G9B9E5_SHAREDEXP = 67,
	DXGI_FORMAT_R8G8_B8G8_UNORM = 68,
	DXGI_FORMAT_G8R8_G8B8_UNORM = 69,
	DXGI_FORMAT_BC1_TYPELESS = 70,
	DXGI_FORMAT_BC1_UNORM = 71,
	DXGI_FORMAT_BC1_UNORM_SRGB = 72,
	DXGI_FORMAT_BC2_TYPELESS = 73,
	DXGI_FORMAT_BC2_UNORM = 74,
	DXGI_FORMAT_BC2_UNORM_SRGB = 75,
	DXGI_FORMAT_BC3_TYPELESS = 76,
	DXGI_FORMAT_BC3_UNORM = 77,
	DXGI_FORMAT_BC3_UNORM_SRGB = 78,
	DXGI_FORMAT_BC4_TYPELESS = 79,
	DXGI_FORMAT_BC4_UNORM = 80,
	DXGI_FORMAT_BC4_SNORM = 81,
	DXGI_FORMAT_BC5_TYPELESS = 82,
	DXGI_FORMAT_BC5_UNORM = 83,
	DXGI_FORMAT_BC5_SNORM = 84,
	DXGI_FORMAT_B5G6R5_UNORM = 85,
	DXGI_FORMAT_B5G5R5A1_UNORM = 86,
	DXGI_FORMAT_B8G8R8A8_UNORM = 87,
	DXGI_FORMAT_B8G8R8X8_UNORM = 88,
	DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM = 89,
	DXGI_FORMAT_B8G8R8A8_TYPELESS = 90,
	DXGI_FORMAT_B8G8R8A8_UNORM_SRGB = 91,
	DXGI_FORMAT_B8G8R8X8_TYPELESS = 92,
	DXGI_FORMAT_B8G8R8X8_UNORM_SRGB = 93,
	DXGI_FORMAT_BC6H_TYPELESS = 94,
	DXGI_FORMAT_BC6H_UF16 = 95,
	DXGI_FORMAT_BC6H_SF16 = 96,
	DXGI_FORMAT_BC7_TYPELESS = 97,
	DXGI_FORMAT_BC7_UNORM = 98,
	DXGI_FORMAT_BC7_UNORM_SRGB = 99,
	DXGI_FORMAT_AYUV = 100,
	DXGI_FORMAT_Y410 = 101,
	DXGI_FORMAT_Y416 = 102,
	DXGI_FORMAT_NV12 = 103,
	DXGI_FORMAT_P010 = 104,
	DXGI_FORMAT_P016 = 105,
	DXGI_FORMAT_420_OPAQUE = 106,
	DXGI_FORMAT_YUY2 = 107,
	DXGI_FORMAT_Y210 = 108,
	DXGI_FORMAT_Y216 = 109,
	DXGI_FORMAT_NV11 = 110,
	DXGI_FORMAT_AI44 = 111,
	DXGI_FORMAT_IA44 = 112,
	DXGI_FORMAT_P8 = 113,
	DXGI_FORMAT_A8P8 = 114,
	DXGI_FORMAT_B4G4R4A4_UNORM = 115,
	DXGI_FORMAT_FORCE_UINT = 0xffffffffUL
} DXGI_FORMAT;

#define EncodeFourCC(x) ((((ui32_t)((x)[0]))      ) | \
                         (((ui32_t)((x)[1])) << 8 ) | \
                         (((ui32_t)((x)[2])) << 16) | \
                         (((ui32_t)((x)[3])) << 24) )


typedef struct {
    int dxt;
    const byte *src;
    byte *dst;
    int w, h, bw;
    int blockBytes;
} dxtRows_t;

static void R_DecompressDXTRows(void *vctx, int begin, int end)
{
    const dxtRows_t *c = (const dxtRows_t *)vctx;
    int bw = c->bw;
    int w = c->w, h = c->h;
    int blockBytes = c->blockBytes;
    int bx, by, i;

    for (by = begin; by < end; by++) {
        for (bx = 0; bx < bw; bx++) {
            const byte *block = c->src + (by * bw + bx) * blockBytes;
            byte        rgba[16 * 4];

            SMP_DXTDecodeBlock(c->dxt, block, rgba);

            for (i = 0; i < 16; i++) {
                int   px = bx * 4 + (i & 3);
                int   py = by * 4 + (i >> 2);
                byte *d;

                if (px >= w || py >= h) {
                    continue;
                }
                d    = c->dst + (py * w + px) * 4;
                d[0] = rgba[i * 4 + 0];
                d[1] = rgba[i * 4 + 1];
                d[2] = rgba[i * 4 + 2];
                d[3] = rgba[i * 4 + 3];
            }
        }
    }
}

static void R_DecompressDXT(int dxt, const byte *src, int w, int h, byte *dst)
{
    dxtRows_t c;

    c.dxt = dxt;
    c.src = src;
    c.dst = dst;
    c.w = w;
    c.h = h;
    c.bw = (w + 3) / 4;
    c.blockBytes = (dxt == 1) ? 8 : 16;

    JobPool_ParallelRange( (h + 3) / 4, ( 16384 / ( c.bw ? c.bw : 1 ) ) + 1,
                           R_DecompressDXTRows, &c );
}

static void LoadDDS(const char* name, byte** pic, int* width, int* height, qboolean* hasAlpha, int* glCompressMode, int* numMipmaps, int* piMipmapsAvailable) {
	int len;
	unsigned int signature;
	ddsHeader_t ddsHeader;
	fileHandle_t handle;
	int w, h;
	int minWidth, minHeight;
	int numMips = 0;
	int size;

	*pic = NULL;
	*piMipmapsAvailable = 0;

	if (width)
		*width = 0;
	if (height)
		*height = 0;

	len = ri.FS_OpenFile((char*)name, &handle, qtrue, qtrue);
	if (len < 0) {
		return;
	}

	if (len < 4 + sizeof(ddsHeader))
	{
		ri.Printf(PRINT_ALL, "File %s is too small to be a DDS file.\n", name);
		ri.FS_CloseFile(handle);
		return;
	}

	ri.FS_Read(&signature, sizeof(signature), handle);
	if (strncmp((const char*)&signature, "DDS ", 4))
	{
		ri.Printf(PRINT_ALL, "File %s is not a DDS file.\n", name);
		ri.FS_CloseFile(handle);
		return;
	}

	ri.FS_Read(&ddsHeader, sizeof(ddsHeader), handle);

	ddsHeader.headerSize = LittleLong(ddsHeader.headerSize);
	ddsHeader.flags = LittleLong(ddsHeader.flags);
	ddsHeader.width = LittleLong(ddsHeader.width);
	ddsHeader.height = LittleLong(ddsHeader.height);
	ddsHeader.numMips = LittleLong(ddsHeader.numMips);
	ddsHeader.pitchOrFirstMipSize = LittleLong(ddsHeader.pitchOrFirstMipSize);
	ddsHeader.fourCC = LittleLong(ddsHeader.fourCC);

	if (ddsHeader.fourCC == EncodeFourCC("DXT1")) {
		*glCompressMode = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
	} else if (ddsHeader.fourCC == EncodeFourCC("DXT2")) {
		*glCompressMode = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
	} else if (ddsHeader.fourCC == EncodeFourCC("DXT3")) {
		*glCompressMode = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
	} else if (ddsHeader.fourCC == EncodeFourCC("DXT4")) {
		*glCompressMode = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
	} else if (ddsHeader.fourCC == EncodeFourCC("DXT5")) {
		*glCompressMode = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
	} else {
		ri.FS_CloseFile(handle);
		return;
	}

	minWidth = ddsHeader.width;
	for (w = 1; w < ddsHeader.width; w *= 2) {}

	minHeight = ddsHeader.height;
	for (h = 1; h < ddsHeader.height; h *= 2) {}

	if (w != ddsHeader.width || h != ddsHeader.height) {
		ri.FS_CloseFile(handle);
		if (w == ddsHeader.width) {
			ri.Printf(PRINT_DEVELOPER, "LoadDDS: Image %s - height not a power of 2\n", name);
		} else if (h == ddsHeader.height) {
			ri.Printf(PRINT_DEVELOPER, "LoadDDS: Image %s - width not a power of 2\n", name);
		} else {
			ri.Printf(PRINT_DEVELOPER, "LoadDDS: Image %s - width and height not a powers of 2\n", name);
		}

		return;
	}

	if (!minWidth) {
		minWidth = 1;
	}
	*width = minWidth;

	if (!minHeight) {
		minHeight = 1;
	}
	*height = minHeight;

	*hasAlpha = ddsHeader.fourCC != EncodeFourCC("DXT1");

	numMips = ddsHeader.numMips;
	if (!numMips) {
		numMips = 1;
	}

	*piMipmapsAvailable = numMips;
	if (*numMipmaps) {
		*numMipmaps = numMips;
	}

	{
		int   dxt        = (*glCompressMode == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) ? 1
		                 : (*glCompressMode == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT) ? 5
		                                                                         : 3;
		int   blockBytes = (dxt == 1) ? 8 : 16;
		int   baseSize   = ((minWidth + 3) / 4) * ((minHeight + 3) / 4) * blockBytes;

		if (glConfig.textureCompression != TC_NONE ||
			smp_texTranscode != SMP_TEXC_NONE) {
			{
				int	lw = minWidth, lh = minHeight, lvl, total = 0;

				for ( lvl = 0; lvl < numMips; lvl++ ) {
					total += ((lw + 3) / 4) * ((lh + 3) / 4) * blockBytes;
					if ( lw > 1 ) lw >>= 1;
					if ( lh > 1 ) lh >>= 1;
				}
				if ( total < baseSize ) {
					total = baseSize;
				}
				size = total;
			}

			*pic = ri.Malloc(size);
			ri.FS_Read(*pic, size, handle);
		} else {
			byte *compressed = ri.Malloc(baseSize);
			byte *rgba        = ri.Malloc(minWidth * minHeight * 4);

			ri.FS_Read(compressed, baseSize, handle);
			R_DecompressDXT(dxt, compressed, minWidth, minHeight, rgba);
			ri.Free(compressed);

			*pic                = rgba;
			*glCompressMode     = 0;
			*piMipmapsAvailable = 0;
		}
	}

	ri.FS_CloseFile(handle);
}


static void LoadTGA ( const char *name, byte **pic, int *width, int *height)
{
	int		columns, rows, numPixels;
	byte	*pixbuf;
	int		row, column;
	byte	*buf_p;
	byte	*buffer;
	TargaHeader	targa_header;
	byte		*targa_rgba;

	*pic = NULL;

	ri.FS_ReadFile ( ( char * ) name, (void **)&buffer);
	if (!buffer) {
		return;
	}

	buf_p = buffer;

	targa_header.id_length = *buf_p++;
	targa_header.colormap_type = *buf_p++;
	targa_header.image_type = *buf_p++;
	
	targa_header.colormap_index = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.colormap_length = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.colormap_size = *buf_p++;
	targa_header.x_origin = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.y_origin = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.width = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.height = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.pixel_size = *buf_p++;
	targa_header.attributes = *buf_p++;

	if (targa_header.image_type!=2 
		&& targa_header.image_type!=10
		&& targa_header.image_type != 3 ) 
	{
		ri.Error (ERR_DROP, "LoadTGA: Only type 2 (RGB), 3 (gray), and 10 (RGB) TGA images supported\n");
	}

	if ( targa_header.colormap_type != 0 )
	{
		ri.Error( ERR_DROP, "LoadTGA: colormaps not supported\n" );
	}

	if ( ( targa_header.pixel_size != 32 && targa_header.pixel_size != 24 ) && targa_header.image_type != 3 )
	{
		ri.Error (ERR_DROP, "LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n");
	}

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	if (width)
		*width = columns;
	if (height)
		*height = rows;

	targa_rgba = ri.Malloc (numPixels*4);
	*pic = targa_rgba;

	if (targa_header.id_length != 0)
		buf_p += targa_header.id_length;  
	
	if ( targa_header.image_type==2 || targa_header.image_type == 3 )
	{ 
		for(row=rows-1; row>=0; row--) 
		{
			pixbuf = targa_rgba + row*columns*4;
			for(column=0; column<columns; column++) 
			{
				unsigned char red,green,blue,alphabyte;
				switch (targa_header.pixel_size) 
				{
					
				case 8:
					blue = *buf_p++;
					green = blue;
					red = blue;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = 255;
					break;

				case 24:
					blue = *buf_p++;
					green = *buf_p++;
					red = *buf_p++;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = 255;
					break;
				case 32:
					blue = *buf_p++;
					green = *buf_p++;
					red = *buf_p++;
					alphabyte = *buf_p++;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = alphabyte;
					break;
				default:
					ri.Error( ERR_DROP, "LoadTGA: illegal pixel_size '%d' in file '%s'\n", targa_header.pixel_size, name );
					break;
				}
			}
		}
	}
	else if (targa_header.image_type==10) {   
		unsigned char red,green,blue,alphabyte,packetHeader,packetSize,j;

		red = 0;
		green = 0;
		blue = 0;
		alphabyte = 0xff;

		for(row=rows-1; row>=0; row--) {
			pixbuf = targa_rgba + row*columns*4;
			for(column=0; column<columns; ) {
				packetHeader= *buf_p++;
				packetSize = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80) {        
					switch (targa_header.pixel_size) {
						case 24:
								blue = *buf_p++;
								green = *buf_p++;
								red = *buf_p++;
								alphabyte = 255;
								break;
						case 32:
								blue = *buf_p++;
								green = *buf_p++;
								red = *buf_p++;
								alphabyte = *buf_p++;
								break;
						default:
							ri.Error( ERR_DROP, "LoadTGA: illegal pixel_size '%d' in file '%s'\n", targa_header.pixel_size, name );
							break;
					}
	
					for(j=0;j<packetSize;j++) {
						*pixbuf++=red;
						*pixbuf++=green;
						*pixbuf++=blue;
						*pixbuf++=alphabyte;
						column++;
						if (column==columns) { 
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row*columns*4;
						}
					}
				}
				else {                            
					for(j=0;j<packetSize;j++) {
						switch (targa_header.pixel_size) {
							case 24:
									blue = *buf_p++;
									green = *buf_p++;
									red = *buf_p++;
									*pixbuf++ = red;
									*pixbuf++ = green;
									*pixbuf++ = blue;
									*pixbuf++ = 255;
									break;
							case 32:
									blue = *buf_p++;
									green = *buf_p++;
									red = *buf_p++;
									alphabyte = *buf_p++;
									*pixbuf++ = red;
									*pixbuf++ = green;
									*pixbuf++ = blue;
									*pixbuf++ = alphabyte;
									break;
							default:
								ri.Error( ERR_DROP, "LoadTGA: illegal pixel_size '%d' in file '%s'\n", targa_header.pixel_size, name );
								break;
						}
						column++;
						if (column==columns) { 
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row*columns*4;
						}						
					}
				}
			}
			breakOut:;
		}
	}

#if 0 
  if (targa_header.attributes & 0x20) {
    unsigned char *flip = (unsigned char*)malloc (columns*4);
    unsigned char *src, *dst;

    for (row = 0; row < rows/2; row++) {
      src = targa_rgba + row * 4 * columns;
      dst = targa_rgba + (rows - row - 1) * 4 * columns;

      memcpy (flip, src, columns*4);
      memcpy (src, dst, columns*4);
      memcpy (dst, flip, columns*4);
    }
    free (flip);
  }
#endif
  if (targa_header.attributes & 0x20) {
    ri.Printf( PRINT_WARNING, "WARNING: '%s' TGA file header declares top-down image, ignoring\n", name);
  }

  ri.FS_FreeFile (buffer);
}


static void R_LoadImage(const char* name, byte** pic, int* width, int* height, qboolean* hasAlpha, int* glCompressMode, int* numMipmaps, int* piMipmapsAvailable) {
	size_t	len;
	char	tempName[MAX_STRING_TOKENS + 1];
	char altname[MAX_QPATH];

	*hasAlpha = qfalse;
	*glCompressMode = 0;
	*pic = NULL;
	*width = 0;
	*height = 0;
	*piMipmapsAvailable = 0;

	len = strlen(name);
	if (len < 5) {
		return;
	}

	SMP_RLOAD_BEGIN( decode );

	strcpy(altname, name);
	len = strlen(altname);

	if (!Q_stricmp(name + len - 4, ".tga")) {
		altname[len - 3] = 'd';
		altname[len - 2] = 'd';
		altname[len - 1] = 's';
		LoadDDS(altname, pic, width, height, hasAlpha, glCompressMode, numMipmaps, piMipmapsAvailable);
		if (*glCompressMode == 0) {
			*piMipmapsAvailable = 1;
		}

		// retail's fallbacks after a failed .dds: FT2/FTX (not reproduced, see
		strcpy(altname, name);
		if (!*pic) {
			LoadTGA(altname, pic, width, height);
		}
	} else if (!Q_stricmp(name + len - 4, ".gst")) {
		LoadGHOST(name, pic, width, height);
	}

	if (tr.registered)
	{
		Com_sprintf(tempName, sizeof(tempName), "n%s", name);
		ri.UI_LoadResource(tempName);
	}

	SMP_RLOAD_END( decode );
}


image_t* R_FindImageFileOld(const char* name, qboolean mipmap, qboolean allowPicmip, qboolean force32bit, int textureMode) {
	image_t	*image;
	int		width, height;
	byte	*pic;
	int		len;
	qboolean	hasAlpha;
	int			glCompressMode;
	int			numMipmaps;
	int			iMipmapsAvailable;
	long	hash;

	if (!name) {
		return NULL;
	}

	hash = generateHashValue(name);

	for (image = hashTable[hash]; image; image = image->next) {
		if (!strcmp(name, image->imgName)) {
			if (strcmp(name, "*white")) {
				if ((image->numMipmaps && !mipmap) || (!image->numMipmaps && mipmap)) {
					ri.Printf(PRINT_WARNING, "WARNING: reused image %s with mixed mipmap parm\n", name);
				}
				if (image->allowPicmip != allowPicmip) {
					ri.Printf(PRINT_WARNING, "WARNING: reused image %s with mixed allowPicmip parm\n", name);
				}
				if (image->textureMode != textureMode) {
					ri.Printf(PRINT_WARNING, "WARNING: reused image %s with mixed glWrapClampMode parm\n", name);
				}
			}
			if (image->r_sequence != -1) {
				image->r_sequence = r_sequencenumber;
			}
			return image;
		}
	}

	SMP_RLOAD_BEGIN( findimg );

	numMipmaps = mipmap;
	iMipmapsAvailable = 0;
	R_LoadImage(name, &pic, &width, &height, &hasAlpha, &glCompressMode, &numMipmaps, &iMipmapsAvailable);
	if (pic == NULL) {
		SMP_RLOAD_END( findimg );
		return NULL;
	}

	image = R_CreateImageOld(
		name,
		pic,
		width,
		height,
		numMipmaps,
		iMipmapsAvailable,
		allowPicmip,
		force32bit,
		hasAlpha,
		glCompressMode,
		textureMode);

    len = strlen(name);
    if (len > 4 && !strcmp(&name[len - 4], ".gst")) {
        R_SetGhostImage(name, image);
    }

	ri.Free(pic);
	SMP_RLOAD_END( findimg );
	return image;
}

image_t* R_RefreshImageFileOld(const char* name, qboolean mipmap, qboolean allowPicmip, qboolean force32bit, int textureMode) {
	char imagename[64];
	image_t* image;
	long hash;
	
	if (!name) {
		return NULL;
	}

	/* This frees the old image before R_FindImageFileOld creates its
	 * replacement, and the free is a driver-resource destruction.  The create
	 * path claims in R_CreateImageOld, which is too late for the deletion, so
	 * the claim is taken here: under r_smp the render thread is parked for both
	 * the glDeleteTextures and the upload that follows. */
	R_IssuePendingRenderCommands();

	strncpy(imagename, name, ARRAY_LEN(imagename));
	hash = generateHashValue(name);
	image = hashTable[hash];

    if (image)
    {
        while (strcmp(name, image->imgName))
        {
            image = image->next;
			if (!image) {
				return R_FindImageFileOld(
					imagename,
					mipmap,
					allowPicmip,
					force32bit,
					textureMode
				);
			}
        }

        R_FreeImage(image);
    }

    return R_FindImageFileOld(
		imagename,
		mipmap,
		allowPicmip,
		force32bit,
		textureMode
	);
}

qboolean R_ImageExists(const char* name) {
    image_t* image;
    long int hash;

    if (name)
    {
        hash = generateHashValue(name);
        image = hashTable[hash];

        if (hash)
        {
            for (; image != NULL; image = image->next)
            {
                if (!strcmp(name, image->imgName)) {
                    return qtrue;
                }
            }
        }
    }

    return qfalse;
}

#define	DLIGHT_SIZE	32
static void R_CreateDlightImage( void ) {
	int		x,y;
	byte	data[DLIGHT_SIZE][DLIGHT_SIZE][4];
	int		b;

	for (x=0 ; x<DLIGHT_SIZE ; x++) {
		for (y=0 ; y<DLIGHT_SIZE ; y++) {
			float	d;

			d = ( DLIGHT_SIZE/2 - 0.5f - x ) * ( DLIGHT_SIZE/2 - 0.5f - x ) +
				( DLIGHT_SIZE/2 - 0.5f - y ) * ( DLIGHT_SIZE/2 - 0.5f - y );
			b = 16000 / d;
			if (b > 255) {
				b = 255;
			} else if ( b < 75 ) {
				b = 0;
			}
			data[y][x][0] = 
			data[y][x][1] = 
			data[y][x][2] = b;
			data[y][x][3] = 255;			
		}
	}
	tr.dlightImage = R_CreateImageOld("*dlight", (byte *)data, DLIGHT_SIZE, DLIGHT_SIZE, 0, 1, qfalse, qfalse, qfalse, qfalse, 0);
	tr.dlightImage->r_sequence = -1;
}

#define	DEFAULT_SIZE	16
static void R_CreateDefaultImage( void ) {
	int		x;
	byte	data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	Com_Memset( data, 32, sizeof( data ) );
	for ( x = 0 ; x < DEFAULT_SIZE ; x++ ) {
		data[0][x][0] =
		data[0][x][1] =
		data[0][x][2] =
		data[0][x][3] = 255;

		data[x][0][0] =
		data[x][0][1] =
		data[x][0][2] =
		data[x][0][3] = 255;

		data[DEFAULT_SIZE-1][x][0] =
		data[DEFAULT_SIZE-1][x][1] =
		data[DEFAULT_SIZE-1][x][2] =
		data[DEFAULT_SIZE-1][x][3] = 255;

		data[x][DEFAULT_SIZE-1][0] =
		data[x][DEFAULT_SIZE-1][1] =
		data[x][DEFAULT_SIZE-1][2] =
		data[x][DEFAULT_SIZE-1][3] = 255;
	}
	tr.defaultImage = R_CreateImageOld("*default", (byte *)data, DEFAULT_SIZE, DEFAULT_SIZE, 1, 1, qfalse, qfalse, qfalse, qfalse, 2);
	tr.defaultImage->r_sequence = -1;
}

void R_CreateBuiltinImages(void) {
	int		x, y;
	byte	data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	R_CreateDefaultImage();

	Com_Memset( data, 255, sizeof( data ) );
	tr.whiteImage = R_CreateImageOld("*white", (byte *)data, 8, 8, 0, 1, qfalse, qfalse, qfalse, qfalse, 2);
	tr.whiteImage->r_sequence = -1;

	for (x=0 ; x<DEFAULT_SIZE ; x++) {
		for (y=0 ; y<DEFAULT_SIZE ; y++) {
			data[y][x][0] = 
			data[y][x][1] = 
			data[y][x][2] = tr.identityLightByte;
			data[y][x][3] = 255;			
		}
	}


	tr.identityLightImage = R_CreateImageOld("*identityLight", (byte *)data, 8, 8, 0, 1, qfalse, qfalse, qfalse, qfalse, 2);
	tr.identityLightImage->r_sequence = -1;

	Com_Memset(data, 0, sizeof(data));
	tr.scratchImage = R_CreateImageOld("*scratch", (byte*)data, DEFAULT_SIZE, DEFAULT_SIZE, 0, 1, r_picmip->integer, qfalse, qfalse, qfalse, 0);
	tr.scratchImage->r_sequence = -1;

	R_CreateDlightImage();
}


void R_SetColorMappings( void ) {
	int		i, j;
	float	g;
	int		inf;
	int		shift;

	tr.overbrightBits = r_overBrightBits->integer;
	if ( !glConfig.deviceSupportsGamma ) {
		tr.overbrightBits = 0;		
	}

	if ( !glConfig.isFullscreen ) 
	{
		tr.overbrightBits = 0;
	}

	if ( glConfig.colorBits > 16 ) {
		if ( tr.overbrightBits > 2 ) {
			tr.overbrightBits = 2;
		}
	} else {
		if ( tr.overbrightBits > 1 ) {
			tr.overbrightBits = 1;
		}
	}
	if ( tr.overbrightBits < 0 ) {
		tr.overbrightBits = 0;
	}

	tr.overbrightShift = r_mapOverBrightBits->integer - tr.overbrightBits;
	tr.overbrightMult = (float)(1 << tr.overbrightShift);
	tr.identityLight = 1.0f / (1 << tr.overbrightBits);
	tr.identityLightByte = 255 * tr.identityLight;

	if ( r_intensity->value <= 1 ) {
		ri.Cvar_Set( "r_intensity", "1" );
	}

	if ( r_gamma->value < 0.5f ) {
		ri.Cvar_Set( "r_gamma", "0.5" );
	} else if ( r_gamma->value > 3.0f ) {
		ri.Cvar_Set( "r_gamma", "3.0" );
	}

	g = r_gamma->value;
	if (!glConfig.deviceSupportsGamma && g != 1.0) {
		tr.needsLightScale = qtrue;
	}
	else if (r_intensity->value != 1.0) {
		tr.needsLightScale = qtrue;
	}
	else {
		tr.needsLightScale = qfalse;
	}

	shift = tr.overbrightBits;

	for ( i = 0; i < 256; i++ ) {
		if ( g == 1 ) {
			inf = i;
		} else {
			inf = 255 * pow ( i/255.0f, 1.0f / g ) + 0.5f;
		}
		inf <<= shift;
		if (inf < 0) {
			inf = 0;
		}
		if (inf > 255) {
			inf = 255;
		}
		s_gammatable[i] = inf;
	}

	for (i=0 ; i<256 ; i++) {
		j = i * r_intensity->value;
		if (j > 255) {
			j = 255;
		}
		s_intensitytable[i] = j;
	}

	if ( glConfig.deviceSupportsGamma )
	{
		GLimp_SetGamma( s_gammatable, s_gammatable, s_gammatable );
	}
}

void	R_InitImages( void ) {
	SMP_RLOAD_BEGIN( imginit );

	Com_Memset(hashTable, 0, sizeof(hashTable));
	Com_Memset(tr.images, 0, sizeof(tr.images));
	tr.numImages = 0;

	smp_texCache = ri.Cvar_Get("smp_texCache", "1", CVAR_ARCHIVE);
	smp_texCacheMaxMB = ri.Cvar_Get("smp_texCacheMaxMB", "96", CVAR_ARCHIVE);
	{
		int mb = smp_texCacheMaxMB->integer;

		if (mb < 1) {
			mb = SMP_TEXCACHE_DEFAULT_MAX_MB;
		}
		if (mb > 2048) {
			mb = 2048;
		}
		SMP_TexCache_InitEngine(smp_texCache->integer ? 1 : 0, mb * 1024 * 1024);
	}

	R_SetColorMappings();

	R_CreateBuiltinImages();

	SMP_RLOAD_END( imginit );
}

void R_DeleteImageFromHash(image_t* image) {
	image_t* current;
	image_t* prev;
	int hash;

    hash = generateHashValue(image->imgName);
    current = hashTable[hash];
    prev = NULL;

    while (current)
    {
        if (image == current)
        {
			if (prev) {
				prev->next = current->next;
			} else {
                hashTable[hash] = current->next;
			}
            return;
        }

        prev = current;
        current = current->next;
    }
}

void R_FreeImage(image_t* image) {
	if (image->texnum) {
		GL_ClaimContextForResource();
		qglDeleteTextures(1, &image->texnum);
	}

    R_DeleteImageFromHash(image);
	memset(image, 0, sizeof(image_t));
}

void R_DeleteTextures( void ) {
	int		i;

	for ( i=0; i<tr.numImages ; i++ ) {
        R_FreeImage(&tr.images[i]);
	}
	Com_Memset( tr.images, 0, sizeof( tr.images ) );

	tr.numImages = 0;

	Com_Memset( glState.currenttextures, 0, sizeof( glState.currenttextures ) );
	if ( qglBindTexture ) {
		if ( qglActiveTextureARB ) {
			GL_SelectTexture( 1 );
			qglBindTexture( GL_TEXTURE_2D, 0 );
			GL_SelectTexture( 0 );
			qglBindTexture( GL_TEXTURE_2D, 0 );
		} else {
			qglBindTexture( GL_TEXTURE_2D, 0 );
		}
	}
}

void R_DumpTextureMemory(void) {
    int i;
    char str_buffer[32];
    char* Label1, * Label2;
    fileHandle_t stat_file;

    stat_file = ri.FS_OpenFileWrite("textureuse.csv");

    Label1 = "Texture Name,";
    ri.FS_Write(Label1, strlen(Label1), stat_file);
    Label2 = "Size (kb),";
    ri.FS_Write(Label2, strlen(Label2), stat_file);
    Label1 = "Num Uses,";
    ri.FS_Write(Label1, strlen(Label1), stat_file);
    Label2 = "Miplevels\n";
    ri.FS_Write(Label2, strlen(Label2), stat_file);

    for (i = 0; i < tr.numImages; i++) {
        image_t* image = &tr.images[i];

        ri.FS_Write(image->imgName, strlen(image->imgName), stat_file);
        ri.FS_Write(", ", 2, stat_file);

        Q_snprintf(str_buffer, sizeof(str_buffer), "%d", image->bytesUsed >> 10);
        ri.FS_Write(str_buffer, strlen(str_buffer), stat_file);
        ri.FS_Write(", ", 2, stat_file);

        Q_snprintf(str_buffer, sizeof(str_buffer), "%d", image->r_sequence);
        ri.FS_Write(str_buffer, strlen(str_buffer), stat_file);
        ri.FS_Write(", ", 2, stat_file);
    }
}

int R_CountTextureMemory(void) {
    int total_bytes;
    int i;

    R_DumpTextureMemory();

    total_bytes = 0;

    for (i = 0; i < tr.numImages; i++) {
        total_bytes = tr.images[i].bytesUsed;
    }

    return total_bytes;
}

void R_FreeUnusedImages(void) {
    int i;

    for (i = 0; i < tr.numImages; i++) {
        image_t* image = &tr.images[i];

        if (image->imgName[0] && image->r_sequence != r_sequencenumber && image->r_sequence != -1) {
            R_FreeImage(image);
        }
    }
}


static char *CommaParse( char **data_p ) {
	int c = 0, len;
	char *data;
	static	char	com_token[MAX_TOKEN_CHARS];

	data = *data_p;
	len = 0;
	com_token[0] = 0;

	if ( !data ) {
		*data_p = NULL;
		return com_token;
	}

	while ( 1 ) {
		while( (c = *data) <= ' ') {
			if( !c ) {
				break;
			}
			data++;
		}


		c = *data;

		if ( c == '/' && data[1] == '/' )
		{
			while (*data && *data != '\n')
				data++;
		}
		else if ( c=='/' && data[1] == '*' ) 
		{
			while ( *data && ( *data != '*' || data[1] != '/' ) ) 
			{
				data++;
			}
			if ( *data ) 
			{
				data += 2;
			}
		}
		else
		{
			break;
		}
	}

	if ( c == 0 ) {
		return "";
	}

	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				*data_p = ( char * ) data;
				return com_token;
			}
			if (len < MAX_TOKEN_CHARS)
			{
				com_token[len] = c;
				len++;
			}
		}
	}

	do
	{
		if (len < MAX_TOKEN_CHARS)
		{
			com_token[len] = c;
			len++;
		}
		data++;
		c = *data;
	} while (c>32 && c != ',' );

	if (len == MAX_TOKEN_CHARS)
	{
		len = 0;
	}
	com_token[len] = 0;

	*data_p = ( char * ) data;
	return com_token;
}


qhandle_t RE_RegisterSkin( const char *name ) {
	qhandle_t	hSkin;
	skin_t		*skin;
	skinSurface_t	*surf;
	char		*text, *text_p;
	char		*token;
	char		surfName[MAX_QPATH];

	if ( !name || !name[0] ) {
		ri.Printf(PRINT_ALL,  "Empty name passed to RE_RegisterSkin\n" );
		return 0;
	}

	if ( strlen( name ) >= MAX_QPATH ) {
		ri.Printf(PRINT_ALL,  "Skin name exceeds MAX_QPATH\n" );
		return 0;
	}


	for ( hSkin = 1; hSkin < tr.numSkins ; hSkin++ ) {
		skin = tr.skins[hSkin];
		if ( !Q_stricmp( skin->name, name ) ) {
			if( skin->numSurfaces == 0 ) {
				return 0;		
			}
			return hSkin;
		}
	}

	if ( tr.numSkins == MAX_SKINS ) {
		ri.Printf( PRINT_WARNING, "WARNING: RE_RegisterSkin( '%s' ) MAX_SKINS hit\n", name );
		return 0;
	}
	tr.numSkins++;
	skin = ri.Hunk_Alloc( sizeof( skin_t ), h_dontcare );
	tr.skins[hSkin] = skin;
	Q_strncpyz( skin->name, name, sizeof( skin->name ) );
	skin->numSurfaces = 0;

	R_IssuePendingRenderCommands();

	if ( strcmp( name + strlen( name ) - 5, ".skin" ) ) {
		skin->numSurfaces = 1;
		skin->surfaces[0] = ri.Hunk_Alloc( sizeof(skin->surfaces[0]), h_dontcare);
		skin->surfaces[0]->shader = R_FindShader( name, LIGHTMAP_NONE, qtrue, qfalse, qfalse, qfalse );
		return hSkin;
	}

    ri.FS_ReadFile( name, (void **)&text );
	if ( !text ) {
		return 0;
	}

	text_p = text;
	while ( text_p && *text_p ) {
		token = CommaParse( &text_p );
		Q_strncpyz( surfName, token, sizeof( surfName ) );

		if ( !token[0] ) {
			break;
		}
		Q_strlwr( surfName );

		if ( *text_p == ',' ) {
			text_p++;
		}

		if ( strstr( token, "tag_" ) ) {
			continue;
		}
		
		token = CommaParse( &text_p );

		surf = skin->surfaces[ skin->numSurfaces ] = ri.Hunk_Alloc( sizeof( *skin->surfaces[0] ), h_dontcare );
		Q_strncpyz( surf->name, surfName, sizeof( surf->name ) );
		surf->shader = R_FindShader( token, LIGHTMAP_NONE, qtrue, qfalse, qfalse, qfalse );
		skin->numSurfaces++;
	}

	ri.FS_FreeFile( text );


	if ( skin->numSurfaces == 0 ) {
		return 0;		
	}

	return hSkin;
}

skin_t	*R_GetSkinByHandle( qhandle_t hSkin ) {
	if ( hSkin < 1 || hSkin >= tr.numSkins ) {
		return tr.skins[0];
	}
	return tr.skins[ hSkin ];
}

void	R_SkinList_f( void ) {
	int			i, j;
	skin_t		*skin;

	ri.Printf (PRINT_ALL, "------------------\n");

	for ( i = 0 ; i < tr.numSkins ; i++ ) {
		skin = tr.skins[i];

		ri.Printf( PRINT_ALL, "%3i:%s\n", i, skin->name );
		for ( j = 0 ; j < skin->numSurfaces ; j++ ) {
			ri.Printf( PRINT_ALL, "       %s = %s\n", 
				skin->surfaces[j]->name, skin->surfaces[j]->shader->name );
		}
	}
	ri.Printf (PRINT_ALL, "------------------\n");
}

qboolean R_LoadRawImage(const char *name, byte **pic, int *width, int *height)
{
    qboolean hasAlpha;
    int glCompressMode;
    int numMipmaps = 0;
    int piMipmapsAvailable;

    R_LoadImage(name, pic, width, height, &hasAlpha, &glCompressMode, &numMipmaps, &piMipmapsAvailable);

    if (!*pic) {
        return qfalse;
    }

    return qtrue;
}

void R_FreeRawImage(byte *pic) {
    ri.Free(pic);
}


void GetColorBlockColors( byte *data, byte *c0, byte *c1, byte *c2, byte *c3, unsigned short *bits )
{
	int		i, j;
	unsigned short	color;

	c0[3] = 255;
	color = *(unsigned short *)data;
	c0[0] = ( data[0] & 0xF8 );
	c0[1] = ( color >> 3 ) & 0xFC;
	c0[2] = ( data[1] ) * 8;

	c1[3] = 255;
	color = *(unsigned short *)( data + 2 );
	c1[0] = ( data[2] & 0xF8 );
	c1[1] = ( color >> 3 ) & 0xFC;
	c1[2] = ( data[3] ) * 8;

	if ( *(unsigned short *)data <= *(unsigned short *)( data + 2 ) ) {
		for ( i = 0; i < 3; i++ ) {
			j = ( c0[i] + c1[i] ) / 2;
			*bits = j;
			c2[i] = j;
		}
		c2[3] = 255;
		*(unsigned short *)c3 = 255;
		c3[2] = 255;
		c3[3] = 0;
	} else {
		for ( i = 0; i < 3; i++ ) {
			j = ( c1[i] + 2 * c0[i] ) / 3;
			*bits = j;
			c2[i] = j;
		}
		c2[3] = 255;
		for ( i = 0; i < 2; i++ ) {
			j = ( c0[i] + 2 * c1[i] ) / 3;
			*bits = j;
			c3[i] = j;
		}
		j = ( c0[2] + 2 * c1[2] ) / 3;
		*bits = j;
		c3[2] = j;
		c3[3] = 255;
	}
}

void DecodeColorBlock( unsigned int *dst, byte *block, int width, unsigned int *c0, unsigned int *c1, unsigned int *c2, unsigned int *c3 )
{
	int		i, j;
	unsigned int	colors[4];
	unsigned int	*dp;

	colors[0] = *c0;
	colors[1] = *c1;
	colors[2] = *c2;
	colors[3] = *c3;

	dp = dst;
	for ( i = 0; i < 4; i++ ) {
		byte	sel = block[4 + i];		

		for ( j = 0; j < 4; j++ ) {
			*dp++ = colors[( sel >> ( 2 * j ) ) & 3];
		}
		dp += width - 4;
	}
}

void decodeAlphaExplicit( unsigned int *dst, unsigned short *src, int stride, unsigned int mask )
{
	int		i, j;
	unsigned int	alphaBits;
	unsigned int	alpha;

	for ( i = 0; i < 4; i++ ) {
		alphaBits = *src++;
		for ( j = 0; j < 4; j++ ) {
			alpha = alphaBits & 0xF;
			alpha |= alpha << 4;
			*dst &= mask;
			*dst |= alpha;
			dst++;
			alphaBits >>= 4;
		}
		dst += stride - 4;
	}
}

int GetDDSOffset( int level, int levels, int width, int height, int blockBytes )
{
	int		offset = 0;
	int		i;

	if ( level >= levels ) {
		level = levels - 1;
	}

	for ( i = 0; i < level && ( width || height ); i++ ) {
		if ( !width ) {
			width = 1;
		}
		if ( !height ) {
			height = 1;
		}
		offset += blockBytes * ( ( width + 3 ) / 4 ) * ( ( height + 3 ) / 4 );
		width >>= 1;
		height >>= 1;
	}

	return offset;
}

typedef struct {
	const byte	*in;
	byte		*out;
	int			inWidth, inHeight, newWidth, newHeight;
} mipMap2_t;

static void R_MipMap2Rows( void *vctx, int begin, int end )
{
	const mipMap2_t	*c = (const mipMap2_t *)vctx;
	int			i, j, k;
	static const int	weights[4][4] = {
		{ 1, 2, 2, 1 },
		{ 2, 4, 4, 2 },
		{ 2, 4, 4, 2 },
		{ 1, 2, 2, 1 }
	};
	const int	inWidth = c->inWidth, inHeight = c->inHeight;
	const int	newWidth = c->newWidth;
	const byte *in = c->in;
	byte	   *outBase = c->out;
	int			outRowOfs = c->newWidth * begin;

	for ( i = begin; i < end; i++, outRowOfs += newWidth ) {
		int		rowOfs[4];
		int		colBase = -1;
		byte		*outRow = outBase + 4 * outRowOfs;

		rowOfs[0] = inWidth * ( ( inHeight - 1 ) & ( 2 * i - 1 ) );
		rowOfs[1] = inWidth * ( ( inHeight - 1 ) & ( 2 * i     ) );
		rowOfs[2] = inWidth * ( ( inHeight - 1 ) & ( 2 * i + 1 ) );
		rowOfs[3] = inWidth * ( ( inHeight - 1 ) & ( 2 * i + 2 ) );

		for ( j = 0; j < newWidth; j++, colBase += 2, outRow += 4 ) {
			int		col[4];
			const byte	*tap[4][4];

			col[0] = ( inWidth - 1 ) & colBase;
			col[1] = ( inWidth - 1 ) & ( colBase + 1 );
			col[2] = ( inWidth - 1 ) & ( colBase + 2 );
			col[3] = ( inWidth - 1 ) & ( colBase + 3 );

			tap[0][0] = in + 4 * ( col[0] + rowOfs[0] );
			tap[0][1] = in + 4 * ( col[1] + rowOfs[0] );
			tap[0][2] = in + 4 * ( col[2] + rowOfs[0] );
			tap[0][3] = in + 4 * ( col[3] + rowOfs[0] );

			tap[1][0] = in + 4 * ( col[0] + rowOfs[1] );
			tap[1][1] = in + 4 * ( col[1] + rowOfs[1] );
			tap[1][2] = in + 4 * ( col[2] + rowOfs[1] );
			tap[1][3] = in + 4 * ( col[3] + rowOfs[1] );

			tap[2][0] = in + 4 * ( col[0] + rowOfs[2] );
			tap[2][1] = in + 4 * ( col[1] + rowOfs[2] );
			tap[2][2] = in + 4 * ( col[2] + rowOfs[2] );
			tap[2][3] = in + 4 * ( col[3] + rowOfs[2] );

			tap[3][0] = in + 4 * ( col[0] + rowOfs[3] );
			tap[3][1] = in + 4 * ( col[1] + rowOfs[3] );
			tap[3][2] = in + 4 * ( col[2] + rowOfs[3] );
			tap[3][3] = in + 4 * ( col[3] + rowOfs[3] );

			for ( k = 0; k < 4; k++ ) {
				int	total = 0;
				int	a, b;

				for ( a = 0; a < 4; a++ ) {
					for ( b = 0; b < 4; b++ ) {
						total += weights[a][b] * tap[a][b][k];
					}
				}
				outRow[k] = total / 36;
			}
		}
	}
}

void R_MipMap2( byte *in, int inWidth, int inHeight )
{
	int			newWidth, newHeight;
	byte			*out;
	mipMap2_t		c;

	newWidth = inWidth >> 1;
	newHeight = inHeight >> 1;

	out = ri.Hunk_AllocateTempMemory( 4 * newWidth * newHeight );

	c.in = in;
	c.out = out;
	c.inWidth = inWidth;
	c.inHeight = inHeight;
	c.newWidth = newWidth;
	c.newHeight = newHeight;

	JobPool_ParallelRange( newHeight, 16, R_MipMap2Rows, &c );

	memcpy( in, out, 4 * newWidth * newHeight );
	ri.Hunk_FreeTempMemory( out );
}

void verifyProperTextureSize( const char *name, int width, int height )
{
	int	i, j;

	for ( i = 1; i < width; i *= 2 )
		;
	for ( j = 1; j < height; j *= 2 )
		;

	if ( i > width || j > height ) {
		ri.Printf( PRINT_WARNING, "WARNING: texture %s being rescaled because of bad size (%dx%d)\n",
				   name, width, height );
	}
}

int getGLClampType( int wrapAxis, int textureMode )
{
	if ( textureMode == 2 ) {
		return GL_REPEAT;
	}
	if ( textureMode < 2 ) {
		if ( textureMode == 0 ) {
			return GL_CLAMP;
		}
		if ( textureMode >= 0 ) {
			return GL_CLAMP_TO_EDGE;
		}
		ri.Printf( PRINT_WARNING, "Bad clamp type\n" );
		return GL_REPEAT;
	}
	if ( textureMode == 4 ) {
		if ( wrapAxis == GL_TEXTURE_WRAP_T ) {
			return GL_CLAMP;
		}
		return GL_REPEAT;
	}
	if ( textureMode >= 4 ) {
		ri.Printf( PRINT_WARNING, "Bad clamp type\n" );
		return GL_REPEAT;
	}
	if ( wrapAxis == GL_TEXTURE_WRAP_S ) {
		return GL_CLAMP;
	}
	return GL_REPEAT;
}

void GL_SetTextureAnisotropicFiltering( GLenum target )
{
	if ( textureFilterAnisotropic ) {
		qglTexParameterf( target, GL_TEXTURE_MAX_ANISOTROPY_EXT, (GLfloat)maxAnisotropy );
	}
}

void GL_SetupAnisotropicFiltering( void )
{
	int		i;

	if ( !textureFilterAnisotropic ) {
		return;
	}

	for ( i = 0; i < tr.numImages; i++ ) {
		if ( tr.images[i].texnum ) {
			GL_Bind( &tr.images[i] );
			qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, (GLfloat)maxAnisotropy );
		}
	}
}
