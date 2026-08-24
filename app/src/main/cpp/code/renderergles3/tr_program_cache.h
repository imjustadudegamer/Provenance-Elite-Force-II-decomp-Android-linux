#ifndef R207_PROGRAM_CACHE_H
#define R207_PROGRAM_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif


#define R207_PROGCACHE_FILE          "programcache/gles3_programs.bin"

#define R207_PROGCACHE_MAGIC         0x47335043u  
#define R207_PROGCACHE_ENTRY_MAGIC   0x47335045u  
#define R207_PROGCACHE_VERSION       1u
#define R207_PROGCACHE_KEY_VERSION   1u

#define R207_PROGCACHE_MAX_FP        64
#define R207_PROGCACHE_MAX_NAME      32
#define R207_PROGCACHE_MAX_FORMATS   8
#define R207_PROGCACHE_MAX_ENTRIES   4096
#define R207_PROGCACHE_MAX_BYTES     (16u * 1024u * 1024u)
#define R207_PROGCACHE_MAX_ENTRY     (1u * 1024u * 1024u)
#define R207_PROGCACHE_MAX_ROWS      8

#define R207_GL_LINK_STATUS                      0x8B82
#define R207_GL_PROGRAM_BINARY_LENGTH            0x8741
#define R207_GL_PROGRAM_BINARY_RETRIEVABLE_HINT  0x8257
#define R207_GL_ACTIVE_ATTRIBUTES                0x8B89
#define R207_GL_ACTIVE_UNIFORMS                  0x8B86
#define R207_GL_NUM_PROGRAM_BINARY_FORMATS       0x87FE
#define R207_GL_TRUE                             1

#define R207_ATTR_COUNT 5

typedef struct {
	void	*ud;						
	long	(*Open)( void *ud, const char *qpath, void **fh );
	int		(*Read)( void *ud, void *fh, void *buf, int len );
	void	(*Close)( void *ud, void *fh );
	int		(*Write)( void *ud, const char *qpath, const void *buf, int len );
	int		(*Microseconds)( void );
} r207ProgCacheIO_t;

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
} r207ProgCacheGl_t;

typedef struct {
	const char	*glVersion;
	const char	*glRenderer;
	const char	*glVendor;
	unsigned	glesMajor;
	unsigned	glesMinor;
	unsigned	sourceHash;		
	unsigned	definesHash;	
} r207ProgCacheKey_t;

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
} r207ProgCacheStats_t;

typedef enum {
	R207_PC_OFF = 0,			
	R207_PC_UNSUPPORTED,		
	R207_PC_NOFILE,				
	R207_PC_LOADED,				
	R207_PC_FOREIGN,			
	R207_PC_CORRUPT,			
	R207_PC_WRITEFAIL			
} r207ProgCacheState_t;


int		R207_ProgCache_On( void );

int		R207_ProgCache_Now( void );

void	R207_ProgCache_Init( const r207ProgCacheIO_t *io, const r207ProgCacheGl_t *gl, int mode );

void	R207_ProgCache_Load( const r207ProgCacheKey_t *key );

void	R207_ProgCache_Shutdown( void );

void	R207_ProgCache_HintLink( unsigned program );

int		R207_ProgCache_TryRestore( const char *name, unsigned defines,
								  unsigned vsHash, unsigned fsHash,
								  unsigned *programOut, unsigned *savedUsecOut,
								  unsigned *restoreUsecOut );

void	R207_ProgCache_NoteCompiled( const char *name, unsigned defines,
									unsigned vsHash, unsigned fsHash,
									unsigned program, unsigned buildUsec );

int		R207_ProgCache_Save( void );

void	R207_ProgCache_Format( r207ProgCacheStats_t *stats, char *out, int outSize );

void	R207_ProgCache_Stats( r207ProgCacheStats_t *out );

unsigned R207_ProgCache_Hash( const void *data, int len );
unsigned R207_ProgCache_HashStr( const char *s );

unsigned R207_ProgCache_HashSeed( void );
unsigned R207_ProgCache_Fold( unsigned h, const void *data, int len );
unsigned R207_ProgCache_FoldStr( unsigned h, const char *s );

void	R207_ProgCache_InitEngine( int mode );

#ifdef __cplusplus
}
#endif
#endif 
