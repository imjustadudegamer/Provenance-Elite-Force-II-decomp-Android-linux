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

#include "tr_local.h"
#include "gles3_g1_api.h"

vec4_t r_colorWhite = { 1.0, 1.0, 1.0, 1.0 };

/* Front-end copy of backEnd.color2D, so an entry point reads the colour it
 * recorded without touching render-thread state. */
static byte r_color2D[4];

/* Recorded window/scissor coalescing.  A duplicate is skipped only while both
 * caches are valid; R_Invalidate2DWindowCache() drops them from every recorded
 * command that can change the viewport or the scissor box. */
static qboolean r_window2DValid;
static int      r_window2D[4];
static float    r_window2DOrtho[6];
static qboolean r_scissor2DValid;
static int      r_scissor2D[4];

void R_Invalidate2DWindowCache( void ) {
	r_window2DValid = qfalse;
	r_scissor2DValid = qfalse;
}

/* ---- the 2D bodies: run directly when the back end is single-threaded, and
 * replayed on the render thread when the same call was recorded ---- */

void RB_2D_SetColor( const byte *rgba ) {
	backEnd.color2D[0] = rgba[0];
	backEnd.color2D[1] = rgba[1];
	backEnd.color2D[2] = rgba[2];
	backEnd.color2D[3] = rgba[3];
	qglColor4ubv( backEnd.color2D );
}

void RB_2D_StretchPic( shader_t *shader, float x, float y, float w, float h, float s1, float t1, float s2, float t2 ) {
	RB_Color4f(backEnd.color2D[0], backEnd.color2D[1], backEnd.color2D[2], backEnd.color2D[3]);
	RB_BeginSurface(shader, 0);

	RB_Texcoord2f(s1, t1);
	RB_Vertex2f(x, y);

	RB_Texcoord2f(s2, t1);
	RB_Vertex2f(x + w, y);

	RB_Texcoord2f(s1, t2);
	RB_Vertex2f(x, y + h);

	RB_Texcoord2f(s2, t2);
	RB_Vertex2f(x + w, y + h);

	RB_StreamEnd();
}

void RB_2D_StretchPic2( shader_t *shader, float x, float y, float w, float h, float s1, float t1, float s2, float t2, float sx, float sy ) {
	float halfWidth, halfHeight;
	float scaledWidth1, scaledHeight1;
	float scaledWidth2, scaledHeight2;

	halfWidth = w * 0.5f;
	halfHeight = h * 0.5f;
	scaledWidth1 = halfWidth * sy;
	scaledHeight1 = halfHeight * sx;
	scaledWidth2 = halfWidth * sx;
	scaledHeight2 = halfHeight * sy;

	RB_Color4f(backEnd.color2D[0], backEnd.color2D[1], backEnd.color2D[2], backEnd.color2D[3]);
	RB_BeginSurface(shader, 0);

	RB_Texcoord2f(s1, t1);
	RB_Vertex3f(x + halfWidth - (scaledWidth2 + -scaledHeight2), y + halfWidth - scaledHeight1 - scaledWidth1, 0);

	RB_Texcoord2f(t2, t1);
	RB_Vertex3f(scaledWidth2 - -scaledHeight2 + x + halfWidth, scaledWidth1 - scaledHeight1 + y + halfWidth, 0);

	RB_Texcoord2f(s1, s2);
	RB_Vertex3f(x+ halfWidth - (scaledWidth2 + scaledHeight2), scaledHeight1 - scaledWidth1 + y + halfWidth, 0);

	RB_Texcoord2f(t2, s2);
	RB_Vertex3f(scaledWidth2 - scaledHeight2 + x + halfWidth, scaledWidth1 + scaledHeight1 + y + halfWidth, 0);

	RB_StreamEnd();
}

