/*
 * jmorecfg.h
 *
 * Copyright (C) 1991-1995, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains additional configuration options that customize the
 * JPEG software for special applications or support machine-dependent
 * optimizations.  Most operators will not need to touch this file.
 */



#define BITS_IN_JSAMPLE  8	



#define MAX_COMPONENTS  10	




#if BITS_IN_JSAMPLE == 8

#ifdef HAVE_UNSIGNED_CHAR

typedef unsigned char JSAMPLE;
#define GETJSAMPLE(value)  ((int) (value))

#else 

typedef char JSAMPLE;
#ifdef CHAR_IS_UNSIGNED
#define GETJSAMPLE(value)  ((int) (value))
#else
#define GETJSAMPLE(value)  ((int) (value) & 0xFF)
#endif 

#endif 

#define MAXJSAMPLE	255
#define CENTERJSAMPLE	128

#endif 


#if BITS_IN_JSAMPLE == 12

typedef short JSAMPLE;
#define GETJSAMPLE(value)  ((int) (value))

#define MAXJSAMPLE	4095
#define CENTERJSAMPLE	2048

#endif 



typedef short JCOEF;



#ifdef HAVE_UNSIGNED_CHAR

typedef unsigned char JOCTET;
#define GETJOCTET(value)  (value)

#else 

typedef char JOCTET;
#ifdef CHAR_IS_UNSIGNED
#define GETJOCTET(value)  (value)
#else
#define GETJOCTET(value)  ((value) & 0xFF)
#endif 

#endif 




#ifdef HAVE_UNSIGNED_CHAR
typedef unsigned char UINT8;
#else 
#ifdef CHAR_IS_UNSIGNED
typedef char UINT8;
#else 
typedef short UINT8;
#endif 
#endif 


#ifdef HAVE_UNSIGNED_SHORT
typedef unsigned short UINT16;
#else 
typedef unsigned int UINT16;
#endif 


#ifndef XMD_H			
typedef short INT16;
#endif




typedef unsigned int JDIMENSION;

#define JPEG_MAX_DIMENSION  65500L  



#define METHODDEF static	
#define LOCAL	  static	
#define GLOBAL			
#define EXTERN	  extern	



#ifdef NEED_FAR_POINTERS
#undef FAR
#define FAR  far
#else
#undef FAR
#define FAR
#endif



#ifndef FALSE			
#define FALSE	0		
#endif
#ifndef TRUE
#define TRUE	1
#endif



#ifdef JPEG_INTERNALS
#define JPEG_INTERNAL_OPTIONS
#endif

#ifdef JPEG_INTERNAL_OPTIONS





#undef DCT_ISLOW_SUPPORTED	
#undef DCT_IFAST_SUPPORTED	
#define DCT_FLOAT_SUPPORTED	


#undef  C_ARITH_CODING_SUPPORTED    
#define C_MULTISCAN_FILES_SUPPORTED 
#define C_PROGRESSIVE_SUPPORTED	    
#define ENTROPY_OPT_SUPPORTED	    
#define INPUT_SMOOTHING_SUPPORTED   


#undef  D_ARITH_CODING_SUPPORTED    
#undef D_MULTISCAN_FILES_SUPPORTED 
#undef D_PROGRESSIVE_SUPPORTED	    
#undef BLOCK_SMOOTHING_SUPPORTED   
#undef IDCT_SCALING_SUPPORTED	    
#undef  UPSAMPLE_SCALING_SUPPORTED  
#undef UPSAMPLE_MERGING_SUPPORTED  
#undef QUANT_1PASS_SUPPORTED	    
#undef QUANT_2PASS_SUPPORTED	    




#define RGB_RED		0	
#define RGB_GREEN	1	
#define RGB_BLUE	2	
#define RGB_PIXELSIZE	4	





#ifndef INLINE
#ifdef __GNUC__			
#define INLINE __inline__
#endif
#ifndef INLINE
#define INLINE			
#endif
#endif



#ifndef MULTIPLIER
#define MULTIPLIER  int		
#endif



#ifndef FAST_FLOAT
#ifdef HAVE_PROTOTYPES
#define FAST_FLOAT  float
#else
#define FAST_FLOAT  double
#endif
#endif

#endif 
