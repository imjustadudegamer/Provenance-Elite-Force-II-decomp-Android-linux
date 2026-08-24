
#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"
#include "jdhuff.h"		



typedef struct {
  int last_dc_val[MAX_COMPS_IN_SCAN]; 
} savable_state;


#ifndef NO_STRUCT_ASSIGN
#define ASSIGN_STATE(dest,src)  ((dest) = (src))
#else
#if MAX_COMPS_IN_SCAN == 4
#define ASSIGN_STATE(dest,src)  \
	((dest).last_dc_val[0] = (src).last_dc_val[0], \
	 (dest).last_dc_val[1] = (src).last_dc_val[1], \
	 (dest).last_dc_val[2] = (src).last_dc_val[2], \
	 (dest).last_dc_val[3] = (src).last_dc_val[3])
#endif
#endif


typedef struct {
  struct jpeg_entropy_decoder pub; 

  bitread_perm_state bitstate;	
  savable_state saved;		

  unsigned int restarts_to_go;	

  d_derived_tbl * dc_derived_tbls[NUM_HUFF_TBLS];
  d_derived_tbl * ac_derived_tbls[NUM_HUFF_TBLS];
} huff_entropy_decoder;

typedef huff_entropy_decoder * huff_entropy_ptr;



METHODDEF void
start_pass_huff_decoder (j_decompress_ptr cinfo)
{
  huff_entropy_ptr entropy = (huff_entropy_ptr) cinfo->entropy;
  int ci, dctbl, actbl;
  jpeg_component_info * compptr;

  if (cinfo->Ss != 0 || cinfo->Se != DCTSIZE2-1 ||
      cinfo->Ah != 0 || cinfo->Al != 0)
    WARNMS(cinfo, JWRN_NOT_SEQUENTIAL);

  for (ci = 0; ci < cinfo->comps_in_scan; ci++) {
    compptr = cinfo->cur_comp_info[ci];
    dctbl = compptr->dc_tbl_no;
    actbl = compptr->ac_tbl_no;
    if (dctbl < 0 || dctbl >= NUM_HUFF_TBLS ||
	cinfo->dc_huff_tbl_ptrs[dctbl] == NULL)
      ERREXIT1(cinfo, JERR_NO_HUFF_TABLE, dctbl);
    if (actbl < 0 || actbl >= NUM_HUFF_TBLS ||
	cinfo->ac_huff_tbl_ptrs[actbl] == NULL)
      ERREXIT1(cinfo, JERR_NO_HUFF_TABLE, actbl);
    jpeg_make_d_derived_tbl(cinfo, cinfo->dc_huff_tbl_ptrs[dctbl],
			    & entropy->dc_derived_tbls[dctbl]);
    jpeg_make_d_derived_tbl(cinfo, cinfo->ac_huff_tbl_ptrs[actbl],
			    & entropy->ac_derived_tbls[actbl]);
    entropy->saved.last_dc_val[ci] = 0;
  }

  entropy->bitstate.bits_left = 0;
  entropy->bitstate.get_buffer = 0; 
  entropy->bitstate.printed_eod = FALSE;

  entropy->restarts_to_go = cinfo->restart_interval;
}



GLOBAL void
jpeg_make_d_derived_tbl (j_decompress_ptr cinfo, JHUFF_TBL * htbl,
			 d_derived_tbl ** pdtbl)
{
  d_derived_tbl *dtbl;
  int p, i, l, si;
  int lookbits, ctr;
  char huffsize[257];
  unsigned int huffcode[257];
  unsigned int code;

  if (*pdtbl == NULL)
    *pdtbl = (d_derived_tbl *)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				  SIZEOF(d_derived_tbl));
  dtbl = *pdtbl;
  dtbl->pub = htbl;		
  

  p = 0;
  for (l = 1; l <= 16; l++) {
    for (i = 1; i <= (int) htbl->bits[l]; i++)
      huffsize[p++] = (char) l;
  }
  huffsize[p] = 0;
  
  
  code = 0;
  si = huffsize[0];
  p = 0;
  while (huffsize[p]) {
    while (((int) huffsize[p]) == si) {
      huffcode[p++] = code;
      code++;
    }
    code <<= 1;
    si++;
  }


  p = 0;
  for (l = 1; l <= 16; l++) {
    if (htbl->bits[l]) {
      dtbl->valptr[l] = p; 
      dtbl->mincode[l] = huffcode[p]; 
      p += htbl->bits[l];
      dtbl->maxcode[l] = huffcode[p-1]; 
    } else {
      dtbl->maxcode[l] = -1;	
    }
  }
  dtbl->maxcode[17] = 0xFFFFFL; 


  MEMZERO(dtbl->look_nbits, SIZEOF(dtbl->look_nbits));

  p = 0;
  for (l = 1; l <= HUFF_LOOKAHEAD; l++) {
    for (i = 1; i <= (int) htbl->bits[l]; i++, p++) {
      lookbits = huffcode[p] << (HUFF_LOOKAHEAD-l);
      for (ctr = 1 << (HUFF_LOOKAHEAD-l); ctr > 0; ctr--) {
	dtbl->look_nbits[lookbits] = l;
	dtbl->look_sym[lookbits] = htbl->huffval[p];
	lookbits++;
      }
    }
  }
}