void RB_2D_TilePic( shader_t *shader, float x, float y, float w, float h, float picw, float pich ) {
	RB_Color4f(backEnd.color2D[0], backEnd.color2D[1], backEnd.color2D[2], backEnd.color2D[3]);

	RB_StreamBegin(shader);

	RB_Texcoord2f(x / picw, y / pich);
	RB_Vertex2f(x, y);

	RB_Texcoord2f((x + w) / picw, y / pich);
	RB_Vertex2f(x + w, y);

	RB_Texcoord2f(x / picw, (y + h) / pich);
	RB_Vertex2f(x, y + h);

	RB_Texcoord2f((x + w) / picw, (y + h) / pich);
	RB_Vertex2f(x + w, y + h);

	RB_StreamEnd();
}

void RB_2D_TilePicOffset( shader_t *shader, float x, float y, float w, float h, float picw, float pich, int offsetX, int offsetY ) {
	RB_Color4f(backEnd.color2D[0], backEnd.color2D[1], backEnd.color2D[2], backEnd.color2D[3]);

	RB_StreamBegin(shader);

	RB_Texcoord2f(x / picw, y / pich);
	RB_Vertex2f(x + offsetX, y + offsetY);

	RB_Texcoord2f((x + w) / picw, y / pich);
	RB_Vertex2f(x + offsetX + w, y + offsetY);

	RB_Texcoord2f(x / picw, (y + h) / pich);
	RB_Vertex2f(x + offsetX, y + offsetY + h);

	RB_Texcoord2f((x + w) / picw, (y + h) / pich);
	RB_Vertex2f(x + offsetX + w, y + offsetY + h);

	RB_StreamEnd();
}

void RB_2D_TrianglePic( shader_t *shader, const vec2_t vPoints[3], const vec2_t vTexCoords[3] ) {
	int i;

	RB_Color4f(backEnd.color2D[0], backEnd.color2D[1], backEnd.color2D[2], backEnd.color2D[3]);

	RB_BeginSurface(shader, 0);

	for (i = 0; i < 3; i++) {
		RB_Texcoord2f(vTexCoords[i][0], vTexCoords[i][1]);
		RB_Vertex2f(vPoints[i][0], vPoints[i][1]);
	}

	RB_StreamEnd();
}

void RB_2D_Box( float x, float y, float w, float h, qboolean blend ) {
	gles3Program_t	*p2d;

	if ( blend ) {
		GLES3_ApplyStateBits(GLS_DEPTHTEST_DISABLE | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_SRCBLEND_SRC_ALPHA);
	} else {
		GLES3_ApplyStateBits(GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE);
	}

	GL_BindUnit0( tr.whiteImage );
	p2d = GLES3_BeginFamily( GLP_FAMILY_2D, 1 );
	GLES3_SetBaseColor4f( p2d, backEnd.color2D[0] / 255.0f, backEnd.color2D[1] / 255.0f,
						  backEnd.color2D[2] / 255.0f, backEnd.color2D[3] / 255.0f );

	RB_ImmBegin( GL_TRIANGLES );
	RB_ImmVertex2f(x, y);
	RB_ImmVertex2f(x + w, y);
	RB_ImmVertex2f(x + w, y + h);
	RB_ImmVertex2f(x, y);
	RB_ImmVertex2f(x + w, y + h);
	RB_ImmVertex2f(x, y + h);
	RB_ImmEnd();
}

static void RB_DrawStippledLine2D( float x0, float y0, float x1, float y1,
								   int factor, unsigned short mask ) {
	float	dx = x1 - x0, dy = y1 - y0;
	float	len = sqrt( dx * dx + dy * dy );
	float	covered = 0.0f, t0 = 0.0f;
	int		bit = 0;

	if ( mask == 0xffff || len <= 0.0f ) {
		RB_ImmBegin( GL_LINES );
		RB_ImmVertex2f( x0, y0 );
		RB_ImmVertex2f( x1, y1 );
		RB_ImmEnd();
		return;
	}
	if ( factor < 1 ) {
		factor = 1;
	}
	while ( covered < len ) {
		float	seg = ( float )factor;
		float	t1;

		if ( covered + seg > len ) {
			seg = len - covered;
		}
		t1 = ( covered + seg ) / len;
		if ( ( mask >> bit ) & 1 ) {
			RB_ImmBegin( GL_LINES );
			RB_ImmVertex2f( x0 + dx * t0, y0 + dy * t0 );
			RB_ImmVertex2f( x0 + dx * t1, y0 + dy * t1 );
			RB_ImmEnd();
		}
		covered += seg;
		t0 = t1;
		bit = ( bit + 1 ) & 15;
	}
}

