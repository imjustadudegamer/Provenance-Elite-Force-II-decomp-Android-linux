
#include <setjmp.h>

#include "tr_common.h"


#ifdef USE_INTERNAL_JPEG
#  define JPEG_INTERNALS
#endif

#include <jpeglib.h>

#ifndef USE_INTERNAL_JPEG
#  if JPEG_LIB_VERSION < 80 && !defined(MEM_SRCDST_SUPPORTED)
#    error Need system libjpeg >= 80 or jpeg_mem_ support
#  endif
#endif

typedef struct q_jpeg_error_mgr_s
{
  struct jpeg_error_mgr pub;  

  jmp_buf setjmp_buffer;  
} q_jpeg_error_mgr_t;

static void R_JPGErrorExit(j_common_ptr cinfo)
{
  char buffer[JMSG_LENGTH_MAX];

  q_jpeg_error_mgr_t *jerr = (q_jpeg_error_mgr_t *)cinfo->err;
  
  (*cinfo->err->format_message) (cinfo, buffer);

  ri.Printf(PRINT_ALL, "Error: %s", buffer);

  longjmp(jerr->setjmp_buffer, 1);
}

static void R_JPGOutputMessage(j_common_ptr cinfo)
{
  char buffer[JMSG_LENGTH_MAX];
  
  (*cinfo->err->format_message) (cinfo, buffer);
  
  ri.Printf(PRINT_ALL, "%s\n", buffer);
}

void R_LoadJPG(const char *filename, unsigned char **pic, int *width, int *height)
{
  struct jpeg_decompress_struct cinfo = {NULL};
  q_jpeg_error_mgr_t jerr;
  JSAMPARRAY buffer;		
  unsigned int row_stride;	
  unsigned int pixelcount, memcount;
  unsigned int sindex, dindex;
  byte *out;
  int len;
	union {
		byte *b;
		void *v;
	} fbuffer;
  byte  *buf;


  len = ri.FS_ReadFile ( ( char * ) filename, &fbuffer.v);
  if (!fbuffer.b || len < 0) {
	return;
  }


  cinfo.err = jpeg_std_error(&jerr.pub);
  cinfo.err->error_exit = R_JPGErrorExit;
  cinfo.err->output_message = R_JPGOutputMessage;

  if (setjmp(jerr.setjmp_buffer))
  {
    jpeg_destroy_decompress(&cinfo);
    ri.FS_FreeFile(fbuffer.v);

    ri.Printf(PRINT_ALL, ", loading file %s\n", filename);
    return;
  }

  jpeg_create_decompress(&cinfo);


  jpeg_mem_src(&cinfo, fbuffer.b, len);


  (void) jpeg_read_header(&cinfo, TRUE);


  cinfo.out_color_space = JCS_RGB;


  (void) jpeg_start_decompress(&cinfo);


  pixelcount = cinfo.output_width * cinfo.output_height;

  if(!cinfo.output_width || !cinfo.output_height
      || ((pixelcount * 4) / cinfo.output_width) / 4 != cinfo.output_height
      || pixelcount > 0x1FFFFFFF || cinfo.output_components != 3
    )
  {
    ri.FS_FreeFile (fbuffer.v);
    jpeg_destroy_decompress(&cinfo);
  
    ri.Error(ERR_DROP, "LoadJPG: %s has an invalid image format: %dx%d*4=%d, components: %d", filename,
		    cinfo.output_width, cinfo.output_height, pixelcount * 4, cinfo.output_components);
  }

  memcount = pixelcount * 4;
  row_stride = cinfo.output_width * cinfo.output_components;

  out = ri.Malloc(memcount);

  *width = cinfo.output_width;
  *height = cinfo.output_height;


  while (cinfo.output_scanline < cinfo.output_height) {
	buf = ((out+(row_stride*cinfo.output_scanline)));
	buffer = &buf;
    (void) jpeg_read_scanlines(&cinfo, buffer, 1);
  }
  
  buf = out;

  sindex = pixelcount * cinfo.output_components;
  dindex = memcount;

  do
  {	
    buf[--dindex] = 255;
    buf[--dindex] = buf[--sindex];
    buf[--dindex] = buf[--sindex];
    buf[--dindex] = buf[--sindex];
  } while(sindex);

  *pic = out;


  jpeg_finish_decompress(&cinfo);


  jpeg_destroy_decompress(&cinfo);

  ri.FS_FreeFile (fbuffer.v);


}



