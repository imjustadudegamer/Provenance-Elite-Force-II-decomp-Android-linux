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
// Modified 2026-10-04 for this port; the changes are listed in PROVENANCE.tsv.
#include "gles3_firstlight.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const char *GLES3_QUAD_VS =
	"#version 300 es\n"
	"in vec2 a_Position;\n"
	"in vec2 a_TexCoord;\n"
	"out vec2 v_TexCoord;\n"
	"uniform mat4 u_MVP;\n"
	"void main() {\n"
	"    v_TexCoord = a_TexCoord;\n"
	"    gl_Position = u_MVP * vec4(a_Position, 0.0, 1.0);\n"
	"}\n";

static const char *GLES3_QUAD_FS =
	"#version 300 es\n"
	"precision mediump float;\n"
	"in vec2 v_TexCoord;\n"
	"uniform sampler2D u_Texture;\n"
	"out vec4 o_Color;\n"
	"void main() {\n"
	"    o_Color = texture(u_Texture, v_TexCoord);\n"
	"}\n";

#define GLES3_ATTR_POS 0
#define GLES3_ATTR_ST  1

typedef struct {
	int			ready;
	unsigned	program;
	int			u_mvp;
	int			u_tex;
	unsigned	vao;
	unsigned	vbo;
	unsigned	white;
} gles3FirstLight_t;

static gles3FirstLight_t gles3_fl;

static void (*gles3_fl_logger)(const char *line) = NULL;

void GLES3_FirstLight_SetLogger(void (*fn)(const char *line))
{
	gles3_fl_logger = fn;
}

static void GLES3_FL_Log(const char *fmt, ...) GLES3_PRINTF_FUNC(1, 2);

static void GLES3_FL_Log(const char *fmt, ...)
{
	char	buf[1024];
	va_list	ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	if (gles3_fl_logger) {
		gles3_fl_logger(buf);
	} else {
		fputs(buf, stderr);
	}
}

static unsigned GLES3_FL_Compile(GLenum type, const char *src, const char *what)
{
	GLuint	sh;
	GLint	ok = 0;
	char	log[1024];

	if (!qglCreateShader || !qglShaderSource || !qglCompileShader) {
		GLES3_FL_Log("[GLES3:firstlight] milestone=%s stage=compile status=fail reason=proc-missing\n", what);
		return 0;
	}

	sh = qglCreateShader(type);
	if (!sh) {
		GLES3_FL_Log("[GLES3:firstlight] milestone=%s stage=compile status=fail reason=CreateShader-0\n", what);
		return 0;
	}
	qglShaderSource(sh, 1, &src, NULL);
	qglCompileShader(sh);
	qglGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		log[0] = '\0';
		if (qglGetShaderInfoLog) {
			qglGetShaderInfoLog(sh, sizeof(log) - 1, NULL, log);
		}
		GLES3_FL_Log("[GLES3:firstlight] milestone=%s stage=compile status=fail log=\"%s\"\n", what, log);
		qglDeleteShader(sh);
		return 0;
	}
	return sh;
}

static int GLES3_FL_BuildProgram(void)
{
	GLuint	vs, fs, prog;
	GLint	ok = 0;
	char	log[1024];

	if (gles3_fl.program) {
		return 1;
	}

	vs = GLES3_FL_Compile(GL_VERTEX_SHADER, GLES3_QUAD_VS, "firstlight-program");
	if (!vs) {
		return 0;
	}
	fs = GLES3_FL_Compile(GL_FRAGMENT_SHADER, GLES3_QUAD_FS, "firstlight-program");
	if (!fs) {
		qglDeleteShader(vs);
		return 0;
	}

	prog = qglCreateProgram();
	qglAttachShader(prog, vs);
	qglAttachShader(prog, fs);
	qglBindAttribLocation(prog, GLES3_ATTR_POS, "a_Position");
	qglBindAttribLocation(prog, GLES3_ATTR_ST, "a_TexCoord");
	qglLinkProgram(prog);
	qglGetProgramiv(prog, GL_LINK_STATUS, &ok);
	qglDeleteShader(vs);
	qglDeleteShader(fs);
	if (!ok) {
		log[0] = '\0';
		if (qglGetProgramInfoLog) {
			qglGetProgramInfoLog(prog, sizeof(log) - 1, NULL, log);
		}
		GLES3_FL_Log("[GLES3:firstlight] milestone=firstlight-program stage=link status=fail log=\"%s\"\n", log);
		qglDeleteProgram(prog);
		return 0;
	}

	gles3_fl.program = prog;
	gles3_fl.u_mvp = qglGetUniformLocation(prog, "u_MVP");
	gles3_fl.u_tex = qglGetUniformLocation(prog, "u_Texture");

	{
		const unsigned char texels[2 * 2 * 4] = {
			255, 0,   0, 255,   0, 255,   0, 255,   
			0,   0, 255, 255,   255, 255, 255, 255   
		};
		gles3_fl.white = GLES3_FirstLight_UploadRGBA8(2, 2, texels);
		if (!gles3_fl.white) {
			GLES3_FL_Log("[GLES3:firstlight] milestone=firstlight-texture status=fail reason=upload-failed\n");
			return 0;
		}
	}

	if (qglGenVertexArrays && qglBindVertexArray) {
		qglGenVertexArrays(1, &gles3_fl.vao);
	}
	qglGenBuffers(1, &gles3_fl.vbo);
	if (!gles3_fl.vbo) {
		GLES3_FL_Log("[GLES3:firstlight] milestone=firstlight-buffers status=fail reason=GenBuffers-0\n");
		return 0;
	}

	gles3_fl.ready = 1;
	return 1;
}

