#ifndef SMP_HWTEX_H
#define SMP_HWTEX_H

typedef enum {
	SMP_TEXC_NONE = 0,
	SMP_TEXC_ETC2_RGBA8,	
	SMP_TEXC_ASTC_4x4		
} smpTexTranscode_t;

#define SMP_DXT1 1
#define SMP_DXT3 3
#define SMP_DXT5 5

#define SMP_TEXC_BLOCK_BYTES 16

int SMP_SelectTexTranscode(int glesMajor, int glesMinor,
							int hasS3tc, int hasEtc2Ext, int hasArbEs3,
							int hasAstcLdr, int compressAllowed);

void SMP_InitCodecTables(void);

void SMP_DXTDecodeBlock(int dxt, const unsigned char *block, unsigned char rgba[64]);

void SMP_TranscodeBlock(int dxt, const unsigned char *src, int target, unsigned char *dst);
void SMP_TranscodeRGBA(const unsigned char rgba[64], int target, unsigned char *dst);

typedef struct {
	const unsigned char *src;	
	unsigned char		*dst;	
	int					 dxt;
	int					 target;
	int					 bw;	
} smpTranscodeRows_t;

void SMP_TranscodeRows(void *vctx, int begin, int end);

void SMP_MipMap(unsigned char *in, int width, int height);

int SMP_TranscodeChainBytes(int upW, int upH, int levels);

typedef void (*smpTranscodeRunner_t)(void *ud, smpTranscodeRows_t *ctx, int bh, int bw);

int SMP_TranscodeChain(const unsigned char *data, int srcDxt, int width, int height,
						int iMipmapsAvailable, int base,
						int upW, int upH, int levels, int target,
						unsigned char *out,
						void *(*alloc)(int), void (*dealloc)(void *),
						smpTranscodeRunner_t runRows, void *runUd);

#endif 
