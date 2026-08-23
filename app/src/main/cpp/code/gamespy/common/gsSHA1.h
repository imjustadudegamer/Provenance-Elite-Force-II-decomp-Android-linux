
#ifndef _SHA1_H_
#define _SHA1_H_

#include "gsCommon.h"
#ifndef _PS3
	typedef gsi_u32 uint32_t;
	typedef gsi_u8  uint8_t;
#endif

typedef gsi_i16 int_least16_t;

#ifndef _SHA_enum_
#define _SHA_enum_
enum
{
    shaSuccess = 0,
    shaNull,            
    shaInputTooLong,    
    shaStateError       
};
#endif
#define SHA1HashSize 20

typedef struct SHA1Context
{
    uint32_t Intermediate_Hash[SHA1HashSize/4]; 

    uint32_t Length_Low;            
    uint32_t Length_High;           

    int_least16_t Message_Block_Index;
    uint8_t Message_Block[64];      

    int Computed;               
    int Corrupted;             
} SHA1Context;


#if defined(__cplusplus)
extern "C"
{
#endif


int SHA1Reset(  SHA1Context *);
int SHA1Input(  SHA1Context *,
                const uint8_t *,
                unsigned int);
int SHA1Result( SHA1Context *,
                uint8_t Message_Digest[SHA1HashSize]);


#if defined(__cplusplus)
}
#endif 


#endif
