


typedef enum {			
	JBUF_PASS_THRU,		
	JBUF_SAVE_SOURCE,	
	JBUF_CRANK_DEST,	
	JBUF_SAVE_AND_PASS	
} J_BUF_MODE;

#define CSTATE_START	100	
#define CSTATE_SCANNING	101	
#define CSTATE_RAW_OK	102	
#define CSTATE_WRCOEFS	103	
#define DSTATE_START	200	
#define DSTATE_INHEADER	201	
#define DSTATE_READY	202	
#define DSTATE_PRELOAD	203	
#define DSTATE_PRESCAN	204	
#define DSTATE_SCANNING	205	
#define DSTATE_RAW_OK	206	
#define DSTATE_BUFIMAGE	207	
#define DSTATE_BUFPOST	208	
#define DSTATE_RDCOEFS	209	
#define DSTATE_STOPPING	210	



struct jpeg_comp_master {
  JMETHOD(void, prepare_for_pass, (j_compress_ptr cinfo));
  JMETHOD(void, pass_startup, (j_compress_ptr cinfo));
  JMETHOD(void, finish_pass, (j_compress_ptr cinfo));

  boolean call_pass_startup;	
  boolean is_last_pass;		
};

struct jpeg_c_main_controller {
  JMETHOD(void, start_pass, (j_compress_ptr cinfo, J_BUF_MODE pass_mode));
  JMETHOD(void, process_data, (j_compress_ptr cinfo,
			       JSAMPARRAY input_buf, JDIMENSION *in_row_ctr,
			       JDIMENSION in_rows_avail));
};

struct jpeg_c_prep_controller {
  JMETHOD(void, start_pass, (j_compress_ptr cinfo, J_BUF_MODE pass_mode));
  JMETHOD(void, pre_process_data, (j_compress_ptr cinfo,
				   JSAMPARRAY input_buf,
				   JDIMENSION *in_row_ctr,
				   JDIMENSION in_rows_avail,
				   JSAMPIMAGE output_buf,
				   JDIMENSION *out_row_group_ctr,
				   JDIMENSION out_row_groups_avail));
};

struct jpeg_c_coef_controller {
  JMETHOD(void, start_pass, (j_compress_ptr cinfo, J_BUF_MODE pass_mode));
  JMETHOD(boolean, compress_data, (j_compress_ptr cinfo,
				   JSAMPIMAGE input_buf));
};

struct jpeg_color_converter {
  JMETHOD(void, start_pass, (j_compress_ptr cinfo));
  JMETHOD(void, color_convert, (j_compress_ptr cinfo,
				JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
				JDIMENSION output_row, int num_rows));
};

struct jpeg_downsampler {
  JMETHOD(void, start_pass, (j_compress_ptr cinfo));
  JMETHOD(void, downsample, (j_compress_ptr cinfo,
			     JSAMPIMAGE input_buf, JDIMENSION in_row_index,
			     JSAMPIMAGE output_buf,
			     JDIMENSION out_row_group_index));

  boolean need_context_rows;	
};

struct jpeg_forward_dct {
  JMETHOD(void, start_pass, (j_compress_ptr cinfo));
  JMETHOD(void, forward_DCT, (j_compress_ptr cinfo,
			      jpeg_component_info * compptr,
			      JSAMPARRAY sample_data, JBLOCKROW coef_blocks,
			      JDIMENSION start_row, JDIMENSION start_col,
			      JDIMENSION num_blocks));
};

struct jpeg_entropy_encoder {
  JMETHOD(void, start_pass, (j_compress_ptr cinfo, boolean gather_statistics));
  JMETHOD(boolean, encode_mcu, (j_compress_ptr cinfo, JBLOCKROW *MCU_data));
  JMETHOD(void, finish_pass, (j_compress_ptr cinfo));
};

struct jpeg_marker_writer {
  JMETHOD(void, write_any_marker, (j_compress_ptr cinfo, int marker,
				   const JOCTET *dataptr, unsigned int datalen));
  JMETHOD(void, write_file_header, (j_compress_ptr cinfo));
  JMETHOD(void, write_frame_header, (j_compress_ptr cinfo));
  JMETHOD(void, write_scan_header, (j_compress_ptr cinfo));
  JMETHOD(void, write_file_trailer, (j_compress_ptr cinfo));
  JMETHOD(void, write_tables_only, (j_compress_ptr cinfo));
};



struct jpeg_decomp_master {
  JMETHOD(void, prepare_for_output_pass, (j_decompress_ptr cinfo));
  JMETHOD(void, finish_output_pass, (j_decompress_ptr cinfo));

  boolean is_dummy_pass;	
};

struct jpeg_input_controller {
  JMETHOD(int, consume_input, (j_decompress_ptr cinfo));
  JMETHOD(void, reset_input_controller, (j_decompress_ptr cinfo));
  JMETHOD(void, start_input_pass, (j_decompress_ptr cinfo));
  JMETHOD(void, finish_input_pass, (j_decompress_ptr cinfo));

