#ifndef R201_HWTEX_H
#define R201_HWTEX_H

typedef enum {
	R201_TEXC_NONE = 0,
	R201_TEXC_ETC2_RGBA8,	
	R201_TEXC_ASTC_4x4		
} r201TexTranscode_t;

#define R201_DXT1 1
#define R201_DXT3 3
#define R201_DXT5 5

#define R201_TEXC_BLOCK_BYTES 16

int R201_SelectTexTranscode(int glesMajor, int glesMinor,
							int hasS3tc, int hasEtc2Ext, int hasArbEs3,
							int hasAstcLdr, int compressAllowed);

void R201_InitCodecTables(void);

void R201_DXTDecodeBlock(int dxt, const unsigned char *block, unsigned char rgba[64]);

void R201_TranscodeBlock(int dxt, const unsigned char *src, int target, unsigned char *dst);
void R201_TranscodeRGBA(const unsigned char rgba[64], int target, unsigned char *dst);

typedef struct {
	const unsigned char *src;	
	unsigned char		*dst;	
	int					 dxt;
	int					 target;
	int					 bw;	
} r201TranscodeRows_t;

void R201_TranscodeRows(void *vctx, int begin, int end);

void R201_MipMap(unsigned char *in, int width, int height);

int R201_TranscodeChainBytes(int upW, int upH, int levels);

typedef void (*r201TranscodeRunner_t)(void *ud, r201TranscodeRows_t *ctx, int bh, int bw);

int R201_TranscodeChain(const unsigned char *data, int srcDxt, int width, int height,
						int iMipmapsAvailable, int base,
						int upW, int upH, int levels, int target,
						unsigned char *out,
						void *(*alloc)(int), void (*dealloc)(void *),
						r201TranscodeRunner_t runRows, void *runUd);

#endif 