#ifdef SLOW_SHIFT_32
#define MIN_GET_BITS  15	
#else
#define MIN_GET_BITS  (BIT_BUF_SIZE-7)
#endif


GLOBAL boolean
jpeg_fill_bit_buffer (bitread_working_state * state,
		      register bit_buf_type get_buffer, register int bits_left,
		      int nbits)
{
  register const JOCTET * next_input_byte = state->next_input_byte;
  register size_t bytes_in_buffer = state->bytes_in_buffer;
  register int c;


  while (bits_left < MIN_GET_BITS) {
    if (state->unread_marker != 0)
      goto no_more_data;	

    if (bytes_in_buffer == 0) {
      if (! (*state->cinfo->src->fill_input_buffer) (state->cinfo))
	return FALSE;
      next_input_byte = state->cinfo->src->next_input_byte;
      bytes_in_buffer = state->cinfo->src->bytes_in_buffer;
    }
    bytes_in_buffer--;
    c = GETJOCTET(*next_input_byte++);

    if (c == 0xFF) {
      do {
	if (bytes_in_buffer == 0) {
	  if (! (*state->cinfo->src->fill_input_buffer) (state->cinfo))
	    return FALSE;
	  next_input_byte = state->cinfo->src->next_input_byte;
	  bytes_in_buffer = state->cinfo->src->bytes_in_buffer;
	}
	bytes_in_buffer--;
	c = GETJOCTET(*next_input_byte++);
      } while (c == 0xFF);

      if (c == 0) {
	c = 0xFF;
      } else {
	state->unread_marker = c;

      no_more_data:
	if (bits_left >= nbits)
	  break;
	if (! *(state->printed_eod_ptr)) {
	  WARNMS(state->cinfo, JWRN_HIT_MARKER);
	  *(state->printed_eod_ptr) = TRUE;
	}
	c = 0;			
      }
    }

    get_buffer = (get_buffer << 8) | c;
    bits_left += 8;
  }

  state->next_input_byte = next_input_byte;
  state->bytes_in_buffer = bytes_in_buffer;
  state->get_buffer = get_buffer;
  state->bits_left = bits_left;

  return TRUE;
}



GLOBAL int
jpeg_huff_decode (bitread_working_state * state,
		  register bit_buf_type get_buffer, register int bits_left,
		  d_derived_tbl * htbl, int min_bits)
{
  register int l = min_bits;
  register INT32 code;


  CHECK_BIT_BUFFER(*state, l, return -1);
  code = GET_BITS(l);


  while (code > htbl->maxcode[l]) {
    code <<= 1;
    CHECK_BIT_BUFFER(*state, 1, return -1);
    code |= GET_BITS(1);
    l++;
  }

  state->get_buffer = get_buffer;
  state->bits_left = bits_left;


  if (l > 16) {
    WARNMS(state->cinfo, JWRN_HUFF_BAD_CODE);
    return 0;			
  }

  return htbl->pub->huffval[ htbl->valptr[l] +
			    ((int) (code - htbl->mincode[l])) ];
}



#ifdef AVOID_TABLES

#define HUFF_EXTEND(x,s)  ((x) < (1<<((s)-1)) ? (x) + (((-1)<<(s)) + 1) : (x))

#else

#define HUFF_EXTEND(x,s)  ((x) < extend_test[s] ? (x) + extend_offset[s] : (x))

static const int extend_test[16] =   
  { 0, 0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080,
    0x0100, 0x0200, 0x0400, 0x0800, 0x1000, 0x2000, 0x4000 };

