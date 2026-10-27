/*
===========================================================================
This file is part of the port of Star Trek: Voyager -- Elite Force II, whose
engine descends from id Tech 3 through OpenMoHAA.

It is free software; you can redistribute it and/or modify it under the terms of
the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU General Public License, COPYING.txt at the repository
root, for more details.
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later
// Modified 2026-10-15 for this port; the changes are listed in PROVENANCE.tsv.

#include "tr_local.h"
#include "vk_local.h"
#include "tr_program.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- the save-game screenshot ------------------------------------------- */
/* SV_Savegame writes the thumbnail by executing `saveshot <file> 256 256' (sv_ccmds.c), which forwards to `screenshot'; the client holds the
   request until it is drawing the suppressed world frame, and the read runs between that frame's submit and its present -- the one window in
   which the image still belongs to us and holds exactly what was drawn. */

#define VK_SHOT_MAXNAME	256

static char		s_shotName[VK_SHOT_MAXNAME];
static int		s_shotWidth, s_shotHeight;
static qboolean	s_shotPending;

void R_Screenshot_Request( const char *name, int width, int height )
{
	if ( !name || !name[0] ) {
		return;
	}
	Q_strncpyz( s_shotName, name, sizeof( s_shotName ) );
	s_shotWidth  = width;
	s_shotHeight = height;
	s_shotPending = qtrue;
}

qboolean R_Screenshot_Pending( void )
{
	return s_shotPending;
}

/* GL's read contract: RGB, three bytes a pixel, row 0 the window's bottom row.
   Every screenshot writer in the reference renderer is written against that
   shape, so this is where the swapchain's top-down RGBA is turned. */
byte *RB_ReadPixels( uint32_t imageIndex, int x, int y, int width, int height, size_t *offset, int *padlen )
{
	unsigned char	*rgba, *out, *dst;
	size_t			rgbaBytes, srcRow;
	int				padWidth, linelen, row, col;

	linelen  = width * 3;
	padWidth = PAD( linelen, 4 );

	*offset = 0;
	*padlen = padWidth - linelen;

	out = ri.Hunk_AllocateTempMemory( (size_t)padWidth * height + *offset + 4 );
	memset( out, 0, (size_t)padWidth * height + *offset + 4 );

	if ( vk.swapchain == VK_NULL_HANDLE || !vk.swapchainExtent.width
		 || !vk.swapchainExtent.height ) {
		return out;
	}

	rgbaBytes = (size_t)vk.swapchainExtent.width * vk.swapchainExtent.height * 4;
	rgba = ri.Hunk_AllocateTempMemory( rgbaBytes );
	if ( !VK_Swapchain_ReadPixels( imageIndex, rgba, rgbaBytes ) ) {
		ri.Hunk_FreeTempMemory( rgba );
		return out;
	}

	srcRow = (size_t)vk.swapchainExtent.width * 4;

	for ( row = 0; row < height; row++ ) {
		/* the swapchain copy runs top down, the caller's rows bottom up */
		int	sy = vk.swapchainExtent.height - 1 - ( y + row );
		const unsigned char *src;
		int	sx;

		if ( sy < 0 || sy >= (int)vk.swapchainExtent.height ) {
			continue;
		}
		src = rgba + (size_t)sy * srcRow;

		dst = out + *offset + (size_t)row * padWidth;
		for ( col = 0; col < width; col++ ) {
			sx = x + col;
			if ( sx < 0 || sx >= (int)vk.swapchainExtent.width ) {
				dst[0] = dst[1] = dst[2] = 0;
			} else {
				const unsigned char *p = src + (size_t)sx * 4;
				dst[0] = p[0];
				dst[1] = p[1];
				dst[2] = p[2];
			}
			dst += 3;
		}
	}

	ri.Hunk_FreeTempMemory( rgba );
	return out;
}

/* A four-by-three box filter over the window, exactly the reference
   resampler's shape: twelve samples a destination pixel, BGR out. */
static void VK_Shot_Resample( const char *filename, int destWidth, int destHeight, uint32_t imageIndex )
{
	byte	*buffer, *source, *allsource, *src, *dst;
	size_t	offset = 0;
	int		padlen;
	int		x, y, r, g, b, xx, yy;
	float	xScale, yScale;
	int		srcW, srcH;

	srcW = vkConfig.vidWidth;
	srcH = vkConfig.vidHeight;

	allsource = RB_ReadPixels( imageIndex, 0, 0, srcW, srcH, &offset, &padlen );
	source = allsource + offset;

	buffer = ri.Hunk_AllocateTempMemory( destHeight * destWidth * 3 + 18 );
	memset( buffer, 0, 18 );
	buffer[2]  = 2;
	buffer[12] = destWidth & 255;
	buffer[13] = destWidth >> 8;
	buffer[14] = destHeight & 255;
	buffer[15] = destHeight >> 8;
	buffer[16] = 24;

	xScale = srcW / (float)( destWidth * 4 );
	yScale = srcH / (float)( destHeight * 3 );
	for ( y = 0 ; y < destHeight ; y++ ) {
		for ( x = 0 ; x < destWidth ; x++ ) {
			r = g = b = 0;
			for ( yy = 0 ; yy < 3 ; yy++ ) {
				for ( xx = 0 ; xx < 4 ; xx++ ) {
					src = source + ( 3 * srcW + padlen ) * (int)( ( y * 3 + yy ) * yScale )
						+ 3 * (int)( ( x * 4 + xx ) * xScale );
					r += src[0];
					g += src[1];
					b += src[2];
				}
			}
			dst = buffer + 18 + 3 * ( y * destWidth + x );
			dst[0] = b / 12;
			dst[1] = g / 12;
			dst[2] = r / 12;
		}
	}

	ri.FS_WriteFile( filename, buffer, destHeight * destWidth * 3 + 18 );

	ri.Hunk_FreeTempMemory( buffer );
	ri.Hunk_FreeTempMemory( allsource );
}