void RB_2D_LineLoop( const vec2_t *points, int count, int stipple_factor, int stipple_mask ) {
	int		i;

	GLES3_BeginWire();
	RB_ImmColor4f( backEnd.color2D[0] / 255.0f, backEnd.color2D[1] / 255.0f,
				   backEnd.color2D[2] / 255.0f, backEnd.color2D[3] / 255.0f );

	if (stipple_factor) {
		for ( i = 0; i < count; i++ ) {
			int	j = ( i + 1 ) % count;

			RB_DrawStippledLine2D( ( float )( int )points[i][0], ( float )( int )points[i][1],
								   ( float )( int )points[j][0], ( float )( int )points[j][1],
								   stipple_factor, ( unsigned short )stipple_mask );
		}
	} else {
		RB_ImmBegin( GL_LINE_LOOP );
		for ( i = 0; i < count; i++ ) {
			RB_ImmVertex2f( ( float )( int )points[i][0], ( float )( int )points[i][1] );
		}
		RB_ImmEnd();
	}
}

void RB_2D_SetWindow( int x, int y, int w, int h, float left, float right, float bottom, float top, float n, float f ) {
	qglViewport(x, y, w, h);
	qglScissor(x, y, w, h);
	SMP_VpProbe( "2d", "Set2DWindow/arg", x, y, w, h, 0, qtrue );
	RB_LoadIdentityProjection();
	RB_OrthoProjection(left, right, bottom, top, n, f);
	RB_LoadIdentityModelView();
	GLES3_ApplyStateBits(GLS_DEPTHTEST_DISABLE | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_SRCBLEND_SRC_ALPHA);
	GL_Cull( CT_TWO_SIDED );
	RB_ClipPlane_Disable();

	if (!backEnd.in2D)
	{
		backEnd.refdef.time = ri.Milliseconds();
		backEnd.in2D = qtrue;
		backEnd.refdef.floatTime = backEnd.refdef.time / 1000.0;
        backEnd.shaderStartTime = 0; 
	}
}

void RB_2D_Scissor( int x, int y, int width, int height ) {
	qglEnable(GL_SCISSOR_TEST);
	qglScissor(x, y, width, height);
	SMP_VpProbe( "scissor", "RE_Scissor/arg", x, y, width, height, 0, qfalse );
}

void RB_2D_ShaderTime( float startTime ) {
	if (backEnd.in2D)
	{
		backEnd.shaderStartTime = startTime;
	}
}

/* ---- entry points: record under r_smp, run the body otherwise ---- */

void Draw_SetColor(const vec4_t rgba) {
	if (!rgba) {
		rgba = r_colorWhite;
	}

	r_color2D[0] = (byte)(rgba[0] * tr.identityLightByte);
	r_color2D[1] = (byte)(rgba[1] * tr.identityLightByte);
	r_color2D[2] = (byte)(rgba[2] * tr.identityLightByte);
	r_color2D[3] = (byte)(rgba[3] * 255.0);

	if ( glConfig.smpActive ) {
		color2DCommand_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_COLOR;
			Com_Memcpy( cmd->color, r_color2D, sizeof( r_color2D ) );
		}

		/* The font path and RE_StretchRaw draw immediately on this thread
		 * after parking the render thread, and they read backEnd.color2D.
		 * Only while this thread owns the context may the front-end copy
		 * reach the render-thread state. */
		if ( GLimp_GLMainOwnsContext() ) {
			backEnd.color2D[0] = r_color2D[0];
			backEnd.color2D[1] = r_color2D[1];
			backEnd.color2D[2] = r_color2D[2];
			backEnd.color2D[3] = r_color2D[3];
		}
		return;
	}

	RB_2D_SetColor( r_color2D );
}

