#ifndef PROGCACHE_PROGRAM_CACHE_H
#define PROGCACHE_PROGRAM_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif


#define PROGCACHE_PROGCACHE_FILE          "programcache/gles3_programs.bin"

#define PROGCACHE_PROGCACHE_MAGIC         0x47335043u  
#define PROGCACHE_PROGCACHE_ENTRY_MAGIC   0x47335045u  
#define PROGCACHE_PROGCACHE_VERSION       1u
#define PROGCACHE_PROGCACHE_KEY_VERSION   1u

#define PROGCACHE_PROGCACHE_MAX_FP        64
#define PROGCACHE_PROGCACHE_MAX_NAME      32
#define PROGCACHE_PROGCACHE_MAX_FORMATS   8
#define PROGCACHE_PROGCACHE_MAX_ENTRIES   4096
#define PROGCACHE_PROGCACHE_MAX_BYTES     (16u * 1024u * 1024u)
#define PROGCACHE_PROGCACHE_MAX_ENTRY     (1u * 1024u * 1024u)
#define PROGCACHE_PROGCACHE_MAX_ROWS      8

#define PROGCACHE_GL_LINK_STATUS                      0x8B82
#define PROGCACHE_GL_PROGRAM_BINARY_LENGTH            0x8741
#define PROGCACHE_GL_PROGRAM_BINARY_RETRIEVABLE_HINT  0x8257
#define PROGCACHE_GL_ACTIVE_ATTRIBUTES                0x8B89
#define PROGCACHE_GL_ACTIVE_UNIFORMS                  0x8B86
#define PROGCACHE_GL_NUM_PROGRAM_BINARY_FORMATS       0x87FE
#define PROGCACHE_GL_TRUE                             1

#define PROGCACHE_ATTR_COUNT 5

typedef struct {
	void	*ud;						
	long	(*Open)( void *ud, const char *qpath, void **fh );
	int		(*Read)( void *ud, void *fh, void *buf, int len );
	void	(*Close)( void *ud, void *fh );
	int		(*Write)( void *ud, const char *qpath, const void *buf, int len );
	int		(*Microseconds)( void );
} progcacheProgCacheIO_t;

typedef struct {
	int			(*GetError)( void );
	unsigned	(*CreateProgram)( void );
	void		(*DeleteProgram)( unsigned program );
	void		(*GetProgramiv)( unsigned program, unsigned pname, int *params );
	void		(*GetProgramBinary)( unsigned program, int bufSize, int *length,
									unsigned *format, void *binary );
	void		(*ProgramBinary)( unsigned program, unsigned format,
									 const void *binary, int length );
	void		(*ProgramParameteri)( unsigned program, unsigned pname, int value );
	void		(*GetActiveAttrib)( unsigned program, unsigned index, int bufSize,
									   int *length, int *size, unsigned *type, char *name );
	void		(*GetActiveUniform)( unsigned program, unsigned index, int bufSize,
										int *length, int *size, unsigned *type, char *name );
	int			(*GetAttribLocation)( unsigned program, const char *name );
	int			(*GetUniformLocation)( unsigned program, const char *name );
	int			(*GetBinaryFormatCount)( void );
	int			(*GetBinaryFormats)( unsigned *out, int max );
} progcacheProgCacheGl_t;

typedef struct {
	const char	*glVersion;
	const char	*glRenderer;
	const char	*glVendor;
	unsigned	glesMajor;
	unsigned	glesMinor;
	unsigned	sourceHash;		
	unsigned	definesHash;	
} progcacheProgCacheKey_t;

typedef struct {
	int			engaged;		
	int			loaded;			
	int			state;			
	int			loadState;		
	unsigned	entries;		
	unsigned	hits;			
	unsigned	miss;			
	unsigned	bad;			
	unsigned	savedUsec;		
	unsigned	restoreUsec;	
	unsigned	coldUsec;		
	unsigned	wroteBytes;		
	unsigned	fileBytes;		
	unsigned	saveUsec;		
	unsigned	glerr;			
} progcacheProgCacheStats_t;

typedef enum {
	PROGCACHE_PC_OFF = 0,			
	PROGCACHE_PC_UNSUPPORTED,		
	PROGCACHE_PC_NOFILE,				
	PROGCACHE_PC_LOADED,				
	PROGCACHE_PC_FOREIGN,			
	PROGCACHE_PC_CORRUPT,			
	PROGCACHE_PC_WRITEFAIL			
} progcacheProgCacheState_t;


int		PROGCACHE_ProgCache_On( void );

int		PROGCACHE_ProgCache_Now( void );

void	PROGCACHE_ProgCache_Init( const progcacheProgCacheIO_t *io, const progcacheProgCacheGl_t *gl, int mode );

void	PROGCACHE_ProgCache_Load( const progcacheProgCacheKey_t *key );

void	PROGCACHE_ProgCache_Shutdown( void );

void	PROGCACHE_ProgCache_HintLink( unsigned program );

int		PROGCACHE_ProgCache_TryRestore( const char *name, unsigned defines,
								  unsigned vsHash, unsigned fsHash,
								  unsigned *programOut, unsigned *savedUsecOut,
								  unsigned *restoreUsecOut );

void	PROGCACHE_ProgCache_NoteCompiled( const char *name, unsigned defines,
									unsigned vsHash, unsigned fsHash,
									unsigned program, unsigned buildUsec );

int		PROGCACHE_ProgCache_Save( void );

void	PROGCACHE_ProgCache_Format( progcacheProgCacheStats_t *stats, char *out, int outSize );

void	PROGCACHE_ProgCache_Stats( progcacheProgCacheStats_t *out );

unsigned PROGCACHE_ProgCache_Hash( const void *data, int len );
unsigned PROGCACHE_ProgCache_HashStr( const char *s );

unsigned PROGCACHE_ProgCache_HashSeed( void );
unsigned PROGCACHE_ProgCache_Fold( unsigned h, const void *data, int len );
unsigned PROGCACHE_ProgCache_FoldStr( unsigned h, const char *s );

void	PROGCACHE_ProgCache_InitEngine( int mode );

#ifdef __cplusplus
}
#endif
#endif 