static void GLES3_FL_ScreenSize(int *w, int *h)
{
	GLint	vp[4] = { 0, 0, 0, 0 };

	if (qglGetIntegerv) {
		qglGetIntegerv(GL_VIEWPORT, vp);
	}
	*w = vp[2] > 0 ? vp[2] : 1;
	*h = vp[3] > 0 ? vp[3] : 1;
}

static int GLES3_FL_ReadPixel(int x, int y, unsigned char out[4])
{
	out[0] = out[1] = out[2] = out[3] = 0;
	if (!qglReadPixels) {
		return 0;
	}
	qglReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, out);
	return 1;
}

static int GLES3_FL_ColorNear(const unsigned char got[4], int r, int g, int b, int tol)
{
	return abs((int)got[0] - r) <= tol && abs((int)got[1] - g) <= tol &&
		   abs((int)got[2] - b) <= tol;
}

static void GLES3_FL_Identity(float m[16])
{
	memset(m, 0, sizeof(float) * 16);
	m[0] = m[5] = m[10] = m[15] = 1.0f;
}

unsigned GLES3_FirstLight_ClearAndQuad(void)
{
	unsigned	result = 0;
	unsigned char	px[4];
	int		w = 0, h = 0;
	GLenum		err;
	static const struct { float fx, fy; int r, g, b; const char *name; } quad[4] = {
		{ 0.25f, 0.25f, 255,   0,   0, "red"   },
		{ 0.75f, 0.25f,   0, 255,   0, "green" },
		{ 0.25f, 0.75f,   0,   0, 255, "blue"  },
		{ 0.75f, 0.75f, 255, 255, 255, "white" }
	};
	int	i;

	if (!GLES3_FL_BuildProgram()) {
		GLES3_FL_Log("[GLES3:firstlight] milestone=clear status=fail reason=program\n");
		GLES3_FL_Log("[GLES3:firstlight] milestone=quad status=fail reason=program\n");
		return 0;
	}

	GLES3_FL_ScreenSize(&w, &h);

	if (qglClearColor && qglClear) {
		qglDisable(GL_SCISSOR_TEST);
		qglClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		GLES3_FL_ReadPixel(w / 2, h / 2, px);
		if (GLES3_FL_ColorNear(px, 0, 0, 0, 2)) {
			result |= GLES3_FL_CLEAR;
			GLES3_FL_Log("[GLES3:firstlight] milestone=clear expected=000000 got=%02x%02x%02x status=pass\n",
						px[0], px[1], px[2]);
		} else {
			GLES3_FL_Log("[GLES3:firstlight] milestone=clear expected=000000 got=%02x%02x%02x status=fail\n",
						px[0], px[1], px[2]);
		}
	} else {
		GLES3_FL_Log("[GLES3:firstlight] milestone=clear status=fail reason=proc-missing\n");
	}

	{
		const float verts[4][4] = {
			{ -1.0f, -1.0f, 0.0f, 0.0f },
			{  1.0f, -1.0f, 1.0f, 0.0f },
			{ -1.0f,  1.0f, 0.0f, 1.0f },
			{  1.0f,  1.0f, 1.0f, 1.0f }
		};
		float	mvp[16];
		int	pass = 1;

		GLES3_FL_Identity(mvp);
		qglDisable(GL_BLEND);
		qglDisable(GL_DEPTH_TEST);
		qglDisable(GL_CULL_FACE);
		qglUseProgram(gles3_fl.program);
		if (gles3_fl.u_mvp >= 0) {
			qglUniformMatrix4fv(gles3_fl.u_mvp, 1, GL_FALSE, mvp);
		}
		if (gles3_fl.u_tex >= 0) {
			qglUniform1i(gles3_fl.u_tex, 0);
		}
		qglActiveTexture(GL_TEXTURE0);
		qglBindTexture(GL_TEXTURE_2D, gles3_fl.white);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		if (gles3_fl.vao) {
			qglBindVertexArray(gles3_fl.vao);
		}
		qglBindBuffer(GL_ARRAY_BUFFER, gles3_fl.vbo);
		qglBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
		qglEnableVertexAttribArray(GLES3_ATTR_POS);
		qglVertexAttribPointer(GLES3_ATTR_POS, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
							   (const void *)0);
		qglEnableVertexAttribArray(GLES3_ATTR_ST);
		qglVertexAttribPointer(GLES3_ATTR_ST, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
							   (const void *)(2 * sizeof(float)));

		err = qglGetError ? qglGetError() : GL_NO_ERROR;
		if (err != GL_NO_ERROR) {
			GLES3_FL_Log("[GLES3:firstlight] milestone=quad stage=setup status=fail glerror=0x%x\n", err);
			pass = 0;
		} else {
			qglDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
			err = qglGetError ? qglGetError() : GL_NO_ERROR;
			if (err != GL_NO_ERROR) {
				GLES3_FL_Log("[GLES3:firstlight] milestone=quad stage=draw status=fail glerror=0x%x\n", err);
				pass = 0;
			}
		}

		if (pass) {
			for (i = 0; i < 4; i++) {
				int x = (int)(quad[i].fx * (float)w);
				int y = (int)(quad[i].fy * (float)h);

				if (x >= w) { x = w - 1; }
				if (y >= h) { y = h - 1; }
				GLES3_FL_ReadPixel(x, y, px);
				if (!GLES3_FL_ColorNear(px, quad[i].r, quad[i].g, quad[i].b, 2)) {
					pass = 0;
					GLES3_FL_Log("[GLES3:firstlight] milestone=quad quadrant=%s expected=%02x%02x%02x got=%02x%02x%02x status=fail\n",
								quad[i].name, quad[i].r, quad[i].g, quad[i].b, px[0], px[1], px[2]);
				}
			}
		}

		if (pass) {
			result |= GLES3_FL_QUAD;
			GLES3_FL_Log("[GLES3:firstlight] milestone=quad texels=4 status=pass\n");
		} else if (!(result & GLES3_FL_QUAD)) {
			GLES3_FL_Log("[GLES3:firstlight] milestone=quad status=fail\n");
		}

		if (gles3_fl.vao) {
			qglBindVertexArray(0);
		}
		qglUseProgram(0);
	}

	return result;
}

