#include "tr_hwtex.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SMP_TOL_MEAN 16.0
#define SMP_TOL_P99  128

static int g_fail = 0;
static int g_fault = 0;

static void chk(int ok, const char *what)
{
	printf("  %s %s\n", ok ? "PASS" : "FAIL", what);
	if (!ok) g_fail++;
}

static EGLDisplay probe_display(void)
{
	EGLDisplay d = eglGetDisplay(EGL_DEFAULT_DISPLAY);

	if (d == EGL_NO_DISPLAY) {
		PFNEGLGETPLATFORMDISPLAYEXTPROC g =
			(PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
		if (g) d = g(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
	}
	return d;
}

static GLuint make_program(void)
{
	static const char *vs =
		"#version 300 es\n"
		"void main(){ vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));"
		" gl_Position=vec4(p*2.0-1.0,0.0,1.0); }\n";
	static const char *fs =
		"#version 300 es\n"
		"precision highp float;\n"
		"uniform highp sampler2D tex;\n"
		"out vec4 o;\n"
		"void main(){ ivec2 s=textureSize(tex,0);"
		" o=texture(tex, gl_FragCoord.xy/vec2(s)); }\n";
	GLuint p = glCreateProgram(), v = glCreateShader(GL_VERTEX_SHADER), f = glCreateShader(GL_FRAGMENT_SHADER);
	GLint  ok = 0;

	glShaderSource(v, 1, &vs, NULL);
	glCompileShader(v);
	glShaderSource(f, 1, &fs, NULL);
	glCompileShader(f);
	glGetShaderiv(v, GL_COMPILE_STATUS, &ok);
	if (!ok) { char l[1024]; glGetShaderInfoLog(v, sizeof l, NULL, l); printf("vs: %s\n", l); }
	glGetShaderiv(f, GL_COMPILE_STATUS, &ok);
	if (!ok) { char l[1024]; glGetShaderInfoLog(f, sizeof l, NULL, l); printf("fs: %s\n", l); }
	glAttachShader(p, v);
	glAttachShader(p, f);
	glLinkProgram(p);
	glGetProgramiv(p, GL_LINK_STATUS, &ok);
	if (!ok) { char l[1024]; glGetProgramInfoLog(p, sizeof l, NULL, l); printf("link: %s\n", l); }
	return p;
}

/* Upload `blocks` as a w x h compressed texture, sample it 1:1 and read the
 * texels back in upload-row order.  A compressed image can never be a colour
 * attachment, so the readback goes through an RGBA8 render target. */
static int gpu_decode(int w, int h, GLenum fmt, const void *blocks, int nbytes, unsigned char *out)
{
	GLuint tex = 0, rt = 0, fbo = 0, prog;
	GLenum err;

	prog = make_program();
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glCompressedTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, nbytes, blocks);
	err = glGetError();
	if (err != GL_NO_ERROR) {
		printf("    glCompressedTexImage2D fmt=0x%04x -> 0x%04x\n", (unsigned)fmt, (unsigned)err);
		return 1;
	}
	glGenTextures(1, &rt);
	glBindTexture(GL_TEXTURE_2D, rt);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		printf("    fbo incomplete\n");
		return 1;
	}
	glViewport(0, 0, w, h);
	glUseProgram(prog);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex);
	glUniform1i(glGetUniformLocation(prog, "tex"), 0);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, out);
	err = glGetError();
	if (err != GL_NO_ERROR) {
		printf("    readback error 0x%04x\n", (unsigned)err);
		return 1;
	}
	return 0;
}

static int cmp_int(const void *a, const void *b)
{
	return (*(const int *)a) - (*(const int *)b);
}

