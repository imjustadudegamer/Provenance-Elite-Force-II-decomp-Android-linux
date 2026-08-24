/*
 * jdhuff.h
 *
 * Copyright (C) 1991-1995, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains declarations for Huffman entropy decoding routines
 * that are shared between the sequential decoder (jdhuff.c) and the
 * progressive decoder (jdphuff.c).  No other modules need to see these.
 */


#ifdef NEED_SHORT_EXTERNAL_NAMES
#define jpeg_make_d_derived_tbl	jMkDDerived
#define jpeg_fill_bit_buffer	jFilBitBuf
#define jpeg_huff_decode	jHufDecode
#endif 



#define HUFF_LOOKAHEAD	8	

typedef struct {
  INT32 mincode[17];		
  INT32 maxcode[18];		
  int valptr[17];		

  JHUFF_TBL *pub;

  int look_nbits[1<<HUFF_LOOKAHEAD]; 
  UINT8 look_sym[1<<HUFF_LOOKAHEAD]; 
} d_derived_tbl;

EXTERN void jpeg_make_d_derived_tbl JPP((j_decompress_ptr cinfo,
				JHUFF_TBL * htbl, d_derived_tbl ** pdtbl));



typedef INT32 bit_buf_type;	
#define BIT_BUF_SIZE  32	


typedef struct {		
  bit_buf_type get_buffer;	
  int bits_left;		
  boolean printed_eod;		
} bitread_perm_state;

typedef struct {		
  const JOCTET * next_input_byte; 
  size_t bytes_in_buffer;	
  int unread_marker;		
  bit_buf_type get_buffer;	
  int bits_left;		
  j_decompress_ptr cinfo;	
  boolean * printed_eod_ptr;	
} bitread_working_state;

#define BITREAD_STATE_VARS  \
	register bit_buf_type get_buffer;  \
	register int bits_left;  \
	bitread_working_state br_state

#define BITREAD_LOAD_STATE(cinfop,permstate)  \
	br_state.cinfo = cinfop; \
	br_state.next_input_byte = cinfop->src->next_input_byte; \
	br_state.bytes_in_buffer = cinfop->src->bytes_in_buffer; \
	br_state.unread_marker = cinfop->unread_marker; \
	get_buffer = permstate.get_buffer; \
	bits_left = permstate.bits_left; \
	br_state.printed_eod_ptr = & permstate.printed_eod

#define BITREAD_SAVE_STATE(cinfop,permstate)  \
	cinfop->src->next_input_byte = br_state.next_input_byte; \
	cinfop->src->bytes_in_buffer = br_state.bytes_in_buffer; \
	cinfop->unread_marker = br_state.unread_marker; \
	permstate.get_buffer = get_buffer; \
	permstate.bits_left = bits_left


#define CHECK_BIT_BUFFER(state,nbits,action) \
	{ if (bits_left < (nbits)) {  \
	    if (! jpeg_fill_bit_buffer(&(state),get_buffer,bits_left,nbits))  \
	      { action; }  \
	    get_buffer = (state).get_buffer; bits_left = (state).bits_left; } }

#define GET_BITS(nbits) \
	(((int) (get_buffer >> (bits_left -= (nbits)))) & ((1<<(nbits))-1))

#define PEEK_BITS(nbits) \
	(((int) (get_buffer >> (bits_left -  (nbits)))) & ((1<<(nbits))-1))

#define DROP_BITS(nbits) \
	(bits_left -= (nbits))

EXTERN boolean jpeg_fill_bit_buffer JPP((bitread_working_state * state,
		register bit_buf_type get_buffer, register int bits_left,
		int nbits));



#define HUFF_DECODE(result,state,htbl,failaction,slowlabel) \
{ register int nb, look; \
  if (bits_left < HUFF_LOOKAHEAD) { \
    if (! jpeg_fill_bit_buffer(&state,get_buffer,bits_left, 0)) {failaction;} \
    get_buffer = state.get_buffer; bits_left = state.bits_left; \
    if (bits_left < HUFF_LOOKAHEAD) { \
      nb = 1; goto slowlabel; \
    } \
  } \
  look = PEEK_BITS(HUFF_LOOKAHEAD); \
  if ((nb = htbl->look_nbits[look]) != 0) { \
    DROP_BITS(nb); \
    result = htbl->look_sym[look]; \
  } else { \
    nb = HUFF_LOOKAHEAD+1; \
slowlabel: \
    if ((result=jpeg_huff_decode(&state,get_buffer,bits_left,htbl,nb)) < 0) \
	{ failaction; } \
    get_buffer = state.get_buffer; bits_left = state.bits_left; \
  } \
}

EXTERN int jpeg_huff_decode JPP((bitread_working_state * state,
		register bit_buf_type get_buffer, register int bits_left,
		d_derived_tbl * htbl, int min_bits));
