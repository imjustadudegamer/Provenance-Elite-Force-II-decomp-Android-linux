


#if BITS_IN_JSAMPLE == 8
typedef int DCTELEM;		
#else
typedef INT32 DCTELEM;		
#endif

typedef JMETHOD(void, forward_DCT_method_ptr, (DCTELEM * data));
typedef JMETHOD(void, float_DCT_method_ptr, (FAST_FLOAT * data));





typedef MULTIPLIER ISLOW_MULT_TYPE; 
#if BITS_IN_JSAMPLE == 8
typedef MULTIPLIER IFAST_MULT_TYPE; 
#define IFAST_SCALE_BITS  2	
#else
typedef INT32 IFAST_MULT_TYPE;	
#define IFAST_SCALE_BITS  13	
#endif
typedef FAST_FLOAT FLOAT_MULT_TYPE; 



#define IDCT_range_limit(cinfo)  ((cinfo)->sample_range_limit + CENTERJSAMPLE)

#define RANGE_MASK  (MAXJSAMPLE * 4 + 3) 



#ifdef NEED_SHORT_EXTERNAL_NAMES
#define jpeg_fdct_islow		jFDislow
#define jpeg_fdct_ifast		jFDifast
#define jpeg_fdct_float		jFDfloat
#define jpeg_idct_islow		jRDislow
#define jpeg_idct_ifast		jRDifast
#define jpeg_idct_float		jRDfloat
#define jpeg_idct_4x4		jRD4x4
#define jpeg_idct_2x2		jRD2x2
#define jpeg_idct_1x1		jRD1x1
#endif 


EXTERN void jpeg_fdct_islow JPP((DCTELEM * data));
EXTERN void jpeg_fdct_ifast JPP((DCTELEM * data));
EXTERN void jpeg_fdct_float JPP((FAST_FLOAT * data));

EXTERN void jpeg_idct_islow
    JPP((j_decompress_ptr cinfo, jpeg_component_info * compptr,
	 JCOEFPTR coef_block, JSAMPARRAY output_buf, JDIMENSION output_col));
EXTERN void jpeg_idct_ifast
    JPP((j_decompress_ptr cinfo, jpeg_component_info * compptr,
	 JCOEFPTR coef_block, JSAMPARRAY output_buf, JDIMENSION output_col));
EXTERN void jpeg_idct_float
    JPP((j_decompress_ptr cinfo, jpeg_component_info * compptr,
	 JCOEFPTR coef_block, JSAMPARRAY output_buf, JDIMENSION output_col));
EXTERN void jpeg_idct_4x4
    JPP((j_decompress_ptr cinfo, jpeg_component_info * compptr,
	 JCOEFPTR coef_block, JSAMPARRAY output_buf, JDIMENSION output_col));
EXTERN void jpeg_idct_2x2
    JPP((j_decompress_ptr cinfo, jpeg_component_info * compptr,
	 JCOEFPTR coef_block, JSAMPARRAY output_buf, JDIMENSION output_col));
EXTERN void jpeg_idct_1x1
    JPP((j_decompress_ptr cinfo, jpeg_component_info * compptr,
	 JCOEFPTR coef_block, JSAMPARRAY output_buf, JDIMENSION output_col));



#define ONE	((INT32) 1)
#define CONST_SCALE (ONE << CONST_BITS)


#define FIX(x)	((INT32) ((x) * CONST_SCALE + 0.5))


#define DESCALE(x,n)  RIGHT_SHIFT((x) + (ONE << ((n)-1)), n)


#ifdef SHORTxSHORT_32		
#define MULTIPLY16C16(var,const)  (((INT16) (var)) * ((INT16) (const)))
#endif
#ifdef SHORTxLCONST_32		
#define MULTIPLY16C16(var,const)  (((INT16) (var)) * ((INT32) (const)))
#endif

#ifndef MULTIPLY16C16		
#define MULTIPLY16C16(var,const)  ((var) * (const))
#endif


#ifdef SHORTxSHORT_32		
#define MULTIPLY16V16(var1,var2)  (((INT16) (var1)) * ((INT16) (var2)))
#endif

#ifndef MULTIPLY16V16		
#define MULTIPLY16V16(var1,var2)  ((var1) * (var2))
#endif
