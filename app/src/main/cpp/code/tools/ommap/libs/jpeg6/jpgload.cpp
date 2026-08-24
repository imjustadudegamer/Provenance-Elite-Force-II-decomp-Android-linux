/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */

    
#include "jpeglib.h"
#include <memory.h>

GLOBAL void LoadJPGBuff(unsigned char *fbuffer, unsigned char **pic, int *width, int *height ) 
{
  struct jpeg_decompress_struct cinfo;

  struct jpeg_error_mgr jerr;
  JSAMPARRAY buffer;		
  int row_stride;		
  unsigned char *out;
  byte  *bbuf;
  int nSize;


  cinfo.err = jpeg_std_error(&jerr);

  jpeg_create_decompress(&cinfo);


  jpeg_stdio_src(&cinfo, fbuffer);


  (void) jpeg_read_header(&cinfo, TRUE);




  (void) jpeg_start_decompress(&cinfo);

  row_stride = cinfo.output_width * cinfo.output_components;

  nSize = cinfo.output_width*cinfo.output_height*cinfo.output_components;
  out = reinterpret_cast<unsigned char*>(malloc(nSize+1));
  memset(out, 0, nSize+1);

  *pic = out;
  *width = cinfo.output_width;
  *height = cinfo.output_height;


  while (cinfo.output_scanline < cinfo.output_height) {
	  bbuf = ((out+(row_stride*cinfo.output_scanline)));
  	buffer = &bbuf;
    (void) jpeg_read_scanlines(&cinfo, buffer, 1);
  }

  {
	  int	i, j;
		byte	*buf;

		buf = *pic;

	  j = cinfo.output_width * cinfo.output_height * 4;
	  for ( i = 3 ; i < j ; i+=4 ) {
		  buf[i] = 255;
	  }
  }


  (void) jpeg_finish_decompress(&cinfo);


  jpeg_destroy_decompress(&cinfo);



}