typedef struct {
  struct jpeg_destination_mgr pub; 

  byte* outfile;		
  int	size;
} my_destination_mgr;

typedef my_destination_mgr * my_dest_ptr;



static void
init_destination (j_compress_ptr cinfo)
{
  my_dest_ptr dest = (my_dest_ptr) cinfo->dest;

  dest->pub.next_output_byte = dest->outfile;
  dest->pub.free_in_buffer = dest->size;
}



static boolean
empty_output_buffer (j_compress_ptr cinfo)
{
  my_dest_ptr dest = (my_dest_ptr) cinfo->dest;
  
  jpeg_destroy_compress(cinfo);
  
  ri.Error(ERR_FATAL, "Output buffer for encoded JPEG image has insufficient size of %d bytes",
           dest->size);

  return FALSE;
}


static void term_destination(j_compress_ptr cinfo)
{
}



static void
jpegDest (j_compress_ptr cinfo, byte* outfile, int size)
{
  my_dest_ptr dest;

  if (cinfo->dest == NULL) {	
    cinfo->dest = (struct jpeg_destination_mgr *)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
				  sizeof(my_destination_mgr));
  }

  dest = (my_dest_ptr) cinfo->dest;
  dest->pub.init_destination = init_destination;
  dest->pub.empty_output_buffer = empty_output_buffer;
  dest->pub.term_destination = term_destination;
  dest->outfile = outfile;
  dest->size = size;
}

size_t RE_SaveJPGToBuffer(byte *buffer, size_t bufSize, int quality,
    int image_width, int image_height, byte *image_buffer, int padding)
{
  struct jpeg_compress_struct cinfo;
  q_jpeg_error_mgr_t jerr;
  JSAMPROW row_pointer[1];	
  my_dest_ptr dest;
  int row_stride;		
  size_t outcount;

  cinfo.err = jpeg_std_error(&jerr.pub);
  cinfo.err->error_exit = R_JPGErrorExit;
  cinfo.err->output_message = R_JPGOutputMessage;

  if (setjmp(jerr.setjmp_buffer))
  {
    jpeg_destroy_compress(&cinfo);

    ri.Printf(PRINT_ALL, "\n");
    return 0;
  }

  jpeg_create_compress(&cinfo);

  jpegDest(&cinfo, buffer, bufSize);

  cinfo.image_width = image_width; 	
  cinfo.image_height = image_height;
  cinfo.input_components = 3;		
  cinfo.in_color_space = JCS_RGB; 	

  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, quality, TRUE );
  if (quality >= 85) {
    cinfo.comp_info[0].h_samp_factor = 1;
    cinfo.comp_info[0].v_samp_factor = 1;
  }

  jpeg_start_compress(&cinfo, TRUE);

  row_stride = image_width * cinfo.input_components + padding; 
  
  while (cinfo.next_scanline < cinfo.image_height) {
    row_pointer[0] = &image_buffer[((cinfo.image_height-1)*row_stride)-cinfo.next_scanline * row_stride];
    (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }

  jpeg_finish_compress(&cinfo);
  
  dest = (my_dest_ptr) cinfo.dest;
  outcount = dest->size - dest->pub.free_in_buffer;
 
  jpeg_destroy_compress(&cinfo);

  return outcount;
}

void RE_SaveJPG(char * filename, int quality, int image_width, int image_height, byte *image_buffer, int padding)
{
  byte *out;
  size_t bufSize;

  bufSize = image_width * image_height * 3;
  out = ri.Hunk_AllocateTempMemory(bufSize);

  bufSize = RE_SaveJPGToBuffer(out, bufSize, quality, image_width, image_height, image_buffer, padding);
  ri.FS_WriteFile(filename, out, bufSize);

  ri.Hunk_FreeTempMemory(out);
}
