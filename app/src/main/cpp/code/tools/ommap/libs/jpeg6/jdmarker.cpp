
#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"


typedef enum {			
  M_SOF0  = 0xc0,
  M_SOF1  = 0xc1,
  M_SOF2  = 0xc2,
  M_SOF3  = 0xc3,
  
  M_SOF5  = 0xc5,
  M_SOF6  = 0xc6,
  M_SOF7  = 0xc7,
  
  M_JPG   = 0xc8,
  M_SOF9  = 0xc9,
  M_SOF10 = 0xca,
  M_SOF11 = 0xcb,
  
  M_SOF13 = 0xcd,
  M_SOF14 = 0xce,
  M_SOF15 = 0xcf,
  
  M_DHT   = 0xc4,
  
  M_DAC   = 0xcc,
  
  M_RST0  = 0xd0,
  M_RST1  = 0xd1,
  M_RST2  = 0xd2,
  M_RST3  = 0xd3,
  M_RST4  = 0xd4,
  M_RST5  = 0xd5,
  M_RST6  = 0xd6,
  M_RST7  = 0xd7,
  
  M_SOI   = 0xd8,
  M_EOI   = 0xd9,
  M_SOS   = 0xda,
  M_DQT   = 0xdb,
  M_DNL   = 0xdc,
  M_DRI   = 0xdd,
  M_DHP   = 0xde,
  M_EXP   = 0xdf,
  
  M_APP0  = 0xe0,
  M_APP1  = 0xe1,
  M_APP2  = 0xe2,
  M_APP3  = 0xe3,
  M_APP4  = 0xe4,
  M_APP5  = 0xe5,
  M_APP6  = 0xe6,
  M_APP7  = 0xe7,
  M_APP8  = 0xe8,
  M_APP9  = 0xe9,
  M_APP10 = 0xea,
  M_APP11 = 0xeb,
  M_APP12 = 0xec,
  M_APP13 = 0xed,
  M_APP14 = 0xee,
  M_APP15 = 0xef,
  
  M_JPG0  = 0xf0,
  M_JPG13 = 0xfd,
  M_COM   = 0xfe,
  
  M_TEM   = 0x01,
  
  M_ERROR = 0x100
} JPEG_MARKER;



#define INPUT_VARS(cinfo)  \
	struct jpeg_source_mgr * datasrc = (cinfo)->src;  \
	const JOCTET * next_input_byte = datasrc->next_input_byte;  \
	size_t bytes_in_buffer = datasrc->bytes_in_buffer

#define INPUT_SYNC(cinfo)  \
	( datasrc->next_input_byte = next_input_byte,  \
	  datasrc->bytes_in_buffer = bytes_in_buffer )

#define INPUT_RELOAD(cinfo)  \
	( next_input_byte = datasrc->next_input_byte,  \
	  bytes_in_buffer = datasrc->bytes_in_buffer )

#define MAKE_BYTE_AVAIL(cinfo,action)  \
	if (bytes_in_buffer == 0) {  \
	  if (! (*datasrc->fill_input_buffer) (cinfo))  \
	    { action; }  \
	  INPUT_RELOAD(cinfo);  \
	}  \
	bytes_in_buffer--

#define INPUT_BYTE(cinfo,V,action)  \
	MAKESTMT( MAKE_BYTE_AVAIL(cinfo,action); \
		  V = GETJOCTET(*next_input_byte++); )

#define INPUT_2BYTES(cinfo,V,action)  \
	MAKESTMT( MAKE_BYTE_AVAIL(cinfo,action); \
		  V = ((unsigned int) GETJOCTET(*next_input_byte++)) << 8; \
		  MAKE_BYTE_AVAIL(cinfo,action); \
		  V += GETJOCTET(*next_input_byte++); )