static void report(const char *tag, const unsigned char *imgref, const unsigned char *imgot,
				   int w, int h)
{
	int		   n = w * h * 4, i;
	long long  sum = 0;
	long long  bsum[4] = { 0, 0, 0, 0 };
	int		   bmax[4] = { 0, 0, 0, 0 };
	int		  *hist = (int *)malloc(sizeof(int) * n);
	int		   p99;

	for (i = 0; i < n; i++) {
		int d = (int)imgref[i] - (int)imgot[i];

		if (d < 0) d = -d;
		hist[i] = d;
		sum += d;
		bsum[i & 3] += d;
		if (d > bmax[i & 3]) bmax[i & 3] = d;
	}
	qsort(hist, n, sizeof(int), cmp_int);
	p99 = hist[(int)((double)(n - 1) * 0.99)];

	printf("  %-12s texels=%d mean=%.2f rgbMean=%.2f,%.2f,%.2f aMean=%.2f "
		   "p99=%d max=%d r=%d g=%d b=%d a=%d\n",
		   tag, w * h, (double)sum / n,
		   (double)bsum[0] / (w * h), (double)bsum[1] / (w * h), (double)bsum[2] / (w * h),
		   (double)bsum[3] / (w * h), p99, hist[n - 1],
		   bmax[0], bmax[1], bmax[2], bmax[3]);
	free(hist);

	if ((double)sum / n > SMP_TOL_MEAN || p99 > SMP_TOL_P99) {
		printf("    -> OUT OF TOLERANCE (mean %.2f > %.2f or p99 %d > %d)\n",
			   (double)sum / n, SMP_TOL_MEAN, p99, SMP_TOL_P99);
		g_fail++;
	}
}

static void encode_dxt3_block(const unsigned char rgba[64], unsigned char dxt[16])
{
	int i, j, best_i = 0, best_j = 15, best_d = -1;

	memset(dxt, 0, 16);
	for (i = 0; i < 16; i++) {
		int a = (rgba[i * 4 + 3] * 15 + 127) / 255;

		dxt[i >> 1] |= (unsigned char)(a << ((i & 1) * 4));
	}
	for (i = 0; i < 16; i++) {
		for (j = i + 1; j < 16; j++) {
			int dr = (int)rgba[i * 4 + 0] - (int)rgba[j * 4 + 0];
			int dg = (int)rgba[i * 4 + 1] - (int)rgba[j * 4 + 1];
			int db = (int)rgba[i * 4 + 2] - (int)rgba[j * 4 + 2];
			int d = dr * dr + dg * dg + db * db;

			if (d > best_d) { best_d = d; best_i = i; best_j = j; }
		}
	}
	{
		unsigned int c0 = (unsigned int)((rgba[best_i * 4 + 0] >> 3) << 11) |
						  (unsigned int)((rgba[best_i * 4 + 1] >> 2) << 5) |
						  (unsigned int)(rgba[best_i * 4 + 2] >> 3);
		unsigned int c1 = (unsigned int)((rgba[best_j * 4 + 0] >> 3) << 11) |
						  (unsigned int)((rgba[best_j * 4 + 1] >> 2) << 5) |
						  (unsigned int)(rgba[best_j * 4 + 2] >> 3);
		int			 a0[3], a1[3], k;
		unsigned int bits = 0;

		dxt[8] = (unsigned char)(c0 & 0xFF);
		dxt[9] = (unsigned char)((c0 >> 8) & 0xFF);
		dxt[10] = (unsigned char)(c1 & 0xFF);
		dxt[11] = (unsigned char)((c1 >> 8) & 0xFF);

		a0[0] = ((int)(c0 >> 11) & 0x1F) * 255 / 31;
		a0[1] = ((int)(c0 >> 5) & 0x3F) * 255 / 63;
		a0[2] = ((int)c0 & 0x1F) * 255 / 31;
		a1[0] = ((int)(c1 >> 11) & 0x1F) * 255 / 31;
		a1[1] = ((int)(c1 >> 5) & 0x3F) * 255 / 63;
		a1[2] = ((int)c1 & 0x1F) * 255 / 31;
		for (i = 0; i < 16; i++) {
			int dr = a1[0] - a0[0], dg = a1[1] - a0[1], db = a1[2] - a0[2];
			int dd = dr * dr + dg * dg + db * db;
			int t, idx;

			if (dd <= 0) {
				idx = 0;
			} else {
				int pr = (int)rgba[i * 4 + 0] - a0[0];
				int pg = (int)rgba[i * 4 + 1] - a0[1];
				int pb = (int)rgba[i * 4 + 2] - a0[2];

				t = (pr * dr + pg * dg + pb * db) * 3;
				if (t <= 0) idx = 0;
				else if (t >= dd) idx = 1;
				else if (t * 2 < dd) idx = 2;
				else idx = 3;
			}
			bits |= (unsigned int)idx << (2 * i);
		}
		for (k = 0; k < 4; k++) {
			dxt[12 + k] = (unsigned char)((bits >> (8 * k)) & 0xFF);
		}
	}
}

