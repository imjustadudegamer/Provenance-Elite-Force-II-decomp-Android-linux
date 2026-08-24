
#pragma once

#include <cstddef>


typedef unsigned char md5_byte_t; 
typedef unsigned int md5_word_t; 

typedef struct md5_state_s {
    md5_word_t count[2];	
    md5_word_t abcd[4];		
    md5_byte_t buf[64];		
} md5_state_t;

#ifdef __cplusplus
extern "C" 
{
#endif




#ifdef __cplusplus
}  
#endif
