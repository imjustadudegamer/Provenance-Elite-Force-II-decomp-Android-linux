/*
 * jmemsys.h
 *
 * Copyright (C) 1992-1994, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This include file defines the interface between the system-independent
 * and system-dependent portions of the JPEG memory manager.  No other
 * modules need include it.  (The system-independent portion is jmemmgr.c;
 * there are several different versions of the system-dependent portion.)
 *
 * This file works as-is for the system-dependent memory managers supplied
 * in the IJG distribution.  You may need to modify it if you write a
 * custom memory manager.  If system-dependent changes are needed in
 * this file, the best method is to #ifdef them based on a configuration
 * symbol supplied in jconfig.h, as we have done with USE_MSDOS_MEMMGR.
 */



#ifdef NEED_SHORT_EXTERNAL_NAMES
#define jpeg_get_small		jGetSmall
#define jpeg_free_small		jFreeSmall
#define jpeg_get_large		jGetLarge
#define jpeg_free_large		jFreeLarge
#define jpeg_mem_available	jMemAvail
#define jpeg_open_backing_store	jOpenBackStore
#define jpeg_mem_init		jMemInit
#define jpeg_mem_term		jMemTerm
#endif 



EXTERN void * jpeg_get_small JPP((j_common_ptr cinfo, size_t sizeofobject));
EXTERN void jpeg_free_small JPP((j_common_ptr cinfo, void * object,
				 size_t sizeofobject));


EXTERN void FAR * jpeg_get_large JPP((j_common_ptr cinfo,size_t sizeofobject));
EXTERN void jpeg_free_large JPP((j_common_ptr cinfo, void FAR * object,
				 size_t sizeofobject));


#ifndef MAX_ALLOC_CHUNK		
#define MAX_ALLOC_CHUNK  1000000000L
#endif


EXTERN long jpeg_mem_available JPP((j_common_ptr cinfo,
				    long min_bytes_needed,
				    long max_bytes_needed,
				    long already_allocated));



#define TEMP_NAME_LENGTH   64	

#ifdef USE_MSDOS_MEMMGR		

typedef unsigned short XMSH;	
typedef unsigned short EMSH;	

typedef union {
  short file_handle;		
  XMSH xms_handle;		
  EMSH ems_handle;		
} handle_union;

#endif 

typedef struct backing_store_struct * backing_store_ptr;

typedef struct backing_store_struct {
  JMETHOD(void, read_backing_store, (j_common_ptr cinfo,
				     backing_store_ptr info,
				     void FAR * buffer_address,
				     long file_offset, long byte_count));
  JMETHOD(void, write_backing_store, (j_common_ptr cinfo,
				      backing_store_ptr info,
				      void FAR * buffer_address,
				      long file_offset, long byte_count));
  JMETHOD(void, close_backing_store, (j_common_ptr cinfo,
				      backing_store_ptr info));

#ifdef USE_MSDOS_MEMMGR
  handle_union handle;		
  char temp_name[TEMP_NAME_LENGTH]; 
#else
  FILE * temp_file;		
  char temp_name[TEMP_NAME_LENGTH]; 
#endif
} backing_store_info;


EXTERN void jpeg_open_backing_store JPP((j_common_ptr cinfo,
					 backing_store_ptr info,
					 long total_bytes_needed));



EXTERN long jpeg_mem_init JPP((j_common_ptr cinfo));
EXTERN void jpeg_mem_term JPP((j_common_ptr cinfo));