LOCAL boolean
get_soi (j_decompress_ptr cinfo)
{
  int i;
  
  TRACEMS(cinfo, 1, JTRC_SOI);

  if (cinfo->marker->saw_SOI)
    ERREXIT(cinfo, JERR_SOI_DUPLICATE);


  for (i = 0; i < NUM_ARITH_TBLS; i++) {
    cinfo->arith_dc_L[i] = 0;
    cinfo->arith_dc_U[i] = 1;
    cinfo->arith_ac_K[i] = 5;
  }
  cinfo->restart_interval = 0;


  cinfo->jpeg_color_space = JCS_UNKNOWN;
  cinfo->CCIR601_sampling = FALSE; 

  cinfo->saw_JFIF_marker = FALSE;
  cinfo->density_unit = 0;	
  cinfo->X_density = 1;
  cinfo->Y_density = 1;
  cinfo->saw_Adobe_marker = FALSE;
  cinfo->Adobe_transform = 0;

  cinfo->marker->saw_SOI = TRUE;

  return TRUE;
}


LOCAL boolean
get_sof (j_decompress_ptr cinfo, boolean is_prog, boolean is_arith)
{
  INT32 length;
  int c, ci;
  jpeg_component_info * compptr;
  INPUT_VARS(cinfo);

  cinfo->progressive_mode = is_prog;
  cinfo->arith_code = is_arith;

  INPUT_2BYTES(cinfo, length, return FALSE);

  INPUT_BYTE(cinfo, cinfo->data_precision, return FALSE);
  INPUT_2BYTES(cinfo, cinfo->image_height, return FALSE);
  INPUT_2BYTES(cinfo, cinfo->image_width, return FALSE);
  INPUT_BYTE(cinfo, cinfo->num_components, return FALSE);

  length -= 8;

  TRACEMS4(cinfo, 1, JTRC_SOF, cinfo->unread_marker,
	   (int) cinfo->image_width, (int) cinfo->image_height,
	   cinfo->num_components);

  if (cinfo->marker->saw_SOF)
    ERREXIT(cinfo, JERR_SOF_DUPLICATE);

  if (cinfo->image_height <= 0 || cinfo->image_width <= 0
      || cinfo->num_components <= 0)
    ERREXIT(cinfo, JERR_EMPTY_IMAGE);

  if (length != (cinfo->num_components * 3))
    ERREXIT(cinfo, JERR_BAD_LENGTH);

  if (cinfo->comp_info == NULL)	
    cinfo->comp_info = (jpeg_component_info *) (*cinfo->mem->alloc_small)
			((j_common_ptr) cinfo, JPOOL_IMAGE,
			 cinfo->num_components * SIZEOF(jpeg_component_info));
  
  for (ci = 0, compptr = cinfo->comp_info; ci < cinfo->num_components;
       ci++, compptr++) {
    compptr->component_index = ci;
    INPUT_BYTE(cinfo, compptr->component_id, return FALSE);
    INPUT_BYTE(cinfo, c, return FALSE);
    compptr->h_samp_factor = (c >> 4) & 15;
    compptr->v_samp_factor = (c     ) & 15;
    INPUT_BYTE(cinfo, compptr->quant_tbl_no, return FALSE);

    TRACEMS4(cinfo, 1, JTRC_SOF_COMPONENT,
	     compptr->component_id, compptr->h_samp_factor,
	     compptr->v_samp_factor, compptr->quant_tbl_no);
  }

  cinfo->marker->saw_SOF = TRUE;

  INPUT_SYNC(cinfo);
  return TRUE;
}