static unsigned int rd32(const unsigned char *p)
{
	return (unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

int main(void)
{
	EGLDisplay dpy;
	EGLConfig  cfg;
	EGLContext ctx;
	EGLSurface surf;
	EGLint	   n, major = 0, minor = 0;
	int		   hasAstc = 0, hasS3tc = 0, hasEtc2Ext = 0, hasArbEs3 = 0;
	const char *vstr;
	int		   i, t;

	unsigned char *dxt = NULL, *ref = NULL, *enc = NULL, *got = NULL;
	int			   W = 0, H = 0, bw = 0, bh = 0, nb = 0;
	unsigned char  synth[64 * 16];

	if (getenv("SMP_HWTEX_FAULT")) {
		g_fault = 1;
	}

	dpy = probe_display();
	if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, &major, &minor)) {
		printf("no EGL display\n");
		return 2;
	}
	eglBindAPI(EGL_OPENGL_ES_API);
	{
		EGLint ca[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
						EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE };
		EGLint xa[] = { EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE };
		EGLint pa[] = { EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE };

		eglChooseConfig(dpy, ca, &cfg, 1, &n);
		ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, xa);
		surf = eglCreatePbufferSurface(dpy, cfg, pa);
		if (!eglMakeCurrent(dpy, surf, surf, ctx)) {
			printf("no GLES3 context\n");
			return 2;
		}
	}
	vstr = (const char *)glGetString(GL_VERSION);
	{
		GLint nn = 0;

		glGetIntegerv(GL_NUM_EXTENSIONS, &nn);
		for (i = 0; i < nn; i++) {
			const char *e = (const char *)glGetStringi(GL_EXTENSIONS, (GLuint)i);

			if (!strcmp(e, "GL_KHR_texture_compression_astc_ldr")) hasAstc = 1;
			if (!strcmp(e, "GL_EXT_texture_compression_s3tc")) hasS3tc = 1;
			if (!strcmp(e, "GL_EXT_texture_compression_etc2")) hasEtc2Ext = 1;
			if (!strcmp(e, "GL_ARB_ES3_compatibility")) hasArbEs3 = 1;
		}
	}
	printf("[SMP:hwtex] GL_VERSION=%s astc_ldr=%d s3tc=%d etc2_ext=%d arb_es3=%d\n",
		   vstr ? vstr : "?", hasAstc, hasS3tc, hasEtc2Ext, hasArbEs3);

	printf("-- detection selection (simulated capability sets) --\n");
	chk(SMP_SelectTexTranscode(3, 2, 0, 0, 0, 0, 1) == SMP_TEXC_ETC2_RGBA8,
		"ES 3.2, s3tc=0 etc2_ext=0 astc=0 -> ETC2 (the Mali-G715 false negative)");
	chk(SMP_SelectTexTranscode(3, 0, 0, 0, 0, 0, 1) == SMP_TEXC_ETC2_RGBA8,
		"ES 3.0 core -> ETC2");
	chk(SMP_SelectTexTranscode(3, 2, 0, 0, 0, 1, 1) == SMP_TEXC_ASTC_4x4,
		"ES 3.2 + astc_ldr -> ASTC 4x4 (preferred)");
	chk(SMP_SelectTexTranscode(3, 2, 1, 0, 0, 1, 1) == SMP_TEXC_ASTC_4x4,
		"ES 3.2 + s3tc + astc -> ASTC (transcode preferred over direct S3TC)");
	chk(SMP_SelectTexTranscode(2, 0, 0, 1, 0, 0, 1) == SMP_TEXC_ETC2_RGBA8,
		"ES 2.0 + GL_EXT_texture_compression_etc2 backport -> ETC2");
	chk(SMP_SelectTexTranscode(2, 0, 0, 0, 1, 0, 1) == SMP_TEXC_ETC2_RGBA8,
		"ES 2.0 + GL_ARB_ES3_compatibility backport -> ETC2");
	chk(SMP_SelectTexTranscode(2, 0, 1, 0, 0, 0, 1) == SMP_TEXC_NONE,
		"ES 2.0, s3tc only -> software DXT decode (no ETC2 backport)");
	chk(SMP_SelectTexTranscode(1, 1, 0, 0, 0, 0, 1) == SMP_TEXC_NONE,
		"ES 1.1 / no compressed support -> software DXT decode");
	chk(SMP_SelectTexTranscode(3, 2, 1, 0, 0, 0, 0) == SMP_TEXC_NONE,
		"r_ext_compress_textures 0 -> software DXT decode");
	chk(SMP_SelectTexTranscode(3, 2, 1, 0, 0, 1, 0) == SMP_TEXC_NONE,
		"r_ext_compress_textures 0 blocks even ASTC");

	{
		const char *ddsPath = getenv("SMP_HWTEX_DDS");

		if (ddsPath && !ddsPath[0]) {
			ddsPath = NULL;
		}
		if (ddsPath) {
			FILE *f = fopen(ddsPath, "rb");
			long  len;

			if (!f) {
				printf("cannot open %s\n", ddsPath);
				return 2;
			}
			fseek(f, 0, SEEK_END);
			len = ftell(f);
			fseek(f, 0, SEEK_SET);
			{
				unsigned char *file = (unsigned char *)malloc(len);

				if (fread(file, 1, len, f) != (size_t)len) { fclose(f); return 2; }
				fclose(f);
				if (memcmp(file, "DDS ", 4) != 0 || rd32(file + 84) != 0x33545844) {
					printf("%s is not a DXT3 .dds\n", ddsPath);
					return 2;
				}
				H = (int)rd32(file + 12);
				W = (int)rd32(file + 16);
				bw = (W + 3) / 4;
				bh = (H + 3) / 4;
				nb = bw * bh;
				dxt = (unsigned char *)malloc((size_t)nb * 16);
				memcpy(dxt, file + 128, (size_t)nb * 16);
				free(file);
			}
		} else {
			int pat;

			for (pat = 0; pat < 8; pat++) {
				int v;

				for (v = 0; v < 8; v++) {
					unsigned char rgba[64];
					int			  x, y;

					for (y = 0; y < 4; y++) {
						for (x = 0; x < 4; x++) {
							int idx = y * 4 + x;
							int r = 0, g = 0, b = 0, a = 255;
							unsigned int lv = (unsigned int)(v * 32);

							switch (pat) {
							case 0: 
								r = 90 + x * 12 + v * 8; g = 80 + y * 10 + v * 6; b = 60 + v * 10; break;
							case 1: 
								r = 20 + x * 60; g = 15 + x * 48; b = 10 + x * 36; break;
							case 2: 
								r = 200 - y * 45; g = 160 - y * 36; b = 120 - y * 27; break;
							case 3: 
								r = (x * 5 + v) * 8; g = (y * 5 + v) * 8; b = 200 - (x + y) * 20; break;
							case 4: 
								r = 180; g = 140; b = 100; a = x * 64 + y * 16; break;
							case 5: 
								r = 200; g = 40; b = 40; a = (x + y >= 3) ? 255 : 0; break;
							case 6: 
								r = 40 + (int)lv; g = 40 + (int)lv; b = 40 + (int)lv;
								if (x >= 1 && x <= 2 && y >= 1 && y <= 2) { r = 220; g = 230; b = 255; }
								break;
							default: 
								r = 15 + (x + y) * 9; g = 20 + (x + y) * 11; b = 30 + (x + y) * 13;
								a = 255 - (x + y) * 12;
								break;
							}
							rgba[idx * 4 + 0] = (unsigned char)r;
							rgba[idx * 4 + 1] = (unsigned char)g;
							rgba[idx * 4 + 2] = (unsigned char)b;
							rgba[idx * 4 + 3] = (unsigned char)a;
						}
					}
					encode_dxt3_block(rgba, synth + (pat * 8 + v) * 16);
				}
			}
			W = 4;
			H = 256;
			bw = 1;
			bh = 64;
			nb = 64;
			dxt = synth;
		}
	}
	printf("-- transcode test image: %dx%d (%d DXT3 blocks)%s --\n", W, H, nb,
		   getenv("SMP_HWTEX_DDS") ? " [real shipped level]" : " [synthetic]");

	ref = (unsigned char *)malloc((size_t)W * H * 4);
	enc = (unsigned char *)malloc((size_t)nb * SMP_TEXC_BLOCK_BYTES);
	got = (unsigned char *)malloc((size_t)W * H * 4);

	for (i = 0; i < nb; i++) {
		int bx = i % bw, by = i / bw;
		unsigned char blockRGBA[64];
		int x, y, k;

		SMP_DXTDecodeBlock(SMP_DXT3, dxt + i * 16, blockRGBA);
		for (y = 0; y < 4; y++) {
			for (x = 0; x < 4; x++) {
				size_t pi = ((size_t)(by * 4 + y) * W + (bx * 4 + x)) * 4;

				for (k = 0; k < 4; k++) {
					ref[pi + k] = blockRGBA[(y * 4 + x) * 4 + k];
				}
			}
		}
	}

	for (t = 0; t < 2; t++) {
		int		   target = (t == 0) ? SMP_TEXC_ETC2_RGBA8 : SMP_TEXC_ASTC_4x4;
		GLenum	   fmt = (t == 0) ? GL_COMPRESSED_RGBA8_ETC2_EAC : GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
		const char *tag = (t == 0) ? "ETC2_RGBA8" : "ASTC_4x4";

		if (t == 1 && !hasAstc) {
			printf("-- %s: not advertised on this driver, skipped\n", tag);
			continue;
		}
		printf("-- %s: driver decode vs the port's software DXT3 decode --\n", tag);
		for (i = 0; i < nb; i++) {
			const unsigned char *src = dxt + i * 16;

			if (g_fault) {
				unsigned char bad[16];

				memset(bad, 0xA5, 16); 
				SMP_TranscodeBlock(SMP_DXT3, bad, target, enc + i * 16);
			} else {
				SMP_TranscodeBlock(SMP_DXT3, src, target, enc + i * 16);
			}
		}
		if (g_fault) {
			printf("  [self-check: encoder input corrupted, comparison must FAIL]\n");
		}
		if (gpu_decode(W, H, fmt, enc, nb * SMP_TEXC_BLOCK_BYTES, got) == 0) {
			report(tag, ref, got, W, H);
		} else {
			chk(0, "driver rejected the transcoded blocks");
		}
	}

	if (hasS3tc) {
		printf("-- reference cross-check: driver S3TC decode vs software decode --\n");
		if (gpu_decode(W, H, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, dxt, nb * 16, got) == 0) {
			int x, y, k, maxe = 0;

			for (y = 0; y < H; y++) {
				for (x = 0; x < W; x++) {
					unsigned char blockRGBA[64];
					size_t		 bi = (size_t)((y >> 2) * bw + (x >> 2)) * 16;
					size_t		 pi = ((size_t)y * W + x) * 4;

					SMP_DXTDecodeBlock(SMP_DXT3, dxt + bi, blockRGBA);
					for (k = 0; k < 4; k++) {
						int d = (int)blockRGBA[((y & 3) * 4 + (x & 3)) * 4 + k] -
								(int)got[pi + k];

						if (d < 0) d = -d;
						if (d > maxe) maxe = d;
					}
				}
			}
			printf("  S3TC vs software max abs error = %d (4-bit alpha/565 rounding)\n", maxe);
			chk(maxe <= 8, "the reference DXT3 decode agrees with the driver's own");
		} else {
			chk(0, "driver rejected raw DXT3");
		}
	}

	printf("result: %s\n", g_fail ? "FAIL" : "PASS");
	return g_fail ? 1 : 0;
}
