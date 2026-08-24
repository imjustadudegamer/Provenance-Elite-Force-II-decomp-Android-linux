

typedef struct {
  unsigned int ehufco[256];	
  char ehufsi[256];		
} c_derived_tbl;


#ifdef NEED_SHORT_EXTERNAL_NAMES
#define jpeg_make_c_derived_tbl	jMkCDerived
#define jpeg_gen_optimal_table	jGenOptTbl
#endif 

EXTERN void jpeg_make_c_derived_tbl JPP((j_compress_ptr cinfo,
				JHUFF_TBL * htbl, c_derived_tbl ** pdtbl));

EXTERN void jpeg_gen_optimal_table JPP((j_compress_ptr cinfo,
					JHUFF_TBL * htbl, long freq[]));