void R_ChangeTextColor(int index) {
	vec4_t color;

	if (index < 0 || index > 7) {
		return;
	}

	color[0] = g_color_table[index][0];
	color[1] = g_color_table[index][1];
	color[2] = g_color_table[index][2];
	color[3] = ( glConfig.smpActive ? r_color2D[3] : backEnd.color2D[3] ) / 255.0;
	Draw_SetColor(color);
}

void Draw_StretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader) {
	shader_t* shader;

	if ( !glConfig.smpActive ) {
		R_SyncRenderThread();
		R_IssuePendingRenderCommands();
	}

	if (hShader) {
		shader = R_GetShaderByHandle(hShader);
	}
	else {
		shader = tr.defaultShader;
	}

	if (w <= 0) {
		w = shader->unfoggedStages[0]->bundle[0].image[0]->width;
		h = shader->unfoggedStages[0]->bundle[0].image[0]->height;
	}

	if ( glConfig.smpActive ) {
		stretchPic2DCommand_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_STRETCH_PIC;
			cmd->shader = shader;
			cmd->x = x; cmd->y = y; cmd->w = w; cmd->h = h;
			cmd->s1 = s1; cmd->t1 = t1; cmd->s2 = s2; cmd->t2 = t2;
		}
		return;
	}

	RB_2D_StretchPic( shader, x, y, w, h, s1, t1, s2, t2 );
}

void Draw_StretchPic2(float x, float y, float w, float h, float s1, float t1, float s2, float t2, float sx, float sy, qhandle_t hShader) {
	shader_t* shader;

	if ( !glConfig.smpActive ) {
		R_SyncRenderThread();
		R_IssuePendingRenderCommands();
	}

	if (hShader) {
		shader = R_GetShaderByHandle(hShader);
	}
	else {
		shader = tr.defaultShader;
	}

	if (w <= 0) {
		w = shader->unfoggedStages[0]->bundle[0].image[0]->width;
		h = shader->unfoggedStages[0]->bundle[0].image[0]->height;
	}

	if ( glConfig.smpActive ) {
		stretchPic2Command_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_STRETCH_PIC2;
			cmd->shader = shader;
			cmd->x = x; cmd->y = y; cmd->w = w; cmd->h = h;
			cmd->s1 = s1; cmd->t1 = t1; cmd->s2 = s2; cmd->t2 = t2;
			cmd->sx = sx; cmd->sy = sy;
		}
		return;
	}

	RB_2D_StretchPic2( shader, x, y, w, h, s1, t1, s2, t2, sx, sy );
}

void Draw_TilePic(float x, float y, float w, float h, qhandle_t hShader) {
	shader_t* shader;
	float		picw, pich;

	if ( !glConfig.smpActive ) {
		R_SyncRenderThread();
		R_IssuePendingRenderCommands();
	}

	if (hShader) {
		shader = R_GetShaderByHandle(hShader);
	}
	else {
		shader = tr.defaultShader;
	}

	if (w <= 0) {
		w = shader->unfoggedStages[0]->bundle[0].image[0]->width;
		h = shader->unfoggedStages[0]->bundle[0].image[0]->height;
	}

	picw = shader->unfoggedStages[0]->bundle[0].image[0]->uploadWidth;
	pich = shader->unfoggedStages[0]->bundle[0].image[0]->uploadHeight;

	if ( glConfig.smpActive ) {
		tilePic2DCommand_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_TILE_PIC;
			cmd->shader = shader;
			cmd->x = x; cmd->y = y; cmd->w = w; cmd->h = h;
			cmd->picw = picw; cmd->pich = pich;
			cmd->offsetX = 0; cmd->offsetY = 0;
		}
		return;
	}

	RB_2D_TilePic( shader, x, y, w, h, picw, pich );
}

