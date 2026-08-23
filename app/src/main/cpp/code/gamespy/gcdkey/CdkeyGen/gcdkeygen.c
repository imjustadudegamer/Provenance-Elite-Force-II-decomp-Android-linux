
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <windows.h>

static void Util_RandSeed(unsigned long seed);
static int Util_RandInt(int low, int high);


#define MINKEYSEED 0x0000100000
#define MAXKEYSEED 0xFFFFFFFFFF
void DoGenerateKeys()
{
	char resp[128] = "";
	char key[128];
	char hexstr[20];
	int keyct;
	unsigned int interval;
	__int64 offset; 
	__int64 base = MINKEYSEED;
	__int64 seed;
	short check;
	FILE *f;

	Util_RandSeed(time(NULL) ^ GetTickCount());
	printf("How many keys would you like? ");
	gets(resp);

	keyct = atoi(resp);

	offset = (MAXKEYSEED - MINKEYSEED) / keyct;
	if (offset > 0xFFFFFFFF) 
		interval = 0xFFFFFFFF;
	else
		interval = (unsigned int)offset;


	f = fopen("keys.txt","w");
	if (!f)
		return;
	while (keyct != 0)
	{
		seed = base + Util_RandInt(0,interval); 
		base += offset; 
		sprintf(hexstr,"%.10I64x",seed);

		check = ((short)(seed % 65535)) ^ 0x9249; 

		sprintf(&key[11],"%.8x",check); 
		key[0] = '0' + (char)(Util_RandInt(0,9)); 
		strncpy(&key[1],hexstr,3); 
		key[4] = '-';
		strncpy(&key[5],&hexstr[3],4); 
		key[9] = '-';
		strncpy(&key[10],&hexstr[7],3); 
		key[13] = '0' + (char)(Util_RandInt(0,9)); 
		key[14] = '-';

		fprintf(f,"%s\n",key); 
		keyct--;
	}
	fclose(f);
	printf("Keys output to keys.txt\n");
}

int ValidateKey(char *key)
{
	__int64 seed;
	char hexstr[20] = "0x";
	int check;
	short realcheck;

	strncpy(hexstr + 2,key + 1,3);
	strncpy(hexstr + 5, key + 5, 4);
	strncpy(hexstr + 9, key + 10, 3);
	hexstr[12] = 0;
	sscanf(hexstr,"%I64x",&seed);

	strncpy(hexstr + 2, key + 15, 4);
	hexstr[6] = 0;
	sscanf(hexstr, "%x",&check);

	realcheck = ((short)(seed % 65535)) ^ 0x9249;
	return ((short)check == realcheck);
}

void DoValidateKeys()
{
	char resp[128] = "";

	do
	{	
		printf("Enter a CD Key to validate, or [ENTER] to quit: ");
		gets(resp);
		if (!resp[0])
			return;
		if (ValidateKey(resp))
			printf("Key Validated\n");
		else
			printf("Invalid Key\n");

	} while (1);


}

int main(int argc, char **argv)
{
	char resp[10];
	printf("What would you like to do?\n\t1. Generate CD Keys\n\t2. Validate a CD Key\n:");
	gets(resp);
	if (resp[0] == '1')
		DoGenerateKeys();
	else
		DoValidateKeys();
}

#define RANa            16807         
#define LONGRAND_MAX    2147483647L   

static long randomnum = 1;

static long nextlongrand(long seed)
{
	unsigned long lo, hi;
	
	lo = RANa * (long)(seed & 0xFFFF);
	hi = RANa * (long)((unsigned long)seed >> 16);
	lo += (hi & 0x7FFF) << 16;
	if (lo > LONGRAND_MAX)
	{
		lo &= LONGRAND_MAX;
		++lo;
	}
	lo += hi >> 15;
	if (lo > LONGRAND_MAX)
	{
		lo &= LONGRAND_MAX;
		++lo;
	}
	return (long)lo;
}

static long longrand(void)                     
{
	randomnum = nextlongrand(randomnum);
	return randomnum;
}

static void Util_RandSeed(unsigned long seed)      
{
	randomnum = seed ? (seed & LONGRAND_MAX) : 1;  
}

static int Util_RandInt(int low, int high)
{
	int  range = high-low;
	int  num = longrand() % range;

	return(num + low);
}
