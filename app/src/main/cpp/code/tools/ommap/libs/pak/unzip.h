
#if defined(STRICTUNZIP) || defined(STRICTZIPUNZIP)
typedef struct TagunzFile__ { int unused; } unzFile__; 
typedef unzFile__ *unzFile;
#else
typedef void* unzFile;
#endif


typedef struct tm_unz_s 
{
	unsigned int tm_sec;            
	unsigned int tm_min;            
	unsigned int tm_hour;           
	unsigned int tm_mday;           
	unsigned int tm_mon;            
	unsigned int tm_year;           
} tm_unz;

typedef struct unz_global_info_s
{
	unsigned long number_entry;         
	unsigned long size_comment;         
} unz_global_info;


typedef struct unz_file_info_s
{
    unsigned long version;              
    unsigned long version_needed;       
    unsigned long flag;                 
    unsigned long compression_method;   
    unsigned long dosDate;              
    unsigned long crc;                  
    unsigned long compressed_size;       
    unsigned long uncompressed_size;     
    unsigned long size_filename;        
    unsigned long size_file_extra;      
    unsigned long size_file_comment;    

    unsigned long disk_num_start;       
    unsigned long internal_fa;          
    unsigned long external_fa;          

    tm_unz tmu_date;
} unz_file_info;

typedef struct unz_file_info_internal_s
{
    unsigned long offset_curfile;
} unz_file_info_internal;

typedef void* (*alloc_func) (void* opaque, unsigned int items, unsigned int size);
typedef void   (*free_func) (void* opaque, void* address);

struct internal_state;

typedef struct z_stream_s {
    unsigned char    *next_in;  
    unsigned int     avail_in;  
    unsigned long    total_in;  

    unsigned char    *next_out; 
    unsigned int     avail_out; 
    unsigned long    total_out; 

    char     *msg;      
    struct internal_state *state; 

    alloc_func zalloc;  
    free_func  zfree;   
    unsigned char*     opaque;  

    int     data_type;  
    unsigned long   adler;      
    unsigned long   reserved;   
} z_stream;

typedef z_stream *z_streamp;


typedef struct
{
	char  *read_buffer;         
	z_stream stream;            

	unsigned long pos_in_zipfile;       
	unsigned long stream_initialised;   

	unsigned long offset_local_extrafield;
	unsigned int  size_local_extrafield;
	unsigned long pos_local_extrafield;   

	unsigned long crc32;                
	unsigned long crc32_wait;           
	unsigned long rest_read_compressed; 
	unsigned long rest_read_uncompressed;
	FILE* file;                 
	unsigned long compression_method;   
	unsigned long byte_before_the_zipfile;
} file_in_zip_read_info_s;


typedef struct
{
	FILE* file;                 
	unz_global_info gi;       
	unsigned long byte_before_the_zipfile;
	unsigned long num_file;             
	unsigned long pos_in_central_dir;   
	unsigned long current_file_ok;      
	unsigned long central_pos;          

	unsigned long size_central_dir;     
	unsigned long offset_central_dir;   

	unz_file_info cur_file_info; 
	unz_file_info_internal cur_file_info_internal; 
    file_in_zip_read_info_s* pfile_in_zip_read; 
} unz_s;

#define UNZ_OK                                  (0)
#define UNZ_END_OF_LIST_OF_FILE (-100)
#define UNZ_ERRNO               (Z_ERRNO)
#define UNZ_EOF                 (0)
#define UNZ_PARAMERROR                  (-102)
#define UNZ_BADZIPFILE                  (-103)
#define UNZ_INTERNALERROR               (-104)
#define UNZ_CRCERROR                    (-105)

#define UNZ_CASESENSITIVE		1
#define UNZ_NOTCASESENSITIVE	2
#define UNZ_OSDEFAULTCASE		0

extern int unzStringFileNameCompare (const char* fileName1, const char* fileName2, int iCaseSensitivity);


extern unzFile unzOpen (const char *path);
extern unzFile unzReOpen (const char* path, unzFile file);


extern int unzClose (unzFile file);


extern int unzGetGlobalInfo (unzFile file, unz_global_info *pglobal_info);



extern int unzGetGlobalComment (unzFile file, char *szComment, unsigned long uSizeBuf);




extern int unzGoToFirstFile (unzFile file);


extern int unzGoToNextFile (unzFile file);


extern int unzLocateFile (unzFile file, const char *szFileName, int iCaseSensitivity);



extern int unzGetCurrentFileInfo (unzFile file, unz_file_info *pfile_info, char *szFileName, unsigned long fileNameBufferSize, void *extraField, unsigned long extraFieldBufferSize, char *szComment, unsigned long commentBufferSize);



extern int unzOpenCurrentFile (unzFile file);


extern int unzCloseCurrentFile (unzFile file);


												
extern int unzReadCurrentFile (unzFile file, void* buf, unsigned len);


extern long unztell(unzFile file);


extern int unzeof (unzFile file);


extern int unzGetLocalExtrafield (unzFile file, void* buf, unsigned len);

