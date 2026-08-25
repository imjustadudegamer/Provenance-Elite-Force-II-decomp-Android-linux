
#include "tr_local.h"
#include "gles3_g1_api.h"

vec4_t r_colorWhite = { 1.0, 1.0, 1.0, 1.0 };

void Draw_SetColor(const vec4_t rgba) {
#if 1
	if (!rgba) {
		rgba = r_colorWhite;
	}

	backEnd.color2D[0] = (byte)(rgba[0] * tr.identityLightByte);
	backEnd.color2D[1] = (byte)(rgba[1] * tr.identityLightByte);
	backEnd.color2D[2] = (byte)(rgba[2] * tr.identityLightByte);
	backEnd.color2D[3] = (byte)(rgba[3] * 255.0);
	qglColor4ubv(backEnd.color2D);
#else
	RE_SetColor(rgba);
#endif
}

void R_ChangeTextColor(int index) {
	vec4_t color;

	if (index < 0 || index > 7) {
		return;
	}

	color[0] = g_color_table[index][0];
	color[1] = g_color_table[index][1];
	color[2] = g_color_table[index][2];
	color[3] = backEnd.color2D[3] / 255.0;
	Draw_SetColor(color);
}

void Draw_StretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader) {
#if 1
	shader_t* shader;

	R_SyncRenderThread();
	R_IssuePendingRenderCommands();

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
#else
	RE_StretchPic(x, y, w, h, s1, t1, s2, t2, hShader);
#endif
}

void Draw_StretchPic2(float x, float y, float w, float h, float s1, float t1, float s2, float t2, float sx, float sy, qhandle_t hShader) {
	shader_t* shader;
	float halfWidth, halfHeight;
	float scaledWidth1, scaledHeight1;
	float scaledWidth2, scaledHeight2;

	R_SyncRenderThread();
	R_IssuePendingRenderCommands();

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


void Draw_TilePic(float x, float y, float w, float h, qhandle_t hShader) {
	shader_t* shader;
	float		picw, pich;

	R_SyncRenderThread();
	R_IssuePendingRenderCommands();

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

void Draw_TilePicOffset(float x, float y, float w, float h, qhandle_t hShader, int offsetX, int offsetY) {
	shader_t* shader;
	float		picw, pich;

	R_SyncRenderThread();
	R_IssuePendingRenderCommands();

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

void Draw_TrianglePic(const vec2_t vPoints[3], const vec2_t vTexCoords[3], qhandle_t hShader) {
	int			i;
	shader_t* shader;

	R_SyncRenderThread();
	R_IssuePendingRenderCommands();

	if (hShader) {
		shader = R_GetShaderByHandle(hShader);
	}
	else {
		shader = tr.defaultShader;
	}

	RB_Color4f(backEnd.color2D[0], backEnd.color2D[1], backEnd.color2D[2], backEnd.color2D[3]);

	RB_BeginSurface(shader, 0);

	for (i = 0; i < 3; i++) {
		RB_Texcoord2f(vTexCoords[i][0], vTexCoords[i][1]);
		RB_Vertex2f(vPoints[i][0], vPoints[i][1]);
	}

	RB_StreamEnd();
}

void RE_DrawBackground_TexSubImage(int cols, int rows, int bgr, byte* data) {
	byte	*upload = data;
	GLenum	format = GL_RGB;
	int		w, h;

	w = glConfig.vidWidth;
	h = glConfig.vidHeight;

	R_SyncRenderThread();
	R_IssuePendingRenderCommands();
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
	R_IssuePendingRenderCommands();

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
	R_SyncRenderThread();
	R_IssuePendingRenderCommands();

	// must use GLS_DEPTHTEST_DISABLE (0x800) to reproduce retail's depth-off +
	GLES3_ApplyStateBits(GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE);

	{
		gles3Program_t	*p2d;

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
}

void DrawBox(float x, float y, float w, float h) {
	if (!r_draw2d->integer) {
		return;
	}
	R_SyncRenderThread();
	R_IssuePendingRenderCommands();

	GLES3_ApplyStateBits(GLS_DEPTHTEST_DISABLE | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_SRCBLEND_SRC_ALPHA);

	{
		gles3Program_t	*p2d;

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

void DrawLineLoop(const vec2_t* points, int count, int stipple_factor, int stipple_mask) {
	int		i;

	if (!r_draw2d->integer) {
		return;
	}
	R_SyncRenderThread();
	R_IssuePendingRenderCommands();

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

void Set2DWindow(int x, int y, int w, int h, float left, float right, float bottom, float top, float n, float f) {
	R_SyncRenderThread();
	R_IssuePendingRenderCommands();
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

void RE_Scissor(int x, int y, int width, int height) {
	qglEnable(GL_SCISSOR_TEST);
	qglScissor(x, y, width, height);
	SMP_VpProbe( "scissor", "RE_Scissor/arg", x, y, width, height, 0, qfalse );
}

void Set2DInitialShaderTime(float startTime)
{
	if (backEnd.in2D)
	{
		backEnd.shaderStartTime = startTime;
	}
}