void Draw_TilePicOffset(float x, float y, float w, float h, qhandle_t hShader, int offsetX, int offsetY) {
	shader_t* shader;
	float		picw, pich;

	if ( !glConfig.smpActive ) {
		R_SyncRenderThread();
		R_IssuePendingRenderCommands();
	}

	if (hShader) {
		shader = R_GetShaderByHandle(hShader);
	}
	else {
		shader = tr.defaultShader;
	}

	if (w <= 0) {
		w = shader->unfoggedStages[0]->bundle[0].image[0]->width;
		h = shader->unfoggedStages[0]->bundle[0].image[0]->height;
	}

	picw = shader->unfoggedStages[0]->bundle[0].image[0]->uploadWidth;
	pich = shader->unfoggedStages[0]->bundle[0].image[0]->uploadHeight;

	if ( glConfig.smpActive ) {
		tilePic2DCommand_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_TILE_PIC_OFFSET;
			cmd->shader = shader;
			cmd->x = x; cmd->y = y; cmd->w = w; cmd->h = h;
			cmd->picw = picw; cmd->pich = pich;
			cmd->offsetX = offsetX; cmd->offsetY = offsetY;
		}
		return;
	}

	RB_2D_TilePicOffset( shader, x, y, w, h, picw, pich, offsetX, offsetY );
}

void Draw_TrianglePic(const vec2_t vPoints[3], const vec2_t vTexCoords[3], qhandle_t hShader) {
	shader_t* shader;

	if ( !glConfig.smpActive ) {
		R_SyncRenderThread();
		R_IssuePendingRenderCommands();
	}

	if (hShader) {
		shader = R_GetShaderByHandle(hShader);
	}
	else {
		shader = tr.defaultShader;
	}

	if ( glConfig.smpActive ) {
		trianglePic2DCommand_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_TRIANGLE_PIC;
			cmd->shader = shader;
			Com_Memcpy( cmd->points, vPoints, sizeof( cmd->points ) );
			Com_Memcpy( cmd->texCoords, vTexCoords, sizeof( cmd->texCoords ) );
		}
		return;
	}

	RB_2D_TrianglePic( shader, vPoints, vTexCoords );
}

void RE_DrawBackground_TexSubImage(int cols, int rows, int bgr, byte* data) {
	byte	*upload = data;
	GLenum	format = GL_RGB;
	int		w, h;

	w = glConfig.vidWidth;
	h = glConfig.vidHeight;

	R_SyncRenderThread();
	R_FlushRenderCommands();
	qglFinish();

	if (bgr) {
		int	n = cols * rows * 3;
		int	i;

		upload = (byte *)ri.Malloc(n);
		for (i = 0; i + 2 < n; i += 3) {
			upload[i + 0] = data[i + 2];
			upload[i + 1] = data[i + 1];
			upload[i + 2] = data[i + 0];
		}
	}

	GL_BindUnit0(tr.scratchImage);

	if (cols == tr.scratchImage->width && rows == tr.scratchImage->height && format == tr.scratchImage->internalFormat)
	{
		qglTexSubImage2D(3553, 0, 0, 0, cols, rows, format, 5121, upload);
	}
	else
	{
		tr.scratchImage->uploadWidth = cols;
		tr.scratchImage->uploadHeight = rows;
		tr.scratchImage->internalFormat = format;
		qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, cols, rows, 0, format, 5121, upload);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 9729.0);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 9729.0);
	}

	if (bgr) {
		ri.Free( upload );
	}

	GL_Cull( CT_TWO_SIDED );
	GLES3_ApplyStateBits( GLS_DEPTHTEST_DISABLE );

	{
		gles3Program_t	*p2d = GLES3_BeginFamily( GLP_FAMILY_2D, 1 );

		(void)p2d;
		RB_ImmBegin( GL_TRIANGLES );

		RB_ImmTexCoord2f(0.5 / (GLfloat)cols, ((GLfloat)rows - 0.5) / rows);
		RB_ImmVertex2f(0, 0);

		RB_ImmTexCoord2f(((GLfloat)cols - 0.5) / cols, ((GLfloat)rows - 0.5) / rows);
		RB_ImmVertex2f(w, 0);

		RB_ImmTexCoord2f(((GLfloat)cols - 0.5) / cols, 0.5 / (GLfloat)rows);
		RB_ImmVertex2f(w, h);

		RB_ImmTexCoord2f(0.5 / (GLfloat)cols, ((GLfloat)rows - 0.5) / rows);
		RB_ImmVertex2f(0, 0);

		RB_ImmTexCoord2f(((GLfloat)cols - 0.5) / cols, 0.5 / (GLfloat)rows);
		RB_ImmVertex2f(w, h);

		RB_ImmTexCoord2f(0.5 / (GLfloat)rows, 0.5 / (GLfloat)rows);
		RB_ImmVertex2f(0, h);

		RB_ImmEnd();
	}
}

