#ifndef R201_TEXCACHE_H
#define R201_TEXCACHE_H

#define R201_TEXCACHE_MAGIC			0x43543252u
#define R201_TEXCACHE_HDR_FIXED		100	

#define R201_TEXCACHE_ENC_VERSION	1u

#define R201_TEXCACHE_NAME_MAX		128
#define R201_TEXCACHE_DIR			"texcache"
#define R201_TEXCACHE_EXT			"r2tc"
#define R201_TEXCACHE_PATH_MAX		160
#define R201_TEXCACHE_DEFAULT_MAX_MB	96
#define R201_TEXCACHE_MAX_ENTRIES	8192

typedef void *r201TexCacheFile_t;

typedef struct r201TexCacheIO_s {
	long (*openRead)(void *ud, const char *qpath, r201TexCacheFile_t *fh);
	int  (*read)(void *ud, r201TexCacheFile_t fh, void *buf, int len);
	void (*closeRead)(void *ud, r201TexCacheFile_t fh);
	int  (*writeAll)(void *ud, const char *qpath, const void *buf, int len);
	int  (*removeFile)(void *ud, const char *qpath);
	char **(*list)(void *ud, int *count);
	void (*freeList)(void *ud, char **names);
	void *ud;
} r201TexCacheIO_t;

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
	char			name[R201_TEXCACHE_NAME_MAX];
} r201TexCacheKey_t;

typedef enum {
	R201_TEXCACHE_STAT_DISABLED = 0,
	R201_TEXCACHE_STAT_HIT,
	R201_TEXCACHE_STAT_MISS,		
	R201_TEXCACHE_STAT_CORRUPT,		
	R201_TEXCACHE_STAT_STORED,		
	R201_TEXCACHE_STAT_WRITE_FAIL,
	R201_TEXCACHE_STAT_SKIP			
} r201TexCacheStat_t;

typedef struct {
	r201TexCacheStat_t	stat;
	int					bytes;		
	int					evicted;	
	char				path[R201_TEXCACHE_PATH_MAX];
} r201TexCacheResult_t;


void R201_TexCache_Hash64( const void *data, int bytes, unsigned int *lo, unsigned int *hi );

int R201_TexCache_StoredBytes( int width, int height, int iMipmapsAvailable, int srcDxt );

void R201_TexCache_SourceHash( const unsigned char *dxt, int width, int height,
							   int iMipmapsAvailable, int srcDxt,
							   unsigned int *lo, unsigned int *hi, int *storedBytes );

void R201_TexCache_FillKey( r201TexCacheKey_t *key, const char *name,
							int width, int height, int numMipmaps, int picmip,
							int maxTextureSize, int srcDxt, int target,
							int base, int upW, int upH, int levels,
							unsigned int srcHashLo, unsigned int srcHashHi,
							int srcBytes );

void R201_TexCache_Path( const r201TexCacheKey_t *key, char *out, int outSize );


void R201_TexCache_Begin( const r201TexCacheIO_t *io, int enabled, int maxBytes );
void R201_TexCache_Shutdown( void );
int  R201_TexCache_Enabled( void );

int R201_TexCache_Load( const r201TexCacheKey_t *key,
						unsigned char **blocks, int *bytes,
						void *(*alloc)( int ), void (*dealloc)( void * ),
						r201TexCacheResult_t *res );

int R201_TexCache_Store( const r201TexCacheKey_t *key,
						 const unsigned char *blocks, int bytes,
						 r201TexCacheResult_t *res );

void R201_TexCache_InitEngine( int enabled, int maxBytes );

#endif 