LOCAL boolean
get_sos (j_decompress_ptr cinfo)
{
  INT32 length;
  int i, ci, n, c, cc;
  jpeg_component_info * compptr;
  INPUT_VARS(cinfo);

  if (! cinfo->marker->saw_SOF)
    ERREXIT(cinfo, JERR_SOS_NO_SOF);

  INPUT_2BYTES(cinfo, length, return FALSE);

  INPUT_BYTE(cinfo, n, return FALSE); 

  if (length != (n * 2 + 6) || n < 1 || n > MAX_COMPS_IN_SCAN)
    ERREXIT(cinfo, JERR_BAD_LENGTH);

  TRACEMS1(cinfo, 1, JTRC_SOS, n);

  cinfo->comps_in_scan = n;


  for (i = 0; i < n; i++) {
    INPUT_BYTE(cinfo, cc, return FALSE);
    INPUT_BYTE(cinfo, c, return FALSE);
    
    for (ci = 0, compptr = cinfo->comp_info; ci < cinfo->num_components;
	 ci++, compptr++) {
      if (cc == compptr->component_id)
	goto id_found;
    }

    ERREXIT1(cinfo, JERR_BAD_COMPONENT_ID, cc);

  id_found:

    cinfo->cur_comp_info[i] = compptr;
    compptr->dc_tbl_no = (c >> 4) & 15;
    compptr->ac_tbl_no = (c     ) & 15;
    
    TRACEMS3(cinfo, 1, JTRC_SOS_COMPONENT, cc,
	     compptr->dc_tbl_no, compptr->ac_tbl_no);
  }

  INPUT_BYTE(cinfo, c, return FALSE);
  cinfo->Ss = c;
  INPUT_BYTE(cinfo, c, return FALSE);
  cinfo->Se = c;
  INPUT_BYTE(cinfo, c, return FALSE);
  cinfo->Ah = (c >> 4) & 15;
  cinfo->Al = (c     ) & 15;

  TRACEMS4(cinfo, 1, JTRC_SOS_PARAMS, cinfo->Ss, cinfo->Se,
	   cinfo->Ah, cinfo->Al);

  cinfo->marker->next_restart_num = 0;

  cinfo->input_scan_number++;

  INPUT_SYNC(cinfo);
  return TRUE;
}


METHODDEF boolean
get_app0 (j_decompress_ptr cinfo)
{
#define JFIF_LEN 14
  INT32 length;
  UINT8 b[JFIF_LEN];
  int buffp;
  INPUT_VARS(cinfo);

  INPUT_2BYTES(cinfo, length, return FALSE);
  length -= 2;


  if (length >= JFIF_LEN) {
    for (buffp = 0; buffp < JFIF_LEN; buffp++)
      INPUT_BYTE(cinfo, b[buffp], return FALSE);
    length -= JFIF_LEN;

    if (b[0]==0x4A && b[1]==0x46 && b[2]==0x49 && b[3]==0x46 && b[4]==0) {
      if (b[5] != 1)
	WARNMS2(cinfo, JWRN_JFIF_MAJOR, b[5], b[6]);
      else if (b[6] > 2)
	TRACEMS2(cinfo, 1, JTRC_JFIF_MINOR, b[5], b[6]);
      cinfo->saw_JFIF_marker = TRUE;
      cinfo->density_unit = b[7];
      cinfo->X_density = (b[8] << 8) + b[9];
      cinfo->Y_density = (b[10] << 8) + b[11];
      TRACEMS3(cinfo, 1, JTRC_JFIF,
	       cinfo->X_density, cinfo->Y_density, cinfo->density_unit);
      if (b[12] | b[13])
	TRACEMS2(cinfo, 1, JTRC_JFIF_THUMBNAIL, b[12], b[13]);
      if (length != ((INT32) b[12] * (INT32) b[13] * (INT32) 3))
	TRACEMS1(cinfo, 1, JTRC_JFIF_BADTHUMBNAILSIZE, (int) length);
    } else {
      TRACEMS1(cinfo, 1, JTRC_APP0, (int) length + JFIF_LEN);
    }
  } else {
    TRACEMS1(cinfo, 1, JTRC_APP0, (int) length);
  }

  INPUT_SYNC(cinfo);
  if (length > 0)		
    (*cinfo->src->skip_input_data) (cinfo, (long) length);

  return TRUE;
}