static void VK_Shot_Full( const char *filename, int width, int height, uint32_t imageIndex )
{
	byte	*allbuf, *buffer, *srcptr, *destptr, *endline, *endmem, *endrow;
	byte	temp;
	size_t	offset = 18, memcount;
	int		linelen, padlen;

	allbuf = RB_ReadPixels( imageIndex, 0, 0, width, height, &offset, &padlen );
	buffer = allbuf + offset - 18;

	memset( buffer, 0, 18 );
	buffer[2]  = 2;
	buffer[12] = width & 255;
	buffer[13] = width >> 8;
	buffer[14] = height & 255;
	buffer[15] = height >> 8;
	buffer[16] = 24;

	linelen = width * 3;

	srcptr = destptr = allbuf + offset;
	endmem = srcptr + ( linelen + padlen ) * height;

	while ( srcptr < endmem ) {
		endline = srcptr + linelen;
		endrow  = endline + padlen;
		while ( srcptr < endline ) {
			temp = srcptr[0];
			*destptr++ = srcptr[2];
			*destptr++ = srcptr[1];
			*destptr++ = temp;
			srcptr += 3;
		}
		srcptr = endrow;
	}

	memcount = linelen * height;
	ri.FS_WriteFile( filename, buffer, memcount + 18 );

	ri.Hunk_FreeTempMemory( allbuf );
}

void R_Screenshot_Flush( uint32_t imageIndex )
{
	if ( !s_shotPending ) {
		return;
	}
	s_shotPending = qfalse;

	if ( vk.swapchain == VK_NULL_HANDLE || !vk.swapchainExtent.width
		 || !vk.swapchainExtent.height ) {
		return;
	}
	if ( imageIndex >= vk.swapchainImageCount ) {
		return;
	}

	/* one read of the one image, handed to the writer: a second read here was
	   answering the request twice, and the second answer was whatever the
	   presentation engine had done with the image in between */
	if ( s_shotWidth > 0 && s_shotHeight > 0
		 && ( s_shotWidth != vkConfig.vidWidth || s_shotHeight != vkConfig.vidHeight ) ) {
		VK_Shot_Resample( s_shotName, s_shotWidth, s_shotHeight, imageIndex );
	} else {
		VK_Shot_Full( s_shotName, vkConfig.vidWidth, vkConfig.vidHeight, imageIndex );
	}
}

void R_ScreenShot_f( void )
{
	char	checkname[MAX_OSPATH];
	int		width, height;
	qboolean silent;

	if ( ri.Cmd_Argc() < 2 ) {
		return;
	}

	silent = qfalse;
	if ( !strcmp( ri.Cmd_Argv( 1 ), "silent" ) ) {
		silent = qtrue;
	}

	width = 0;
	height = 0;

	if ( ri.Cmd_Argc() >= 2 && !silent ) {
		const char *name = ri.Cmd_Argv( 1 );

		if ( strchr( name, '/' ) ) {
			Q_strncpyz( checkname, name, sizeof( checkname ) );
		} else {
			Com_sprintf( checkname, MAX_OSPATH, "screenshots/%s.tga", name );
		}

		if ( ri.Cmd_Argc() > 3 ) {
			width  = atoi( ri.Cmd_Argv( 2 ) );
			height = atoi( ri.Cmd_Argv( 3 ) );
		}
	} else {
		int i;

		for ( i = 0; i <= 9999; i++ ) {
			Com_sprintf( checkname, MAX_OSPATH, "screenshots/shot%04i.tga", i );
			if ( !ri.FS_FileExists( checkname ) ) {
				break;
			}
		}
	}

	if ( !width || !height ) {
		R_Screenshot_Request( checkname, 0, 0 );
	} else {
		R_Screenshot_Request( checkname, width, height );
	}

	if ( !silent ) {
		char message[64];

		Com_sprintf( message, sizeof( message ), "centerprint \"Wrote %s\"\n", checkname );
		ri.Cmd_ExecuteText( EXEC_NOW, message );
	}
}

/* The renderer's console commands.  This lane defines the list commands but
   registered none of them, so `screenshot` -- and with it the save-game
   thumbnail -- silently did nothing. */
void R_Register( void )
{
	ri.Cmd_AddCommand( "totaltexturememinfo", R_TotalTextureMemInfo_f );
	ri.Cmd_AddCommand( "imagelist", R_ImageList_f );
	ri.Cmd_AddCommand( "shaderlist", R_ShaderList_f );
	ri.Cmd_AddCommand( "skinlist", R_SkinList_f );
	ri.Cmd_AddCommand( "modellist", R_Modellist_f );
	ri.Cmd_AddCommand( "screenshot", R_ScreenShot_f );
}

/* A renderer .so is unloaded and reloaded on every restart, and a command
   entry that outlives it holds a pointer into the dead mapping.  The GL
   twin removes its own commands in RE_Shutdown; this one does the same. */
void R_Unregister( void )
{
	ri.Cmd_RemoveCommand( "totaltexturememinfo" );
	ri.Cmd_RemoveCommand( "imagelist" );
	ri.Cmd_RemoveCommand( "shaderlist" );
	ri.Cmd_RemoveCommand( "skinlist" );
	ri.Cmd_RemoveCommand( "modellist" );
	ri.Cmd_RemoveCommand( "screenshot" );
}