void RE_DrawBackground_DrawPixels(int cols, int rows, int bgr, byte* data) {
	byte	*upload = data;
	float	zoomX, zoomY, qw, qh;
	gles3Program_t	*p2d;

	R_SyncRenderThread();
	R_FlushRenderCommands();

	if (bgr) {
		int	n = cols * rows * 3;
		int	i;

		upload = (byte *)ri.Malloc(n);
		for (i = 0; i + 2 < n; i += 3) {
			upload[i + 0] = data[i + 2];
			upload[i + 1] = data[i + 1];
			upload[i + 2] = data[i + 0];
		}
	}

	GL_BindUnit0( tr.scratchImage );
	tr.scratchImage->uploadWidth = cols;
	tr.scratchImage->uploadHeight = rows;
	tr.scratchImage->internalFormat = GL_RGB;
	qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, cols, rows, 0, GL_RGB, GL_UNSIGNED_BYTE, upload );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

	if (bgr) {
		ri.Free( upload );
	}

	GLES3_ApplyStateBits(0);

	zoomX = (float)glConfig.vidWidth / (float)rows;
	zoomY = (float)glConfig.vidHeight / (float)cols;
	qw = (float)cols * zoomX;
	qh = (float)rows * zoomY;

	p2d = GLES3_BeginFamily( GLP_FAMILY_2D, 1 );
	GLES3_SetBaseColor4f( p2d, 1.0f, 1.0f, 1.0f, 1.0f );

	RB_ImmBegin( GL_TRIANGLES );
	RB_ImmTexCoord2f( 0, 0 ); RB_ImmVertex2f( 0, 0 );
	RB_ImmTexCoord2f( 1, 0 ); RB_ImmVertex2f( qw, 0 );
	RB_ImmTexCoord2f( 1, 1 ); RB_ImmVertex2f( qw, qh );
	RB_ImmTexCoord2f( 0, 0 ); RB_ImmVertex2f( 0, 0 );
	RB_ImmTexCoord2f( 1, 1 ); RB_ImmVertex2f( qw, qh );
	RB_ImmTexCoord2f( 0, 1 ); RB_ImmVertex2f( 0, qh );
	RB_ImmEnd();
}

void AddBox(float x, float y, float w, float h) {
	if (!r_draw2d->integer) {
		return;
	}

	if ( !glConfig.smpActive ) {
		R_SyncRenderThread();
		R_IssuePendingRenderCommands();
		RB_2D_Box( x, y, w, h, qfalse );
		return;
	}

	{
		box2DCommand_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_BOX;
			cmd->x = x; cmd->y = y; cmd->w = w; cmd->h = h;
			cmd->blend = 0;
		}
	}
}

void DrawBox(float x, float y, float w, float h) {
	if (!r_draw2d->integer) {
		return;
	}

	if ( !glConfig.smpActive ) {
		R_SyncRenderThread();
		R_IssuePendingRenderCommands();
		RB_2D_Box( x, y, w, h, qtrue );
		return;
	}

	{
		box2DCommand_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_BOX;
			cmd->x = x; cmd->y = y; cmd->w = w; cmd->h = h;
			cmd->blend = 1;
		}
	}
}