METHODDEF boolean
get_app14 (j_decompress_ptr cinfo)
{
#define ADOBE_LEN 12
  INT32 length;
  UINT8 b[ADOBE_LEN];
  int buffp;
  unsigned int version, flags0, flags1, transform;
  INPUT_VARS(cinfo);

  INPUT_2BYTES(cinfo, length, return FALSE);
  length -= 2;


  if (length >= ADOBE_LEN) {
    for (buffp = 0; buffp < ADOBE_LEN; buffp++)
      INPUT_BYTE(cinfo, b[buffp], return FALSE);
    length -= ADOBE_LEN;

    if (b[0]==0x41 && b[1]==0x64 && b[2]==0x6F && b[3]==0x62 && b[4]==0x65) {
      version = (b[5] << 8) + b[6];
      flags0 = (b[7] << 8) + b[8];
      flags1 = (b[9] << 8) + b[10];
      transform = b[11];
      TRACEMS4(cinfo, 1, JTRC_ADOBE, version, flags0, flags1, transform);
      cinfo->saw_Adobe_marker = TRUE;
      cinfo->Adobe_transform = (UINT8) transform;
    } else {
      TRACEMS1(cinfo, 1, JTRC_APP14, (int) length + ADOBE_LEN);
    }
  } else {
    TRACEMS1(cinfo, 1, JTRC_APP14, (int) length);
  }

  INPUT_SYNC(cinfo);
  if (length > 0)		
    (*cinfo->src->skip_input_data) (cinfo, (long) length);

  return TRUE;
}


LOCAL boolean
get_dac (j_decompress_ptr cinfo)
{
  INT32 length;
  int index, val;
  INPUT_VARS(cinfo);

  INPUT_2BYTES(cinfo, length, return FALSE);
  length -= 2;
  
  while (length > 0) {
    INPUT_BYTE(cinfo, index, return FALSE);
    INPUT_BYTE(cinfo, val, return FALSE);

    length -= 2;

    TRACEMS2(cinfo, 1, JTRC_DAC, index, val);

    if (index < 0 || index >= (2*NUM_ARITH_TBLS))
      ERREXIT1(cinfo, JERR_DAC_INDEX, index);

    if (index >= NUM_ARITH_TBLS) { 
      cinfo->arith_ac_K[index-NUM_ARITH_TBLS] = (UINT8) val;
    } else {			
      cinfo->arith_dc_L[index] = (UINT8) (val & 0x0F);
      cinfo->arith_dc_U[index] = (UINT8) (val >> 4);
      if (cinfo->arith_dc_L[index] > cinfo->arith_dc_U[index])
	ERREXIT1(cinfo, JERR_DAC_VALUE, val);
    }
  }

  INPUT_SYNC(cinfo);
  return TRUE;
}


LOCAL boolean
get_dht (j_decompress_ptr cinfo)
{
  INT32 length;
  UINT8 bits[17];
  UINT8 huffval[256];
  int i, index, count;
  JHUFF_TBL **htblptr;
  INPUT_VARS(cinfo);

  INPUT_2BYTES(cinfo, length, return FALSE);
  length -= 2;
  
  while (length > 0) {
    INPUT_BYTE(cinfo, index, return FALSE);

    TRACEMS1(cinfo, 1, JTRC_DHT, index);
      
    bits[0] = 0;
    count = 0;
    for (i = 1; i <= 16; i++) {
      INPUT_BYTE(cinfo, bits[i], return FALSE);
      count += bits[i];
    }

    length -= 1 + 16;

    TRACEMS8(cinfo, 2, JTRC_HUFFBITS,
	     bits[1], bits[2], bits[3], bits[4],
	     bits[5], bits[6], bits[7], bits[8]);
    TRACEMS8(cinfo, 2, JTRC_HUFFBITS,
	     bits[9], bits[10], bits[11], bits[12],
	     bits[13], bits[14], bits[15], bits[16]);

    if (count > 256 || ((INT32) count) > length)
      ERREXIT(cinfo, JERR_DHT_COUNTS);

    for (i = 0; i < count; i++)
      INPUT_BYTE(cinfo, huffval[i], return FALSE);

    length -= count;

    if (index & 0x10) {		
      index -= 0x10;
      htblptr = &cinfo->ac_huff_tbl_ptrs[index];
    } else {			
      htblptr = &cinfo->dc_huff_tbl_ptrs[index];
    }

    if (index < 0 || index >= NUM_HUFF_TBLS)
      ERREXIT1(cinfo, JERR_DHT_INDEX, index);

    if (*htblptr == NULL)
      *htblptr = jpeg_alloc_huff_table((j_common_ptr) cinfo);
  
    MEMCOPY((*htblptr)->bits, bits, SIZEOF((*htblptr)->bits));
    MEMCOPY((*htblptr)->huffval, huffval, SIZEOF((*htblptr)->huffval));
  }

  INPUT_SYNC(cinfo);
  return TRUE;
}