  boolean has_multiple_scans;	
  boolean eoi_reached;		
};

struct jpeg_d_main_controller {
  JMETHOD(void, start_pass, (j_decompress_ptr cinfo, J_BUF_MODE pass_mode));
  JMETHOD(void, process_data, (j_decompress_ptr cinfo,
			       JSAMPARRAY output_buf, JDIMENSION *out_row_ctr,
			       JDIMENSION out_rows_avail));
};

struct jpeg_d_coef_controller {
  JMETHOD(void, start_input_pass, (j_decompress_ptr cinfo));
  JMETHOD(int, consume_data, (j_decompress_ptr cinfo));
  JMETHOD(void, start_output_pass, (j_decompress_ptr cinfo));
  JMETHOD(int, decompress_data, (j_decompress_ptr cinfo,
				 JSAMPIMAGE output_buf));
  jvirt_barray_ptr *coef_arrays;
};

struct jpeg_d_post_controller {
  JMETHOD(void, start_pass, (j_decompress_ptr cinfo, J_BUF_MODE pass_mode));
  JMETHOD(void, post_process_data, (j_decompress_ptr cinfo,
				    JSAMPIMAGE input_buf,
				    JDIMENSION *in_row_group_ctr,
				    JDIMENSION in_row_groups_avail,
				    JSAMPARRAY output_buf,
				    JDIMENSION *out_row_ctr,
				    JDIMENSION out_rows_avail));
};

struct jpeg_marker_reader {
  JMETHOD(void, reset_marker_reader, (j_decompress_ptr cinfo));
  JMETHOD(int, read_markers, (j_decompress_ptr cinfo));
  jpeg_marker_parser_method read_restart_marker;
  jpeg_marker_parser_method process_COM;
  jpeg_marker_parser_method process_APPn[16];

  boolean saw_SOI;		
  boolean saw_SOF;		
  int next_restart_num;		
  unsigned int discarded_bytes;	
};

struct jpeg_entropy_decoder {
  JMETHOD(void, start_pass, (j_decompress_ptr cinfo));
  JMETHOD(boolean, decode_mcu, (j_decompress_ptr cinfo,
				JBLOCKROW *MCU_data));
};

typedef JMETHOD(void, inverse_DCT_method_ptr,
		(j_decompress_ptr cinfo, jpeg_component_info * compptr,
		 JCOEFPTR coef_block,
		 JSAMPARRAY output_buf, JDIMENSION output_col));

struct jpeg_inverse_dct {
  JMETHOD(void, start_pass, (j_decompress_ptr cinfo));
  inverse_DCT_method_ptr inverse_DCT[MAX_COMPONENTS];
};

struct jpeg_upsampler {
  JMETHOD(void, start_pass, (j_decompress_ptr cinfo));
  JMETHOD(void, upsample, (j_decompress_ptr cinfo,
			   JSAMPIMAGE input_buf,
			   JDIMENSION *in_row_group_ctr,
			   JDIMENSION in_row_groups_avail,
			   JSAMPARRAY output_buf,
			   JDIMENSION *out_row_ctr,
			   JDIMENSION out_rows_avail));

  boolean need_context_rows;	
};

struct jpeg_color_deconverter {
  JMETHOD(void, start_pass, (j_decompress_ptr cinfo));
  JMETHOD(void, color_convert, (j_decompress_ptr cinfo,
				JSAMPIMAGE input_buf, JDIMENSION input_row,
				JSAMPARRAY output_buf, int num_rows));
};

struct jpeg_color_quantizer {
  JMETHOD(void, start_pass, (j_decompress_ptr cinfo, boolean is_pre_scan));
  JMETHOD(void, color_quantize, (j_decompress_ptr cinfo,
				 JSAMPARRAY input_buf, JSAMPARRAY output_buf,
				 int num_rows));
  JMETHOD(void, finish_pass, (j_decompress_ptr cinfo));
  JMETHOD(void, new_color_map, (j_decompress_ptr cinfo));
};



#undef MAX
#define MAX(a,b)	((a) > (b) ? (a) : (b))
#undef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))



#ifdef RIGHT_SHIFT_IS_UNSIGNED
#define SHIFT_TEMPS	INT32 shift_temp;
#define RIGHT_SHIFT(x,shft)  \
	((shift_temp = (x)) < 0 ? \
	 (shift_temp >> (shft)) | ((~((INT32) 0)) << (32-(shft))) : \
	 (shift_temp >> (shft)))
#else
#define SHIFT_TEMPS
#define RIGHT_SHIFT(x,shft)	((x) >> (shft))
#endif



