#ifndef SMP_TEXCACHE_H
#define SMP_TEXCACHE_H

#define SMP_TEXCACHE_MAGIC			0x43543252u
#define SMP_TEXCACHE_HDR_FIXED		100	

#define SMP_TEXCACHE_ENC_VERSION	1u

#define SMP_TEXCACHE_NAME_MAX		128
#define SMP_TEXCACHE_DIR			"texcache"
#define SMP_TEXCACHE_EXT			"r2tc"
#define SMP_TEXCACHE_PATH_MAX		160
#define SMP_TEXCACHE_DEFAULT_MAX_MB	96
#define SMP_TEXCACHE_MAX_ENTRIES	8192

typedef void *smpTexCacheFile_t;

typedef struct smpTexCacheIO_s {
	long (*openRead)(void *ud, const char *qpath, smpTexCacheFile_t *fh);
	int  (*read)(void *ud, smpTexCacheFile_t fh, void *buf, int len);
	void (*closeRead)(void *ud, smpTexCacheFile_t fh);
	int  (*writeAll)(void *ud, const char *qpath, const void *buf, int len);
	int  (*removeFile)(void *ud, const char *qpath);
	char **(*list)(void *ud, int *count);
	void (*freeList)(void *ud, char **names);
	void *ud;
} smpTexCacheIO_t;

typedef struct {
	unsigned int	version;
	unsigned int	target;			
	unsigned int	srcDxt;			
	int				width, height;	
	int				numMipmaps;		
	int				picmip;			
	int				maxTextureSize;
	int				base;			
	int				upW, upH;		
	int				levels;			
	unsigned int	srcBytes;		
	unsigned int	srcHashLo, srcHashHi;	
	char			name[SMP_TEXCACHE_NAME_MAX];
} smpTexCacheKey_t;

typedef enum {
	SMP_TEXCACHE_STAT_DISABLED = 0,
	SMP_TEXCACHE_STAT_HIT,
	SMP_TEXCACHE_STAT_MISS,		
	SMP_TEXCACHE_STAT_CORRUPT,		
	SMP_TEXCACHE_STAT_STORED,		
	SMP_TEXCACHE_STAT_WRITE_FAIL,
	SMP_TEXCACHE_STAT_SKIP			
} smpTexCacheStat_t;

typedef struct {
	smpTexCacheStat_t	stat;
	int					bytes;		
	int					evicted;	
	char				path[SMP_TEXCACHE_PATH_MAX];
} smpTexCacheResult_t;


void SMP_TexCache_Hash64( const void *data, int bytes, unsigned int *lo, unsigned int *hi );

int SMP_TexCache_StoredBytes( int width, int height, int iMipmapsAvailable, int srcDxt );

void SMP_TexCache_SourceHash( const unsigned char *dxt, int width, int height,
							   int iMipmapsAvailable, int srcDxt,
							   unsigned int *lo, unsigned int *hi, int *storedBytes );

void SMP_TexCache_FillKey( smpTexCacheKey_t *key, const char *name,
							int width, int height, int numMipmaps, int picmip,
							int maxTextureSize, int srcDxt, int target,
							int base, int upW, int upH, int levels,
							unsigned int srcHashLo, unsigned int srcHashHi,
							int srcBytes );

void SMP_TexCache_Path( const smpTexCacheKey_t *key, char *out, int outSize );


void SMP_TexCache_Begin( const smpTexCacheIO_t *io, int enabled, int maxBytes );
void SMP_TexCache_Shutdown( void );
int  SMP_TexCache_Enabled( void );

int SMP_TexCache_Load( const smpTexCacheKey_t *key,
						unsigned char **blocks, int *bytes,
						void *(*alloc)( int ), void (*dealloc)( void * ),
						smpTexCacheResult_t *res );

int SMP_TexCache_Store( const smpTexCacheKey_t *key,
						 const unsigned char *blocks, int bytes,
						 smpTexCacheResult_t *res );

void SMP_TexCache_InitEngine( int enabled, int maxBytes );

#endif 
