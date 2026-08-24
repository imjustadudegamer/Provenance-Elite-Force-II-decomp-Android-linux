/*
 * jdpostct.c
 *
 * Copyright (C) 1994-1995, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains the decompression postprocessing controller.
 * This controller manages the upsampling, color conversion, and color
 * quantization/reduction steps; specifically, it controls the buffering
 * between upsample/color conversion and color quantization/reduction.
 *
 * If no color quantization/reduction is required, then this module has no
 * work to do, and it just hands off to the upsample/color conversion code.
 * An integrated upsample/convert/quantize process would replace this module
 * entirely.
 */

#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"



typedef struct {
  struct jpeg_d_post_controller pub; 

  jvirt_sarray_ptr whole_image;	
  JSAMPARRAY buffer;		
  JDIMENSION strip_height;	
  JDIMENSION starting_row;	
  JDIMENSION next_row;		
} my_post_controller;

typedef my_post_controller * my_post_ptr;


METHODDEF void post_process_1pass
	JPP((j_decompress_ptr cinfo,
	     JSAMPIMAGE input_buf, JDIMENSION *in_row_group_ctr,
	     JDIMENSION in_row_groups_avail,
	     JSAMPARRAY output_buf, JDIMENSION *out_row_ctr,
	     JDIMENSION out_rows_avail));
#ifdef QUANT_2PASS_SUPPORTED
METHODDEF void post_process_prepass
	JPP((j_decompress_ptr cinfo,
	     JSAMPIMAGE input_buf, JDIMENSION *in_row_group_ctr,
	     JDIMENSION in_row_groups_avail,
	     JSAMPARRAY output_buf, JDIMENSION *out_row_ctr,
	     JDIMENSION out_rows_avail));
METHODDEF void post_process_2pass
	JPP((j_decompress_ptr cinfo,
	     JSAMPIMAGE input_buf, JDIMENSION *in_row_group_ctr,
	     JDIMENSION in_row_groups_avail,
	     JSAMPARRAY output_buf, JDIMENSION *out_row_ctr,
	     JDIMENSION out_rows_avail));
#endif



METHODDEF void
start_pass_dpost (j_decompress_ptr cinfo, J_BUF_MODE pass_mode)
{
  my_post_ptr post = (my_post_ptr) cinfo->post;

  switch (pass_mode) {
  case JBUF_PASS_THRU:
    if (cinfo->quantize_colors) {
      post->pub.post_process_data = post_process_1pass;
      if (post->buffer == NULL) {
	post->buffer = (*cinfo->mem->access_virt_sarray)
	  ((j_common_ptr) cinfo, post->whole_image,
	   (JDIMENSION) 0, post->strip_height, TRUE);
      }
    } else {
      post->pub.post_process_data = cinfo->upsample->upsample;
    }
    break;
#ifdef QUANT_2PASS_SUPPORTED
  case JBUF_SAVE_AND_PASS:
    if (post->whole_image == NULL)
      ERREXIT(cinfo, JERR_BAD_BUFFER_MODE);
    post->pub.post_process_data = post_process_prepass;
    break;
  case JBUF_CRANK_DEST:
    if (post->whole_image == NULL)
      ERREXIT(cinfo, JERR_BAD_BUFFER_MODE);
    post->pub.post_process_data = post_process_2pass;
    break;
#endif 
  default:
    ERREXIT(cinfo, JERR_BAD_BUFFER_MODE);
    break;
  }
  post->starting_row = post->next_row = 0;
}



METHODDEF void
post_process_1pass (j_decompress_ptr cinfo,
		    JSAMPIMAGE input_buf, JDIMENSION *in_row_group_ctr,
		    JDIMENSION in_row_groups_avail,
		    JSAMPARRAY output_buf, JDIMENSION *out_row_ctr,
		    JDIMENSION out_rows_avail)
{
  my_post_ptr post = (my_post_ptr) cinfo->post;
  JDIMENSION num_rows, max_rows;

  max_rows = out_rows_avail - *out_row_ctr;
  if (max_rows > post->strip_height)
    max_rows = post->strip_height;
  num_rows = 0;
  (*cinfo->upsample->upsample) (cinfo,
		input_buf, in_row_group_ctr, in_row_groups_avail,
		post->buffer, &num_rows, max_rows);
  (*cinfo->cquantize->color_quantize) (cinfo,
		post->buffer, output_buf + *out_row_ctr, (int) num_rows);
  *out_row_ctr += num_rows;
}