static const int extend_offset[16] = 
  { 0, ((-1)<<1) + 1, ((-1)<<2) + 1, ((-1)<<3) + 1, ((-1)<<4) + 1,
    ((-1)<<5) + 1, ((-1)<<6) + 1, ((-1)<<7) + 1, ((-1)<<8) + 1,
    ((-1)<<9) + 1, ((-1)<<10) + 1, ((-1)<<11) + 1, ((-1)<<12) + 1,
    ((-1)<<13) + 1, ((-1)<<14) + 1, ((-1)<<15) + 1 };

#endif 



LOCAL boolean
process_restart (j_decompress_ptr cinfo)
{
  huff_entropy_ptr entropy = (huff_entropy_ptr) cinfo->entropy;
  int ci;

  cinfo->marker->discarded_bytes += entropy->bitstate.bits_left / 8;
  entropy->bitstate.bits_left = 0;

  if (! (*cinfo->marker->read_restart_marker) (cinfo))
    return FALSE;

  for (ci = 0; ci < cinfo->comps_in_scan; ci++)
    entropy->saved.last_dc_val[ci] = 0;

  entropy->restarts_to_go = cinfo->restart_interval;

  entropy->bitstate.printed_eod = FALSE;

  return TRUE;
}



METHODDEF boolean
decode_mcu (j_decompress_ptr cinfo, JBLOCKROW *MCU_data)
{
  huff_entropy_ptr entropy = (huff_entropy_ptr) cinfo->entropy;
  register int s, k, r;
  int blkn, ci;
  JBLOCKROW block;
  BITREAD_STATE_VARS;
  savable_state state;
  d_derived_tbl * dctbl;
  d_derived_tbl * actbl;
  jpeg_component_info * compptr;

  if (cinfo->restart_interval) {
    if (entropy->restarts_to_go == 0)
      if (! process_restart(cinfo))
	return FALSE;
  }

  BITREAD_LOAD_STATE(cinfo,entropy->bitstate);
  ASSIGN_STATE(state, entropy->saved);


  for (blkn = 0; blkn < cinfo->blocks_in_MCU; blkn++) {
    block = MCU_data[blkn];
    ci = cinfo->MCU_membership[blkn];
    compptr = cinfo->cur_comp_info[ci];
    dctbl = entropy->dc_derived_tbls[compptr->dc_tbl_no];
    actbl = entropy->ac_derived_tbls[compptr->ac_tbl_no];


    HUFF_DECODE(s, br_state, dctbl, return FALSE, label1);
    if (s) {
      CHECK_BIT_BUFFER(br_state, s, return FALSE);
      r = GET_BITS(s);
      s = HUFF_EXTEND(r, s);
    }

    if (! compptr->component_needed)
      goto skip_ACs;

    s += state.last_dc_val[ci];
    state.last_dc_val[ci] = s;
    (*block)[0] = (JCOEF) s;

    if (compptr->DCT_scaled_size > 1) {

      for (k = 1; k < DCTSIZE2; k++) {
	HUFF_DECODE(s, br_state, actbl, return FALSE, label2);
      
	r = s >> 4;
	s &= 15;
      
	if (s) {
	  k += r;
	  CHECK_BIT_BUFFER(br_state, s, return FALSE);
	  r = GET_BITS(s);
	  s = HUFF_EXTEND(r, s);
	  (*block)[jpeg_natural_order[k]] = (JCOEF) s;
	} else {
	  if (r != 15)
	    break;
	  k += 15;
	}
      }

    } else {
skip_ACs:

      for (k = 1; k < DCTSIZE2; k++) {
	HUFF_DECODE(s, br_state, actbl, return FALSE, label3);
      
	r = s >> 4;
	s &= 15;
      
	if (s) {
	  k += r;
	  CHECK_BIT_BUFFER(br_state, s, return FALSE);
	  DROP_BITS(s);
	} else {
	  if (r != 15)
	    break;
	  k += 15;
	}
      }

    }
  }

  BITREAD_SAVE_STATE(cinfo,entropy->bitstate);
  ASSIGN_STATE(entropy->saved, state);

  entropy->restarts_to_go--;

  return TRUE;
}



GLOBAL void
jinit_huff_decoder (j_decompress_ptr cinfo)
{
  huff_entropy_ptr entropy;
  int i;

  entropy = (huff_entropy_ptr)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				SIZEOF(huff_entropy_decoder));
  cinfo->entropy = (struct jpeg_entropy_decoder *) entropy;
  entropy->pub.start_pass = start_pass_huff_decoder;
  entropy->pub.decode_mcu = decode_mcu;

  for (i = 0; i < NUM_HUFF_TBLS; i++) {
    entropy->dc_derived_tbls[i] = entropy->ac_derived_tbls[i] = NULL;
  }
}