void DrawLineLoop(const vec2_t* points, int count, int stipple_factor, int stipple_mask) {
	if (!r_draw2d->integer) {
		return;
	}

	if ( !glConfig.smpActive ) {
		R_SyncRenderThread();
		R_IssuePendingRenderCommands();
		RB_2D_LineLoop( points, count, stipple_factor, stipple_mask );
		return;
	}

	if ( count <= 0 ) {
		return;
	}

	{
		int bytes = PAD( (int)sizeof( lineLoop2DCommand_t ) + count * (int)sizeof( vec2_t ), sizeof( void * ) );
		lineLoop2DCommand_t *cmd = R_GetCommandBuffer( bytes );

		if ( cmd ) {
			cmd->commandId = RC_2D_LINE_LOOP;
			cmd->count = count;
			cmd->stipple_factor = stipple_factor;
			cmd->stipple_mask = stipple_mask;
			Com_Memcpy( (vec2_t *)( cmd + 1 ), points, count * sizeof( vec2_t ) );
		}
	}
}

void Set2DWindow(int x, int y, int w, int h, float left, float right, float bottom, float top, float n, float f) {
	if ( !glConfig.smpActive ) {
		R_SyncRenderThread();
		R_IssuePendingRenderCommands();
		RB_2D_SetWindow( x, y, w, h, left, right, bottom, top, n, f );
		return;
	}

	if ( r_window2DValid && r_scissor2DValid &&
		 r_window2D[0] == x && r_window2D[1] == y && r_window2D[2] == w && r_window2D[3] == h &&
		 r_window2DOrtho[0] == left && r_window2DOrtho[1] == right && r_window2DOrtho[2] == bottom &&
		 r_window2DOrtho[3] == top && r_window2DOrtho[4] == n && r_window2DOrtho[5] == f &&
		 r_scissor2D[0] == x && r_scissor2D[1] == y && r_scissor2D[2] == w && r_scissor2D[3] == h ) {
		return;
	}

	{
		setWindow2DCommand_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_SET_WINDOW;
			cmd->x = x; cmd->y = y; cmd->w = w; cmd->h = h;
			cmd->left = left; cmd->right = right; cmd->bottom = bottom; cmd->top = top;
			cmd->n = n; cmd->f = f;

			r_window2DValid = qtrue;
			r_window2D[0] = x; r_window2D[1] = y; r_window2D[2] = w; r_window2D[3] = h;
			r_window2DOrtho[0] = left; r_window2DOrtho[1] = right; r_window2DOrtho[2] = bottom;
			r_window2DOrtho[3] = top; r_window2DOrtho[4] = n; r_window2DOrtho[5] = f;
			r_scissor2DValid = qtrue;
			r_scissor2D[0] = x; r_scissor2D[1] = y; r_scissor2D[2] = w; r_scissor2D[3] = h;
		}
	}
}

void RE_Scissor(int x, int y, int width, int height) {
	if ( !glConfig.smpActive ) {
		RB_2D_Scissor( x, y, width, height );
		return;
	}

	if ( r_scissor2DValid &&
		 r_scissor2D[0] == x && r_scissor2D[1] == y && r_scissor2D[2] == width && r_scissor2D[3] == height ) {
		return;
	}

	{
		scissor2DCommand_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_SCISSOR;
			cmd->x = x; cmd->y = y; cmd->width = width; cmd->height = height;

			r_scissor2DValid = qtrue;
			r_scissor2D[0] = x; r_scissor2D[1] = y; r_scissor2D[2] = width; r_scissor2D[3] = height;
		}
	}
}

void Set2DInitialShaderTime(float startTime)
{
	if ( !glConfig.smpActive ) {
		RB_2D_ShaderTime( startTime );
		return;
	}

	{
		shaderTime2DCommand_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_SHADER_TIME;
			cmd->startTime = startTime;
		}
	}
}