#ifdef QUANT_2PASS_SUPPORTED


METHODDEF void
post_process_prepass (j_decompress_ptr cinfo,
		      JSAMPIMAGE input_buf, JDIMENSION *in_row_group_ctr,
		      JDIMENSION in_row_groups_avail,
		      JSAMPARRAY output_buf, JDIMENSION *out_row_ctr,
		      JDIMENSION out_rows_avail)
{
  my_post_ptr post = (my_post_ptr) cinfo->post;
  JDIMENSION old_next_row, num_rows;

  if (post->next_row == 0) {
    post->buffer = (*cinfo->mem->access_virt_sarray)
	((j_common_ptr) cinfo, post->whole_image,
	 post->starting_row, post->strip_height, TRUE);
  }

  old_next_row = post->next_row;
  (*cinfo->upsample->upsample) (cinfo,
		input_buf, in_row_group_ctr, in_row_groups_avail,
		post->buffer, &post->next_row, post->strip_height);

  if (post->next_row > old_next_row) {
    num_rows = post->next_row - old_next_row;
    (*cinfo->cquantize->color_quantize) (cinfo, post->buffer + old_next_row,
					 (JSAMPARRAY) NULL, (int) num_rows);
    *out_row_ctr += num_rows;
  }

  if (post->next_row >= post->strip_height) {
    post->starting_row += post->strip_height;
    post->next_row = 0;
  }
}



METHODDEF void
post_process_2pass (j_decompress_ptr cinfo,
		    JSAMPIMAGE input_buf, JDIMENSION *in_row_group_ctr,
		    JDIMENSION in_row_groups_avail,
		    JSAMPARRAY output_buf, JDIMENSION *out_row_ctr,
		    JDIMENSION out_rows_avail)
{
  my_post_ptr post = (my_post_ptr) cinfo->post;
  JDIMENSION num_rows, max_rows;

  if (post->next_row == 0) {
    post->buffer = (*cinfo->mem->access_virt_sarray)
	((j_common_ptr) cinfo, post->whole_image,
	 post->starting_row, post->strip_height, FALSE);
  }

  num_rows = post->strip_height - post->next_row; 
  max_rows = out_rows_avail - *out_row_ctr; 
  if (num_rows > max_rows)
    num_rows = max_rows;
  max_rows = cinfo->output_height - post->starting_row;
  if (num_rows > max_rows)
    num_rows = max_rows;

  (*cinfo->cquantize->color_quantize) (cinfo,
		post->buffer + post->next_row, output_buf + *out_row_ctr,
		(int) num_rows);
  *out_row_ctr += num_rows;

  post->next_row += num_rows;
  if (post->next_row >= post->strip_height) {
    post->starting_row += post->strip_height;
    post->next_row = 0;
  }
}

#endif 



GLOBAL void
jinit_d_post_controller (j_decompress_ptr cinfo, boolean need_full_buffer)
{
  my_post_ptr post;

  post = (my_post_ptr)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				SIZEOF(my_post_controller));
  cinfo->post = (struct jpeg_d_post_controller *) post;
  post->pub.start_pass = start_pass_dpost;
  post->whole_image = NULL;	
  post->buffer = NULL;		

  if (cinfo->quantize_colors) {
    post->strip_height = (JDIMENSION) cinfo->max_v_samp_factor;
    if (need_full_buffer) {
#ifdef QUANT_2PASS_SUPPORTED
      post->whole_image = (*cinfo->mem->request_virt_sarray)
	((j_common_ptr) cinfo, JPOOL_IMAGE, FALSE,
	 cinfo->output_width * cinfo->out_color_components,
	 (JDIMENSION) jround_up((long) cinfo->output_height,
				(long) post->strip_height),
	 post->strip_height);
#else
      ERREXIT(cinfo, JERR_BAD_BUFFER_MODE);
#endif 
    } else {
      post->buffer = (*cinfo->mem->alloc_sarray)
	((j_common_ptr) cinfo, JPOOL_IMAGE,
	 cinfo->output_width * cinfo->out_color_components,
	 post->strip_height);
    }
  }
}