#ifdef NEED_SHORT_EXTERNAL_NAMES
#define jinit_compress_master	jICompress
#define jinit_c_master_control	jICMaster
#define jinit_c_main_controller	jICMainC
#define jinit_c_prep_controller	jICPrepC
#define jinit_c_coef_controller	jICCoefC
#define jinit_color_converter	jICColor
#define jinit_downsampler	jIDownsampler
#define jinit_forward_dct	jIFDCT
#define jinit_huff_encoder	jIHEncoder
#define jinit_phuff_encoder	jIPHEncoder
#define jinit_marker_writer	jIMWriter
#define jinit_master_decompress	jIDMaster
#define jinit_d_main_controller	jIDMainC
#define jinit_d_coef_controller	jIDCoefC
#define jinit_d_post_controller	jIDPostC
#define jinit_input_controller	jIInCtlr
#define jinit_marker_reader	jIMReader
#define jinit_huff_decoder	jIHDecoder
#define jinit_phuff_decoder	jIPHDecoder
#define jinit_inverse_dct	jIIDCT
#define jinit_upsampler		jIUpsampler
#define jinit_color_deconverter	jIDColor
#define jinit_1pass_quantizer	jI1Quant
#define jinit_2pass_quantizer	jI2Quant
#define jinit_merged_upsampler	jIMUpsampler
#define jinit_memory_mgr	jIMemMgr
#define jdiv_round_up		jDivRound
#define jround_up		jRound
#define jcopy_sample_rows	jCopySamples
#define jcopy_block_row		jCopyBlocks
#define jzero_far		jZeroFar
#define jpeg_zigzag_order	jZIGTable
#define jpeg_natural_order	jZAGTable
#endif 


EXTERN void jinit_compress_master JPP((j_compress_ptr cinfo));
EXTERN void jinit_c_master_control JPP((j_compress_ptr cinfo,
					boolean transcode_only));
EXTERN void jinit_c_main_controller JPP((j_compress_ptr cinfo,
					 boolean need_full_buffer));
EXTERN void jinit_c_prep_controller JPP((j_compress_ptr cinfo,
					 boolean need_full_buffer));
EXTERN void jinit_c_coef_controller JPP((j_compress_ptr cinfo,
					 boolean need_full_buffer));
EXTERN void jinit_color_converter JPP((j_compress_ptr cinfo));
EXTERN void jinit_downsampler JPP((j_compress_ptr cinfo));
EXTERN void jinit_forward_dct JPP((j_compress_ptr cinfo));
EXTERN void jinit_huff_encoder JPP((j_compress_ptr cinfo));
EXTERN void jinit_phuff_encoder JPP((j_compress_ptr cinfo));
EXTERN void jinit_marker_writer JPP((j_compress_ptr cinfo));
EXTERN void jinit_master_decompress JPP((j_decompress_ptr cinfo));
EXTERN void jinit_d_main_controller JPP((j_decompress_ptr cinfo,
					 boolean need_full_buffer));
EXTERN void jinit_d_coef_controller JPP((j_decompress_ptr cinfo,
					 boolean need_full_buffer));
EXTERN void jinit_d_post_controller JPP((j_decompress_ptr cinfo,
					 boolean need_full_buffer));
EXTERN void jinit_input_controller JPP((j_decompress_ptr cinfo));
EXTERN void jinit_marker_reader JPP((j_decompress_ptr cinfo));
EXTERN void jinit_huff_decoder JPP((j_decompress_ptr cinfo));
EXTERN void jinit_phuff_decoder JPP((j_decompress_ptr cinfo));
EXTERN void jinit_inverse_dct JPP((j_decompress_ptr cinfo));
EXTERN void jinit_upsampler JPP((j_decompress_ptr cinfo));
EXTERN void jinit_color_deconverter JPP((j_decompress_ptr cinfo));
EXTERN void jinit_1pass_quantizer JPP((j_decompress_ptr cinfo));
EXTERN void jinit_2pass_quantizer JPP((j_decompress_ptr cinfo));
EXTERN void jinit_merged_upsampler JPP((j_decompress_ptr cinfo));
EXTERN void jinit_memory_mgr JPP((j_common_ptr cinfo));

EXTERN long jdiv_round_up JPP((long a, long b));
EXTERN long jround_up JPP((long a, long b));
EXTERN void jcopy_sample_rows JPP((JSAMPARRAY input_array, int source_row,
				   JSAMPARRAY output_array, int dest_row,
				   int num_rows, JDIMENSION num_cols));
EXTERN void jcopy_block_row JPP((JBLOCKROW input_row, JBLOCKROW output_row,
				 JDIMENSION num_blocks));
EXTERN void jzero_far JPP((void FAR * target, size_t bytestozero));
extern const int jpeg_zigzag_order[]; 
extern const int jpeg_natural_order[]; 


#ifdef INCOMPLETE_TYPES_BROKEN
#ifndef AM_MEMORY_MANAGER	
struct jvirt_sarray_control { long dummy; };
struct jvirt_barray_control { long dummy; };
#endif
#endif 
