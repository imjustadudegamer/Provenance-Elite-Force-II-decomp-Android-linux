/*
 * jpeglib.h
 *
 * Copyright (C) 1991-1995, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file defines the application interface for the JPEG library.
 * Most applications using the library need only include this file,
 * and perhaps jerror.h if they want to know the exact error codes.
 */

#ifndef JPEGLIB_H
#define JPEGLIB_H

#ifdef __cplusplus
extern "C"
{
#endif



#ifdef __MACOS__

#define	boolean	qboolean
#define	register
#define	INT32	int

#endif


#ifdef _WIN32
#include "windows.h"
#include "stdio.h"
#endif

#ifndef INT32
#define	INT32	int
#endif

extern void LoadJPGBuff(unsigned char *fbuffer, unsigned char **pic, int *width, int *height );



#ifndef JCONFIG_INCLUDED	
#include "jpeg6/jconfig.h"		
#endif
#include "jpeg6/jmorecfg.h"		



#define JPEG_LIB_VERSION  60	



#define DCTSIZE		    8	
#define DCTSIZE2	    64	
#define NUM_QUANT_TBLS      4	
#define NUM_HUFF_TBLS       4	
#define NUM_ARITH_TBLS      16	
#define MAX_COMPS_IN_SCAN   4	
#define MAX_SAMP_FACTOR     4	
#define C_MAX_BLOCKS_IN_MCU   10 
#ifndef D_MAX_BLOCKS_IN_MCU
#define D_MAX_BLOCKS_IN_MCU   10 
#endif



#ifdef HAVE_PROTOTYPES
#define JMETHOD(type,methodname,arglist)  type (*methodname) arglist
#else
#define JMETHOD(type,methodname,arglist)  type (*methodname) ()
#endif



typedef JSAMPLE FAR *JSAMPROW;	
typedef JSAMPROW *JSAMPARRAY;	
typedef JSAMPARRAY *JSAMPIMAGE;	

typedef JCOEF JBLOCK[DCTSIZE2];	
typedef JBLOCK FAR *JBLOCKROW;	
typedef JBLOCKROW *JBLOCKARRAY;		
typedef JBLOCKARRAY *JBLOCKIMAGE;	

typedef JCOEF FAR *JCOEFPTR;	





typedef struct {
  UINT16 quantval[DCTSIZE2];	
  boolean sent_table;		
} JQUANT_TBL;



typedef struct {
  UINT8 bits[17];		
  UINT8 huffval[256];		
  boolean sent_table;		
} JHUFF_TBL;



typedef struct {
  int component_id;		
  int component_index;		
  int h_samp_factor;		
  int v_samp_factor;		
  int quant_tbl_no;		
  int dc_tbl_no;		
  int ac_tbl_no;		
  
  
  JDIMENSION width_in_blocks;
  JDIMENSION height_in_blocks;
  int DCT_scaled_size;
  JDIMENSION downsampled_width;	 
  JDIMENSION downsampled_height; 
  boolean component_needed;	

  int MCU_width;		
  int MCU_height;		
  int MCU_blocks;		
  int MCU_sample_width;		
  int last_col_width;		
  int last_row_height;		

  JQUANT_TBL * quant_table;

  void * dct_table;
} jpeg_component_info;



typedef struct {
  int comps_in_scan;		
  int component_index[MAX_COMPS_IN_SCAN]; 
  int Ss, Se;			
  int Ah, Al;			
} jpeg_scan_info;



typedef enum {
	JCS_UNKNOWN,		
	JCS_GRAYSCALE,		
	JCS_RGB,		
	JCS_YCbCr,		
	JCS_CMYK,		
	JCS_YCCK		
} J_COLOR_SPACE;


typedef enum {
	JDCT_ISLOW,		
	JDCT_IFAST,		
	JDCT_FLOAT		
} J_DCT_METHOD;

#ifndef JDCT_DEFAULT		
#define JDCT_DEFAULT  JDCT_ISLOW
#endif
#ifndef JDCT_FASTEST		
#define JDCT_FASTEST  JDCT_IFAST
#endif


typedef enum {
	JDITHER_NONE,		
	JDITHER_ORDERED,	
	JDITHER_FS		
} J_DITHER_MODE;



#define jpeg_common_fields \
  struct jpeg_error_mgr * err;	\
  struct jpeg_memory_mgr * mem;	\
  struct jpeg_progress_mgr * progress; \
  boolean is_decompressor;	\
  int global_state		

struct jpeg_common_struct {
  jpeg_common_fields;		
};

typedef struct jpeg_common_struct * j_common_ptr;
typedef struct jpeg_compress_struct * j_compress_ptr;
typedef struct jpeg_decompress_struct * j_decompress_ptr;



struct jpeg_compress_struct {
  jpeg_common_fields;		

  struct jpeg_destination_mgr * dest;


  JDIMENSION image_width;	
  JDIMENSION image_height;	
  int input_components;		
  J_COLOR_SPACE in_color_space;	

  double input_gamma;		


  int data_precision;		

  int num_components;		
  J_COLOR_SPACE jpeg_color_space; 

  jpeg_component_info * comp_info;
  
  JQUANT_TBL * quant_tbl_ptrs[NUM_QUANT_TBLS];
  
  JHUFF_TBL * dc_huff_tbl_ptrs[NUM_HUFF_TBLS];
  JHUFF_TBL * ac_huff_tbl_ptrs[NUM_HUFF_TBLS];
  
  UINT8 arith_dc_L[NUM_ARITH_TBLS]; 
  UINT8 arith_dc_U[NUM_ARITH_TBLS]; 
  UINT8 arith_ac_K[NUM_ARITH_TBLS]; 

  int num_scans;		
  const jpeg_scan_info * scan_info; 

  boolean raw_data_in;		
  boolean arith_code;		
  boolean optimize_coding;	
  boolean CCIR601_sampling;	
  int smoothing_factor;		
  J_DCT_METHOD dct_method;	

  unsigned int restart_interval; 
  int restart_in_rows;		


  boolean write_JFIF_header;	
  UINT8 density_unit;		
  UINT16 X_density;		
  UINT16 Y_density;		
  boolean write_Adobe_marker;	
  

  JDIMENSION next_scanline;	


  boolean progressive_mode;	
  int max_h_samp_factor;	
  int max_v_samp_factor;	

  JDIMENSION total_iMCU_rows;	
  
  int comps_in_scan;		
  jpeg_component_info * cur_comp_info[MAX_COMPS_IN_SCAN];
  
  JDIMENSION MCUs_per_row;	
  JDIMENSION MCU_rows_in_scan;	
  
  int blocks_in_MCU;		
  int MCU_membership[C_MAX_BLOCKS_IN_MCU];

  int Ss, Se, Ah, Al;		

  struct jpeg_comp_master * master;
  struct jpeg_c_main_controller * main;
  struct jpeg_c_prep_controller * prep;
  struct jpeg_c_coef_controller * coef;
  struct jpeg_marker_writer * marker;
  struct jpeg_color_converter * cconvert;
  struct jpeg_downsampler * downsample;
  struct jpeg_forward_dct * fdct;
  struct jpeg_entropy_encoder * entropy;
};



struct jpeg_decompress_struct {
  jpeg_common_fields;		

  struct jpeg_source_mgr * src;


  JDIMENSION image_width;	
  JDIMENSION image_height;	
  int num_components;		
  J_COLOR_SPACE jpeg_color_space; 


  J_COLOR_SPACE out_color_space; 

  unsigned int scale_num, scale_denom; 

  double output_gamma;		

  boolean buffered_image;	
  boolean raw_data_out;		

  J_DCT_METHOD dct_method;	
  boolean do_fancy_upsampling;	
  boolean do_block_smoothing;	

  boolean quantize_colors;	
  J_DITHER_MODE dither_mode;	
  boolean two_pass_quantize;	
  int desired_number_of_colors;	
  boolean enable_1pass_quant;	
  boolean enable_external_quant;
  boolean enable_2pass_quant;	


  JDIMENSION output_width;	
  JDIMENSION output_height;	
  int out_color_components;	
  int output_components;	
  int rec_outbuf_height;	

  int actual_number_of_colors;	
  JSAMPARRAY colormap;		


  JDIMENSION output_scanline;	

  int input_scan_number;	
  JDIMENSION input_iMCU_row;	

  int output_scan_number;	
  JDIMENSION output_iMCU_row;	

  int (*coef_bits)[DCTSIZE2];	



  JQUANT_TBL * quant_tbl_ptrs[NUM_QUANT_TBLS];

  JHUFF_TBL * dc_huff_tbl_ptrs[NUM_HUFF_TBLS];
  JHUFF_TBL * ac_huff_tbl_ptrs[NUM_HUFF_TBLS];


  int data_precision;		

  jpeg_component_info * comp_info;

  boolean progressive_mode;	
  boolean arith_code;		

  UINT8 arith_dc_L[NUM_ARITH_TBLS]; 
  UINT8 arith_dc_U[NUM_ARITH_TBLS]; 
  UINT8 arith_ac_K[NUM_ARITH_TBLS]; 

  unsigned int restart_interval; 

  boolean saw_JFIF_marker;	
  UINT8 density_unit;		
  UINT16 X_density;		
  UINT16 Y_density;		
  boolean saw_Adobe_marker;	
  UINT8 Adobe_transform;	

  boolean CCIR601_sampling;	


  int max_h_samp_factor;	
  int max_v_samp_factor;	

  int min_DCT_scaled_size;	

  JDIMENSION total_iMCU_rows;	

  JSAMPLE * sample_range_limit; 

  int comps_in_scan;		
  jpeg_component_info * cur_comp_info[MAX_COMPS_IN_SCAN];

  JDIMENSION MCUs_per_row;	
  JDIMENSION MCU_rows_in_scan;	

  int blocks_in_MCU;		
  int MCU_membership[D_MAX_BLOCKS_IN_MCU];

  int Ss, Se, Ah, Al;		

  int unread_marker;

  struct jpeg_decomp_master * master;
  struct jpeg_d_main_controller * main;
  struct jpeg_d_coef_controller * coef;
  struct jpeg_d_post_controller * post;
  struct jpeg_input_controller * inputctl;
  struct jpeg_marker_reader * marker;
  struct jpeg_entropy_decoder * entropy;
  struct jpeg_inverse_dct * idct;
  struct jpeg_upsampler * upsample;
  struct jpeg_color_deconverter * cconvert;
  struct jpeg_color_quantizer * cquantize;
};





struct jpeg_error_mgr {
  JMETHOD(void, error_exit, (j_common_ptr cinfo));
  JMETHOD(void, emit_message, (j_common_ptr cinfo, int msg_level));
  JMETHOD(void, output_message, (j_common_ptr cinfo));
  JMETHOD(void, format_message, (j_common_ptr cinfo, char * buffer));
#define JMSG_LENGTH_MAX  200	
  JMETHOD(void, reset_error_mgr, (j_common_ptr cinfo));
  
  int msg_code;
#define JMSG_STR_PARM_MAX  80
  union {
    int i[8];
    char s[JMSG_STR_PARM_MAX];
  } msg_parm;
  
  
  int trace_level;		
  
  long num_warnings;		

  const char * const * jpeg_message_table; 
  int last_jpeg_message;    
  const char * const * addon_message_table; 
  int first_addon_message;	
  int last_addon_message;	
};



struct jpeg_progress_mgr {
  JMETHOD(void, progress_monitor, (j_common_ptr cinfo));

  long pass_counter;		
  long pass_limit;		
  int completed_passes;		
  int total_passes;		
};



struct jpeg_destination_mgr {
  JOCTET * next_output_byte;	
  size_t free_in_buffer;	

  JMETHOD(void, init_destination, (j_compress_ptr cinfo));
  JMETHOD(boolean, empty_output_buffer, (j_compress_ptr cinfo));
  JMETHOD(void, term_destination, (j_compress_ptr cinfo));
};



struct jpeg_source_mgr {
  const JOCTET * next_input_byte; 
  size_t bytes_in_buffer;	

  JMETHOD(void, init_source, (j_decompress_ptr cinfo));
  JMETHOD(boolean, fill_input_buffer, (j_decompress_ptr cinfo));
  JMETHOD(void, skip_input_data, (j_decompress_ptr cinfo, long num_bytes));
  JMETHOD(boolean, resync_to_restart, (j_decompress_ptr cinfo, int desired));
  JMETHOD(void, term_source, (j_decompress_ptr cinfo));
};



#define JPOOL_PERMANENT	0	
#define JPOOL_IMAGE	1	
#define JPOOL_NUMPOOLS	2

typedef struct jvirt_sarray_control * jvirt_sarray_ptr;
typedef struct jvirt_barray_control * jvirt_barray_ptr;


struct jpeg_memory_mgr {
  JMETHOD(void *, alloc_small, (j_common_ptr cinfo, int pool_id,
				size_t sizeofobject));
  JMETHOD(void FAR *, alloc_large, (j_common_ptr cinfo, int pool_id,
				     size_t sizeofobject));
  JMETHOD(JSAMPARRAY, alloc_sarray, (j_common_ptr cinfo, int pool_id,
				     JDIMENSION samplesperrow,
				     JDIMENSION numrows));
  JMETHOD(JBLOCKARRAY, alloc_barray, (j_common_ptr cinfo, int pool_id,
				      JDIMENSION blocksperrow,
				      JDIMENSION numrows));
  JMETHOD(jvirt_sarray_ptr, request_virt_sarray, (j_common_ptr cinfo,
						  int pool_id,
						  boolean pre_zero,
						  JDIMENSION samplesperrow,
						  JDIMENSION numrows,
						  JDIMENSION maxaccess));
  JMETHOD(jvirt_barray_ptr, request_virt_barray, (j_common_ptr cinfo,
						  int pool_id,
						  boolean pre_zero,
						  JDIMENSION blocksperrow,
						  JDIMENSION numrows,
						  JDIMENSION maxaccess));
  JMETHOD(void, realize_virt_arrays, (j_common_ptr cinfo));
  JMETHOD(JSAMPARRAY, access_virt_sarray, (j_common_ptr cinfo,
					   jvirt_sarray_ptr ptr,
					   JDIMENSION start_row,
					   JDIMENSION num_rows,
					   boolean writable));
  JMETHOD(JBLOCKARRAY, access_virt_barray, (j_common_ptr cinfo,
					    jvirt_barray_ptr ptr,
					    JDIMENSION start_row,
					    JDIMENSION num_rows,
					    boolean writable));
  JMETHOD(void, free_pool, (j_common_ptr cinfo, int pool_id));
  JMETHOD(void, self_destruct, (j_common_ptr cinfo));

  long max_memory_to_use;
};


typedef JMETHOD(boolean, jpeg_marker_parser_method, (j_decompress_ptr cinfo));



#ifdef HAVE_PROTOTYPES
#define JPP(arglist)	arglist
#else
#define JPP(arglist)	()
#endif



#ifdef NEED_SHORT_EXTERNAL_NAMES
#define jpeg_std_error		jStdError
#define jpeg_create_compress	jCreaCompress
#define jpeg_create_decompress	jCreaDecompress
#define jpeg_destroy_compress	jDestCompress
#define jpeg_destroy_decompress	jDestDecompress
#define jpeg_stdio_dest		jStdDest
#define jpeg_stdio_src		jStdSrc
#define jpeg_set_defaults	jSetDefaults
#define jpeg_set_colorspace	jSetColorspace
#define jpeg_default_colorspace	jDefColorspace
#define jpeg_set_quality	jSetQuality
#define jpeg_set_linear_quality	jSetLQuality
#define jpeg_add_quant_table	jAddQuantTable
#define jpeg_quality_scaling	jQualityScaling
#define jpeg_simple_progression	jSimProgress
#define jpeg_suppress_tables	jSuppressTables
#define jpeg_alloc_quant_table	jAlcQTable
#define jpeg_alloc_huff_table	jAlcHTable
#define jpeg_start_compress	jStrtCompress
#define jpeg_write_scanlines	jWrtScanlines
#define jpeg_finish_compress	jFinCompress
#define jpeg_write_raw_data	jWrtRawData
#define jpeg_write_marker	jWrtMarker
#define jpeg_write_tables	jWrtTables
#define jpeg_read_header	jReadHeader
#define jpeg_start_decompress	jStrtDecompress
#define jpeg_read_scanlines	jReadScanlines
#define jpeg_finish_decompress	jFinDecompress
#define jpeg_read_raw_data	jReadRawData
#define jpeg_has_multiple_scans	jHasMultScn
#define jpeg_start_output	jStrtOutput
#define jpeg_finish_output	jFinOutput
#define jpeg_input_complete	jInComplete
#define jpeg_new_colormap	jNewCMap
#define jpeg_consume_input	jConsumeInput
#define jpeg_calc_output_dimensions	jCalcDimensions
#define jpeg_set_marker_processor	jSetMarker
#define jpeg_read_coefficients	jReadCoefs
#define jpeg_write_coefficients	jWrtCoefs
#define jpeg_copy_critical_parameters	jCopyCrit
#define jpeg_abort_compress	jAbrtCompress
#define jpeg_abort_decompress	jAbrtDecompress
#define jpeg_abort		jAbort
#define jpeg_destroy		jDestroy
#define jpeg_resync_to_restart	jResyncRestart
#endif 


EXTERN struct jpeg_error_mgr *jpeg_std_error JPP((struct jpeg_error_mgr *err));

EXTERN void jpeg_create_compress JPP((j_compress_ptr cinfo));
EXTERN void jpeg_create_decompress JPP((j_decompress_ptr cinfo));
EXTERN void jpeg_destroy_compress JPP((j_compress_ptr cinfo));
EXTERN void jpeg_destroy_decompress JPP((j_decompress_ptr cinfo));

EXTERN void jpeg_stdio_dest JPP((j_compress_ptr cinfo, FILE * outfile));
EXTERN void jpeg_stdio_src JPP((j_decompress_ptr cinfo, unsigned char *infile));

EXTERN void jpeg_set_defaults JPP((j_compress_ptr cinfo));
EXTERN void jpeg_set_colorspace JPP((j_compress_ptr cinfo,
				     J_COLOR_SPACE colorspace));
EXTERN void jpeg_default_colorspace JPP((j_compress_ptr cinfo));
EXTERN void jpeg_set_quality JPP((j_compress_ptr cinfo, int quality,
				  boolean force_baseline));
EXTERN void jpeg_set_linear_quality JPP((j_compress_ptr cinfo,
					 int scale_factor,
					 boolean force_baseline));
EXTERN void jpeg_add_quant_table JPP((j_compress_ptr cinfo, int which_tbl,
				      const unsigned int *basic_table,
				      int scale_factor,
				      boolean force_baseline));
EXTERN int jpeg_quality_scaling JPP((int quality));
EXTERN void jpeg_simple_progression JPP((j_compress_ptr cinfo));
EXTERN void jpeg_suppress_tables JPP((j_compress_ptr cinfo,
				      boolean suppress));
EXTERN JQUANT_TBL * jpeg_alloc_quant_table JPP((j_common_ptr cinfo));
EXTERN JHUFF_TBL * jpeg_alloc_huff_table JPP((j_common_ptr cinfo));

EXTERN void jpeg_start_compress JPP((j_compress_ptr cinfo,
				     boolean write_all_tables));
EXTERN JDIMENSION jpeg_write_scanlines JPP((j_compress_ptr cinfo,
					    JSAMPARRAY scanlines,
					    JDIMENSION num_lines));
EXTERN void jpeg_finish_compress JPP((j_compress_ptr cinfo));

EXTERN JDIMENSION jpeg_write_raw_data JPP((j_compress_ptr cinfo,
					   JSAMPIMAGE data,
					   JDIMENSION num_lines));

EXTERN void jpeg_write_marker JPP((j_compress_ptr cinfo, int marker,
				   const JOCTET *dataptr, unsigned int datalen));

EXTERN void jpeg_write_tables JPP((j_compress_ptr cinfo));

EXTERN int jpeg_read_header JPP((j_decompress_ptr cinfo,
				 boolean require_image));
#define JPEG_SUSPENDED		0 
#define JPEG_HEADER_OK		1 
#define JPEG_HEADER_TABLES_ONLY	2 

EXTERN boolean jpeg_start_decompress JPP((j_decompress_ptr cinfo));
EXTERN JDIMENSION jpeg_read_scanlines JPP((j_decompress_ptr cinfo,
					   JSAMPARRAY scanlines,
					   JDIMENSION max_lines));
EXTERN boolean jpeg_finish_decompress JPP((j_decompress_ptr cinfo));

EXTERN JDIMENSION jpeg_read_raw_data JPP((j_decompress_ptr cinfo,
					  JSAMPIMAGE data,
					  JDIMENSION max_lines));

EXTERN boolean jpeg_has_multiple_scans JPP((j_decompress_ptr cinfo));
EXTERN boolean jpeg_start_output JPP((j_decompress_ptr cinfo,
				      int scan_number));
EXTERN boolean jpeg_finish_output JPP((j_decompress_ptr cinfo));
EXTERN boolean jpeg_input_complete JPP((j_decompress_ptr cinfo));
EXTERN void jpeg_new_colormap JPP((j_decompress_ptr cinfo));
EXTERN int jpeg_consume_input JPP((j_decompress_ptr cinfo));
#define JPEG_REACHED_SOS	1 
#define JPEG_REACHED_EOI	2 
#define JPEG_ROW_COMPLETED	3 
#define JPEG_SCAN_COMPLETED	4 

EXTERN void jpeg_calc_output_dimensions JPP((j_decompress_ptr cinfo));

EXTERN void jpeg_set_marker_processor JPP((j_decompress_ptr cinfo,
					   int marker_code,
					   jpeg_marker_parser_method routine));

EXTERN jvirt_barray_ptr * jpeg_read_coefficients JPP((j_decompress_ptr cinfo));
EXTERN void jpeg_write_coefficients JPP((j_compress_ptr cinfo,
					 jvirt_barray_ptr * coef_arrays));
EXTERN void jpeg_copy_critical_parameters JPP((j_decompress_ptr srcinfo,
					       j_compress_ptr dstinfo));

EXTERN void jpeg_abort_compress JPP((j_compress_ptr cinfo));
EXTERN void jpeg_abort_decompress JPP((j_decompress_ptr cinfo));

EXTERN void jpeg_abort JPP((j_common_ptr cinfo));
EXTERN void jpeg_destroy JPP((j_common_ptr cinfo));

EXTERN boolean jpeg_resync_to_restart JPP((j_decompress_ptr cinfo,
					   int desired));



#define JPEG_RST0	0xD0	
#define JPEG_EOI	0xD9	
#define JPEG_APP0	0xE0	
#define JPEG_COM	0xFE	



#ifdef INCOMPLETE_TYPES_BROKEN
#ifndef JPEG_INTERNALS		
struct jvirt_sarray_control { long dummy; };
struct jvirt_barray_control { long dummy; };
struct jpeg_comp_master { long dummy; };
struct jpeg_c_main_controller { long dummy; };
struct jpeg_c_prep_controller { long dummy; };
struct jpeg_c_coef_controller { long dummy; };
struct jpeg_marker_writer { long dummy; };
struct jpeg_color_converter { long dummy; };
struct jpeg_downsampler { long dummy; };
struct jpeg_forward_dct { long dummy; };
struct jpeg_entropy_encoder { long dummy; };
struct jpeg_decomp_master { long dummy; };
struct jpeg_d_main_controller { long dummy; };
struct jpeg_d_coef_controller { long dummy; };
struct jpeg_d_post_controller { long dummy; };
struct jpeg_input_controller { long dummy; };
struct jpeg_marker_reader { long dummy; };
struct jpeg_entropy_decoder { long dummy; };
struct jpeg_inverse_dct { long dummy; };
struct jpeg_upsampler { long dummy; };
struct jpeg_color_deconverter { long dummy; };
struct jpeg_color_quantizer { long dummy; };
#endif 
#endif 



#ifdef JPEG_INTERNALS
#include "jpegint.h"		
#include "jerror.h"		
#endif

#ifdef __cplusplus
}
#endif

#endif 