LOCAL boolean
get_dqt (j_decompress_ptr cinfo)
{
  INT32 length;
  int n, i, prec;
  unsigned int tmp;
  JQUANT_TBL *quant_ptr;
  INPUT_VARS(cinfo);

  INPUT_2BYTES(cinfo, length, return FALSE);
  length -= 2;

  while (length > 0) {
    INPUT_BYTE(cinfo, n, return FALSE);
    prec = n >> 4;
    n &= 0x0F;

    TRACEMS2(cinfo, 1, JTRC_DQT, n, prec);

    if (n >= NUM_QUANT_TBLS)
      ERREXIT1(cinfo, JERR_DQT_INDEX, n);
      
    if (cinfo->quant_tbl_ptrs[n] == NULL)
      cinfo->quant_tbl_ptrs[n] = jpeg_alloc_quant_table((j_common_ptr) cinfo);
    quant_ptr = cinfo->quant_tbl_ptrs[n];

    for (i = 0; i < DCTSIZE2; i++) {
      if (prec)
	INPUT_2BYTES(cinfo, tmp, return FALSE);
      else
	INPUT_BYTE(cinfo, tmp, return FALSE);
      quant_ptr->quantval[i] = (UINT16) tmp;
    }

    for (i = 0; i < DCTSIZE2; i += 8) {
      TRACEMS8(cinfo, 2, JTRC_QUANTVALS,
	       quant_ptr->quantval[i  ], quant_ptr->quantval[i+1],
	       quant_ptr->quantval[i+2], quant_ptr->quantval[i+3],
	       quant_ptr->quantval[i+4], quant_ptr->quantval[i+5],
	       quant_ptr->quantval[i+6], quant_ptr->quantval[i+7]);
    }

    length -= DCTSIZE2+1;
    if (prec) length -= DCTSIZE2;
  }

  INPUT_SYNC(cinfo);
  return TRUE;
}


LOCAL boolean
get_dri (j_decompress_ptr cinfo)
{
  INT32 length;
  unsigned int tmp;
  INPUT_VARS(cinfo);

  INPUT_2BYTES(cinfo, length, return FALSE);
  
  if (length != 4)
    ERREXIT(cinfo, JERR_BAD_LENGTH);

  INPUT_2BYTES(cinfo, tmp, return FALSE);

  TRACEMS1(cinfo, 1, JTRC_DRI, tmp);

  cinfo->restart_interval = tmp;

  INPUT_SYNC(cinfo);
  return TRUE;
}


METHODDEF boolean
skip_variable (j_decompress_ptr cinfo)
{
  INT32 length;
  INPUT_VARS(cinfo);

  INPUT_2BYTES(cinfo, length, return FALSE);
  
  TRACEMS2(cinfo, 1, JTRC_MISC_MARKER, cinfo->unread_marker, (int) length);

  INPUT_SYNC(cinfo);		
  (*cinfo->src->skip_input_data) (cinfo, (long) length - 2L);

  return TRUE;
}



