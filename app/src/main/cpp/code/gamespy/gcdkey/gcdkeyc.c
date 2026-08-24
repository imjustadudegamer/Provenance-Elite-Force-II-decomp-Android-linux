/******
gcdkeyc.c
GameSpy CDKey SDK Client Code
  
Copyright 1999-2007 GameSpy Industries, Inc

devsupport@gamespy.com

******

 Please see the GameSpy CDKey SDK documentation for more 
 information

******/
#include "../md5.h"
#include "gcdkeyc.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define RAWSIZE 512

#ifdef __cplusplus
extern "C" {
#endif

void gcd_compute_response(char *cdkey, char *challenge, char response[RESPONSE_SIZE], CDResponseMethod method)
{
	char rawout[RAWSIZE];
	unsigned int anyrandom;
	char randstr[9];


	if (strlen(cdkey) * 2 + strlen(challenge) + 8 >= RAWSIZE)
	{
		strcpy(response,"CD Key or challenge too long");
		return;
	}

	srand((unsigned int)time(NULL) ^ 0x33333333);
	anyrandom = (rand() << 16 | rand()); 
	sprintf(randstr,"%.8x",anyrandom);

	if (method == 0)
		sprintf(rawout, "%s%d%s",cdkey, anyrandom % 0xFFFF , challenge );
	else
		sprintf(rawout, "%s%d%s",challenge, anyrandom % 0xFFFF, cdkey);

	MD5Digest((unsigned char *)cdkey, strlen(cdkey), response);
	strcpy(&response[32], randstr);
	MD5Digest((unsigned char *)rawout, strlen(rawout), &response[40]);	
}


#ifdef __cplusplus
}
#endif