unsigned GLES3_FirstLight_DrawFace(const float *xyz, const float *st, int numVerts,
								  unsigned tex)
{
	float	mvp[16];
	float	mins[3] = { 1e30f, 1e30f, 1e30f }, maxs[3] = { -1e30f, -1e30f, -1e30f };
	float	verts[64][5];
	float	sx, sy, cx, cy;
	int	i;
	unsigned char	before[4], after[4];
	int	w = 0, h = 0;

	if (!GLES3_FL_BuildProgram() || numVerts < 3 || numVerts > 64) {
		GLES3_FL_Log("[GLES3:firstlight] milestone=bsp_face status=fail reason=setup numVerts=%d\n", numVerts);
		return 0;
	}

	for (i = 0; i < numVerts; i++) {
		int k;
		for (k = 0; k < 3; k++) {
			float v = xyz[i * 3 + k];
			if (v < mins[k]) { mins[k] = v; }
			if (v > maxs[k]) { maxs[k] = v; }
		}
	}
	cx = 0.5f * (mins[0] + maxs[0]);
	cy = 0.5f * (mins[1] + maxs[1]);
	sx = (maxs[0] - mins[0]) > 1e-6f ? 1.6f / (maxs[0] - mins[0]) : 1.0f;
	sy = (maxs[1] - mins[1]) > 1e-6f ? 1.6f / (maxs[1] - mins[1]) : 1.0f;

	for (i = 0; i < numVerts; i++) {
		verts[i][0] = (xyz[i * 3 + 0] - cx) * sx;
		verts[i][1] = (xyz[i * 3 + 1] - cy) * sy;
		verts[i][2] = st[i * 2 + 0];
		verts[i][3] = st[i * 2 + 1];
		verts[i][4] = 0.0f;
	}

	GLES3_FL_Identity(mvp);
	GLES3_FL_ScreenSize(&w, &h);
	GLES3_FL_ReadPixel(w / 2, h / 2, before);

	qglDisable(GL_BLEND);
	qglDisable(GL_DEPTH_TEST);
	qglDisable(GL_CULL_FACE);
	qglUseProgram(gles3_fl.program);
	if (gles3_fl.u_mvp >= 0) {
		qglUniformMatrix4fv(gles3_fl.u_mvp, 1, GL_FALSE, mvp);
	}
	if (gles3_fl.u_tex >= 0) {
		qglUniform1i(gles3_fl.u_tex, 0);
	}
	qglActiveTexture(GL_TEXTURE0);
	qglBindTexture(GL_TEXTURE_2D, tex ? tex : gles3_fl.white);

	if (gles3_fl.vao) {
		qglBindVertexArray(gles3_fl.vao);
	}
	qglBindBuffer(GL_ARRAY_BUFFER, gles3_fl.vbo);
	qglBufferData(GL_ARRAY_BUFFER, sizeof(float) * 5 * numVerts, verts, GL_STATIC_DRAW);
	qglEnableVertexAttribArray(GLES3_ATTR_POS);
	qglVertexAttribPointer(GLES3_ATTR_POS, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
						   (const void *)0);
	qglEnableVertexAttribArray(GLES3_ATTR_ST);
	qglVertexAttribPointer(GLES3_ATTR_ST, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
						   (const void *)(2 * sizeof(float)));
	qglDrawArrays(GL_TRIANGLES, 0, numVerts - (numVerts % 3));

	{
		GLenum err = qglGetError ? qglGetError() : GL_NO_ERROR;
		if (err != GL_NO_ERROR) {
			GLES3_FL_Log("[GLES3:firstlight] milestone=bsp_face status=fail glerror=0x%x verts=%d\n",
						err, numVerts);
			if (gles3_fl.vao) { qglBindVertexArray(0); }
			qglUseProgram(0);
			return 0;
		}
	}

	GLES3_FL_ReadPixel(w / 2, h / 2, after);
	if (gles3_fl.vao) {
		qglBindVertexArray(0);
	}
	qglUseProgram(0);

	GLES3_FL_Log("[GLES3:firstlight] milestone=bsp_face verts=%d drew=1 centre_before=%02x%02x%02x "
				"centre_after=%02x%02x%02x status=pass\n",
				numVerts, before[0], before[1], before[2], after[0], after[1], after[2]);
	return GLES3_FL_FACE;
}

unsigned GLES3_FirstLight_UploadRGBA8(int w, int h, const unsigned char *pixels)
{
	GLuint	tex = 0;

	qglGenTextures(1, &tex);
	if (!tex) {
		return 0;
	}
	qglBindTexture(GL_TEXTURE_2D, tex);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	return tex;
}

void GLES3_FirstLight_DeleteTexture(unsigned tex)
{
	if (tex) {
		GLuint t = tex;
		qglDeleteTextures(1, &t);
	}
}

void GLES3_FirstLight_Shutdown(void)
{
	if (gles3_fl.vbo) {
		qglDeleteBuffers(1, &gles3_fl.vbo);
		gles3_fl.vbo = 0;
	}
	if (gles3_fl.vao) {
		qglDeleteVertexArrays(1, &gles3_fl.vao);
		gles3_fl.vao = 0;
	}
	if (gles3_fl.white) {
		GLES3_FirstLight_DeleteTexture(gles3_fl.white);
		gles3_fl.white = 0;
	}
	if (gles3_fl.program) {
		qglDeleteProgram(gles3_fl.program);
		gles3_fl.program = 0;
	}
	gles3_fl.ready = 0;
}

int GLES3_FirstLight_Ready(void)
{
	return gles3_fl.ready;
}