LOCAL boolean
next_marker (j_decompress_ptr cinfo)
{
  int c;
  INPUT_VARS(cinfo);

  for (;;) {
    INPUT_BYTE(cinfo, c, return FALSE);
    while (c != 0xFF) {
      cinfo->marker->discarded_bytes++;
      INPUT_SYNC(cinfo);
      INPUT_BYTE(cinfo, c, return FALSE);
    }
    do {
      INPUT_BYTE(cinfo, c, return FALSE);
    } while (c == 0xFF);
    if (c != 0)
      break;			
    cinfo->marker->discarded_bytes += 2;
    INPUT_SYNC(cinfo);
  }

  if (cinfo->marker->discarded_bytes != 0) {
    WARNMS2(cinfo, JWRN_EXTRANEOUS_DATA, cinfo->marker->discarded_bytes, c);
    cinfo->marker->discarded_bytes = 0;
  }

  cinfo->unread_marker = c;

  INPUT_SYNC(cinfo);
  return TRUE;
}


LOCAL boolean
first_marker (j_decompress_ptr cinfo)
{
  int c, c2;
  INPUT_VARS(cinfo);

  INPUT_BYTE(cinfo, c, return FALSE);
  INPUT_BYTE(cinfo, c2, return FALSE);
  if (c != 0xFF || c2 != (int) M_SOI)
    ERREXIT2(cinfo, JERR_NO_SOI, c, c2);

  cinfo->unread_marker = c2;

  INPUT_SYNC(cinfo);
  return TRUE;
}



METHODDEF int
read_markers (j_decompress_ptr cinfo)
{
  for (;;) {
    if (cinfo->unread_marker == 0) {
      if (! cinfo->marker->saw_SOI) {
	if (! first_marker(cinfo))
	  return JPEG_SUSPENDED;
      } else {
	if (! next_marker(cinfo))
	  return JPEG_SUSPENDED;
      }
    }
    switch (cinfo->unread_marker) {
    case M_SOI:
      if (! get_soi(cinfo))
	return JPEG_SUSPENDED;
      break;

    case M_SOF0:		
    case M_SOF1:		
      if (! get_sof(cinfo, FALSE, FALSE))
	return JPEG_SUSPENDED;
      break;

    case M_SOF2:		
      if (! get_sof(cinfo, TRUE, FALSE))
	return JPEG_SUSPENDED;
      break;

    case M_SOF9:		
      if (! get_sof(cinfo, FALSE, TRUE))
	return JPEG_SUSPENDED;
      break;

    case M_SOF10:		
      if (! get_sof(cinfo, TRUE, TRUE))
	return JPEG_SUSPENDED;
      break;

    case M_SOF3:		
    case M_SOF5:		
    case M_SOF6:		
    case M_SOF7:		
    case M_JPG:			
    case M_SOF11:		
    case M_SOF13:		
    case M_SOF14:		
    case M_SOF15:		
      ERREXIT1(cinfo, JERR_SOF_UNSUPPORTED, cinfo->unread_marker);
      break;

    case M_SOS:
      if (! get_sos(cinfo))
	return JPEG_SUSPENDED;
      cinfo->unread_marker = 0;	
      return JPEG_REACHED_SOS;
    
    case M_EOI:
      TRACEMS(cinfo, 1, JTRC_EOI);
      cinfo->unread_marker = 0;	
      return JPEG_REACHED_EOI;
      
    case M_DAC:
      if (! get_dac(cinfo))
	return JPEG_SUSPENDED;
      break;
      
    case M_DHT:
      if (! get_dht(cinfo))
	return JPEG_SUSPENDED;
      break;
      
    case M_DQT:
      if (! get_dqt(cinfo))
	return JPEG_SUSPENDED;
      break;
      
    case M_DRI:
      if (! get_dri(cinfo))
	return JPEG_SUSPENDED;
      break;
      
    case M_APP0:
    case M_APP1:
    case M_APP2:
    case M_APP3:
    case M_APP4:
    case M_APP5:
    case M_APP6:
    case M_APP7:
    case M_APP8:
    case M_APP9:
    case M_APP10:
    case M_APP11:
    case M_APP12:
    case M_APP13:
    case M_APP14:
    case M_APP15:
      if (! (*cinfo->marker->process_APPn[cinfo->unread_marker - (int) M_APP0]) (cinfo))
	return JPEG_SUSPENDED;
      break;
      
    case M_COM:
      if (! (*cinfo->marker->process_COM) (cinfo))
	return JPEG_SUSPENDED;
      break;

    case M_RST0:		
    case M_RST1:
    case M_RST2:
    case M_RST3:
    case M_RST4:
    case M_RST5:
    case M_RST6:
    case M_RST7:
    case M_TEM:
      TRACEMS1(cinfo, 1, JTRC_PARMLESS_MARKER, cinfo->unread_marker);
      break;

    case M_DNL:			
      if (! skip_variable(cinfo))
	return JPEG_SUSPENDED;
      break;

    default:			
      ERREXIT1(cinfo, JERR_UNKNOWN_MARKER, cinfo->unread_marker);
      break;
    }
    cinfo->unread_marker = 0;
  } 
}



