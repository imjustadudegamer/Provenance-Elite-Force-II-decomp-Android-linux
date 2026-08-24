#include <stdlib.h>
#include <string.h>
#include "sb_crypt.h"


static unsigned char keyrand(GOACryptState *state, int limit,
							 unsigned char *user_key,
							 unsigned char keysize,
							 unsigned char *rsum,
							 unsigned *keypos)
{
    unsigned int u,             
        retry_limiter,      
        mask;               
	
    if (!limit) return 0;   
    retry_limiter = 0;
    mask = 1;               
    while (mask < (unsigned)limit)    
        mask = (mask << 1) + 1;
    do
	{
        *rsum = (unsigned char)(state->cards[*rsum] + user_key[(*keypos)++]);
        if (*keypos >= keysize)
		{
            *keypos = 0;            
            *rsum = (unsigned char)(*rsum + keysize);   
		}
        u = mask & *rsum;
        if (++retry_limiter > 11)
            u %= (unsigned int)limit;     
	}
    while (u > (unsigned)limit);
    return (unsigned char)(u);
}


void GOAHashInit(GOACryptState *state)
{
	
    int i, j;
	
	
    state->rotor = 1;
    state->ratchet = 3;
    state->avalanche = 5;
    state->last_plain = 7;
    state->last_cipher = 11;
	
	
    for (i=0, j=255;i<256;i++,j--)
        state->cards[i] = (unsigned char) j;
}


void GOACryptInit(GOACryptState *state, unsigned char *key, unsigned char keysize)
{
	
    int i;
    unsigned char toswap, swaptemp, rsum;
    unsigned keypos;
	
	
    if (keysize < 1)
	{
        GOAHashInit(state);
        return;
	}
	
	
    for (i=0;i<256;i++)
        state->cards[i] = (unsigned char)(i);
	
	
    toswap = 0;
    keypos = 0;         
    rsum = 0;
    for (i=255;i>=0;i--)
	{
        toswap = keyrand(state, i, key, keysize, &rsum, &keypos);
        swaptemp = state->cards[i];
        state->cards[i] = state->cards[toswap];
        state->cards[toswap] = swaptemp;
	}
	
	
    state->rotor = state->cards[1];
    state->ratchet = state->cards[3];
    state->avalanche = state->cards[5];
    state->last_plain = state->cards[7];
    state->last_cipher = state->cards[rsum];
	
    toswap = swaptemp = rsum = 0;
    keypos = 0;
}


unsigned char GOAEncryptByte(GOACryptState *state, unsigned char b)
{
	
	
    unsigned char swaptemp;
	
	
    state->ratchet = (unsigned char)(state->ratchet + state->cards[state->rotor++]);
    swaptemp = state->cards[state->last_cipher];
    state->cards[state->last_cipher] = state->cards[state->ratchet];
    state->cards[state->ratchet] = state->cards[state->last_plain];
    state->cards[state->last_plain] = state->cards[state->rotor];
    state->cards[state->rotor] = swaptemp;
    state->avalanche = (unsigned char)(state->avalanche + state->cards[swaptemp]);
	
    state->last_cipher = (unsigned char)(b^state->cards[(state->cards[state->avalanche] + state->cards[state->rotor]) & 0xFF] ^
		state->cards[state->cards[(state->cards[state->last_plain] +
		state->cards[state->last_cipher] +
		state->cards[state->ratchet])&0xFF]]);
    state->last_plain = b;
    return state->last_cipher;
}


void GOAEncrypt(GOACryptState *state, unsigned char *bp, int len)
{
	int i;
	for (i = 0 ; i < len ; i++)
	{
		bp[i] = GOAEncryptByte(state, bp[i]);
	}
}

unsigned char GOADecryptByte(GOACryptState *state, unsigned char b)
{
    unsigned char swaptemp;
	
	
    state->ratchet = (unsigned char)(state->ratchet + state->cards[state->rotor++]);
    swaptemp = state->cards[state->last_cipher];
    state->cards[state->last_cipher] = state->cards[state->ratchet];
    state->cards[state->ratchet] = state->cards[state->last_plain];
    state->cards[state->last_plain] = state->cards[state->rotor];
    state->cards[state->rotor] = swaptemp;
    state->avalanche = (unsigned char)(state->avalanche + state->cards[swaptemp]);
	
	state->last_plain = (unsigned char)(b^state->cards[(state->cards[state->avalanche] + state->cards[state->rotor]) & 0xFF] ^
		state->cards[state->cards[(state->cards[state->last_plain] +
		state->cards[state->last_cipher] +
		state->cards[state->ratchet])&0xFF]]);
    state->last_cipher = b;
    return state->last_plain;
}

void GOADecrypt(GOACryptState *state, unsigned char *bp, int len)
{
	int i;
	for (i = 0 ; i < len ; i++)
	{
		bp[i] = GOADecryptByte(state, bp[i]);
	}
}

void GOAHashFinal(GOACryptState *state, unsigned char *outputhash,      
				  unsigned char hashlength) 
{
    int i;
	
    for (i=255;i>=0;i--)
        GOAEncryptByte(state, (unsigned char) i);
    for (i=0;i<hashlength;i++)
        outputhash[i] = GOAEncryptByte(state, 0);
}

