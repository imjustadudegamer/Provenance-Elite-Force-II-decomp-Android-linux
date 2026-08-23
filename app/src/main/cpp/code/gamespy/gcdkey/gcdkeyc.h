
#ifndef _GOACDKEYC_H_
#define _GOACDKEYC_H_

#ifdef __cplusplus
extern "C" {
#endif

#define RESPONSE_SIZE 73

typedef enum 
{
	CDResponseMethod_NEWAUTH, 
	CDResponseMethod_REAUTH   
} CDResponseMethod;
	
	
void gcd_compute_response(char *cdkey, char *challenge, char response[73], CDResponseMethod method);


#ifdef __cplusplus
}
#endif

#endif