METHODDEF boolean
read_restart_marker (j_decompress_ptr cinfo)
{
  if (cinfo->unread_marker == 0) {
    if (! next_marker(cinfo))
      return FALSE;
  }

  if (cinfo->unread_marker ==
      ((int) M_RST0 + cinfo->marker->next_restart_num)) {
    TRACEMS1(cinfo, 2, JTRC_RST, cinfo->marker->next_restart_num);
    cinfo->unread_marker = 0;
  } else {
    if (! (*cinfo->src->resync_to_restart) (cinfo,
					    cinfo->marker->next_restart_num))
      return FALSE;
  }

  cinfo->marker->next_restart_num = (cinfo->marker->next_restart_num + 1) & 7;

  return TRUE;
}



GLOBAL boolean
jpeg_resync_to_restart (j_decompress_ptr cinfo, int desired)
{
  int marker = cinfo->unread_marker;
  int action = 1;
  
  WARNMS2(cinfo, JWRN_MUST_RESYNC, marker, desired);
  
  for (;;) {
    if (marker < (int) M_SOF0)
      action = 2;		
    else if (marker < (int) M_RST0 || marker > (int) M_RST7)
      action = 3;		
    else {
      if (marker == ((int) M_RST0 + ((desired+1) & 7)) ||
	  marker == ((int) M_RST0 + ((desired+2) & 7)))
	action = 3;		
      else if (marker == ((int) M_RST0 + ((desired-1) & 7)) ||
	       marker == ((int) M_RST0 + ((desired-2) & 7)))
	action = 2;		
      else
	action = 1;		
    }
    TRACEMS2(cinfo, 4, JTRC_RECOVERY_ACTION, marker, action);
    switch (action) {
    case 1:
      cinfo->unread_marker = 0;
      return TRUE;
    case 2:
      if (! next_marker(cinfo))
	return FALSE;
      marker = cinfo->unread_marker;
      break;
    case 3:
      return TRUE;
    }
  } 
}



METHODDEF void
reset_marker_reader (j_decompress_ptr cinfo)
{
  cinfo->comp_info = NULL;		
  cinfo->input_scan_number = 0;		
  cinfo->unread_marker = 0;		
  cinfo->marker->saw_SOI = FALSE;	
  cinfo->marker->saw_SOF = FALSE;
  cinfo->marker->discarded_bytes = 0;
}



GLOBAL void
jinit_marker_reader (j_decompress_ptr cinfo)
{
  int i;

  cinfo->marker = (struct jpeg_marker_reader *)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
				SIZEOF(struct jpeg_marker_reader));
  cinfo->marker->reset_marker_reader = reset_marker_reader;
  cinfo->marker->read_markers = read_markers;
  cinfo->marker->read_restart_marker = read_restart_marker;
  cinfo->marker->process_COM = skip_variable;
  for (i = 0; i < 16; i++)
    cinfo->marker->process_APPn[i] = skip_variable;
  cinfo->marker->process_APPn[0] = get_app0;
  cinfo->marker->process_APPn[14] = get_app14;
  reset_marker_reader(cinfo);
}
