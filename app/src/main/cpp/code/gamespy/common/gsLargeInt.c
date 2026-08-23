#include "gsLargeInt.h"


static gsi_bool gsiLargeIntPrint(FILE* logFile, const l_word *data, l_word length);
static gsi_bool gsiLargeIntResize(gsLargeInt_t *lint, l_word length);
static gsi_bool gsiLargeIntStripLeadingZeroes(gsLargeInt_t* lint);
static gsi_bool gsiLargeIntSizePower2(const gsLargeInt_t *src1, const gsLargeInt_t *src2, l_word *lenout);
static gsi_i32  gsiLargeIntCompare(const l_word *data1, l_word len1, const l_word *data2, l_word len2);

static gsi_bool gsiLargeIntKMult(const l_word *data1, const l_word *data2, l_word length, l_word *dest, l_word *lenout, l_word maxlen);
static gsi_bool gsiLargeIntMult (const l_word *data1, l_word length1, const l_word *data2, l_word length2, l_word *dest, l_word *lenout, l_word maxlen);
static gsi_bool gsiLargeIntDiv  (const l_word *src1, l_word length1, const gsLargeInt_t *divisor, gsLargeInt_t *dest, gsLargeInt_t *remainder);

static gsi_bool gsiLargeIntAdd  (const l_word *data1, l_word length1, const l_word *data2, l_word length2, l_word *dest, l_word *lenout, l_word maxlen);
static gsi_bool gsiLargeIntSub  (const l_word *amount, l_word length1, const l_word *from, l_word length2, l_word *dest, l_word *lenout);

static gsi_bool gsiLargeIntSubDivide(l_word *src1, l_word length, const l_word *divisor, l_word dlen, gsi_u32 highbit, l_word *quotient);

gsi_bool gsiLargeIntMultM(gsLargeInt_t *src1, gsLargeInt_t *src2, const gsLargeInt_t *mod, gsi_u32 modPrime, gsLargeInt_t *dest);
gsi_bool gsiLargeIntInverseMod(const gsLargeInt_t *mod, l_word *modPrimeOut);


#define GS_LINT_TIMING
#ifdef GS_LINT_TIMING

typedef enum 
{
	GSLintTimerMult,  
	GSLintTimerMultM, 
	GSLintTimerKMult, 
	GSLintTimerAdd,
	GSLintTimerSub,   
	GSLintTimerDiv,
	GSLintTimerSubDivide, 
	GSLintTimerSquareMod,
	GSLintTimerPowerMod, 

	GSLintTimerCount
} GSLintTimerID;

typedef struct GSLintTimer
{
	gsi_time started;
	gsi_time total;
	gsi_u32  entries;
	gsi_u32  running; 
} GSLintTimer;
static struct GSLintTimer gTimers[GSLintTimerCount];

static void gsiLargeIntTimerEnter(GSLintTimerID id)
{
	if (gTimers[id].running==0)
	{
		gTimers[id].entries++;
		gTimers[id].started = current_time_hires();
		gTimers[id].running = 1;
	}
}
static void gsiLargeIntTimerExit(GSLintTimerID id)
{
	if (gTimers[id].running==1)
	{
		gTimers[id].total += current_time_hires()-gTimers[id].started;
		gTimers[id].running = 0;
	}
}

#define GSLINT_ENTERTIMER(id) gsiLargeIntTimerEnter(id)
#define GSLINT_EXITTIMER(id) gsiLargeIntTimerExit(id)

#else
#define GSLINT_ENTERTIMER(id)
#define GSLINT_EXITTIMER(id)
#endif
 



gsi_bool gsLargeIntSetValue(gsLargeInt_t *lint, l_word value)
{
	lint->mLength = 1;
	lint->mData[0] = value;
	return gsi_true;
}


gsi_bool gsiLargeIntResize(gsLargeInt_t *lint, l_word length)
{
	if (length > GS_LARGEINT_MAX_DIGITS)
		return gsi_false;

	if (lint->mLength >= length)
	{
		while(lint->mLength > length && lint->mData[lint->mLength-1]==0)
			lint->mLength--; 
		if (lint->mLength == length)
			return gsi_true;
		else
			return gsi_false;
	}

	else
	{
		memset(&lint->mData[lint->mLength], 0, (length-lint->mLength)*sizeof(l_word));
		lint->mLength = length;
		return gsi_true;
	}
}


gsi_bool gsiLargeIntSizePower2(const gsLargeInt_t *src1, const gsLargeInt_t *src2, l_word *lenout)
{
	unsigned int i = 0;

	int len1 = (int)src1->mLength;
	int len2 = (int)src2->mLength;

	while(len1>0 && src1->mData[len1-1] == 0)
		len1--;
	while(len2>0 && src2->mData[len2-1] == 0)
		len2--;

	*lenout = (l_word)max(len1, len2);
	
	i=1;
	while(i < *lenout)
		i = i<<1;
	*lenout = (l_word)i;

	if (*lenout > GS_LARGEINT_MAX_DIGITS)
		return gsi_false;
	return gsi_true;
}


static gsi_i32 gsiLargeIntCompare(const l_word *data1, l_word len1, const l_word *data2, l_word len2)
{
	while(data1[len1-1] == 0 && len1>0)
		len1--;
	while(data2[len2-1] == 0 && len2>0)
		len2--;
	if (len1<len2)
		return -1;
	else if (len1>len2)
		return 1;
	else 
	{
		while(len1 > 0)
		{
			if (data1[len1-1] < data2[len1-1])
				return -1;
			else if (data1[len1-1] > data2[len1-1])
				return 1;
			len1--;
		}
	}
	return 0; 
}


static gsi_bool gsiLargeIntStripLeadingZeroes(gsLargeInt_t* lint)
{
	while(lint->mLength >0 && lint->mData[lint->mLength-1]==0)
		lint->mLength--;
	return gsi_true;
}


gsi_bool gsLargeIntAdd(const gsLargeInt_t *src1, const gsLargeInt_t *src2, gsLargeInt_t *dest)
{
	gsi_bool result = gsiLargeIntAdd(src1->mData, src1->mLength, src2->mData, src2->mLength, dest->mData, &dest->mLength, GS_LARGEINT_MAX_DIGITS);
	if (gsi_is_false(result))
		memset(dest, 0, sizeof(gsLargeInt_t)); 
	return result;
}

static gsi_bool gsiLargeIntAdd(const l_word *data1, l_word length1, const l_word *data2, l_word length2, l_word *dest, l_word *lenout, l_word maxlen)
{
	gsi_u32 i=0;
	l_dword carry = 0; 

	gsi_u32 shorterLen = 0;
	gsi_u32 longerLen = 0;
	const l_word *longerSrc = NULL;

	GSLINT_ENTERTIMER(GSLintTimerAdd);

	if (maxlen < length1 || maxlen < length2)
		return gsi_false; 

	if (length1 < length2)
	{
		shorterLen = length1;
		longerLen = length2;
		longerSrc = data2;
	}
	else
	{
		shorterLen = length2;
		longerLen = length1;
		longerSrc = data1;
	}

	while(i < shorterLen)
	{
		carry += (l_dword)data1[i] + data2[i];
		dest[i] = (l_word)carry;
		carry = carry >> GS_LARGEINT_DIGIT_SIZE_BITS; 
		i++;
	}

	while((carry > 0) && (i < longerLen))
	{
		carry += (l_dword)longerSrc[i];
		dest[i] = (l_word)carry;
		carry = carry >> GS_LARGEINT_DIGIT_SIZE_BITS; 
		i++;
	}

	if (carry > 0) 
	{
		if (maxlen <= i)
			return gsi_false; 
		dest[i++] = (l_word)carry;
		carry = 0;
	}

	if (i < longerLen)
	{
		if (&dest[i] != &longerSrc[i])
			memcpy(&dest[i], &longerSrc[i], (longerLen-i)*sizeof(l_word));
		i = longerLen;
	}
	*lenout = (l_word)i;

	GSLINT_EXITTIMER(GSLintTimerAdd);

	if (carry)
		return gsi_false; 
	else
		return gsi_true;
}


gsi_bool gsLargeIntSub(const gsLargeInt_t *src1, const gsLargeInt_t *src2, gsLargeInt_t *dest)
{
	gsi_bool result = gsiLargeIntSub(src1->mData, src1->mLength, src2->mData, src2->mLength, dest->mData, &dest->mLength);
	if (gsi_is_true(result))
		gsiLargeIntStripLeadingZeroes(dest);
	return result;
}

gsi_bool gsiLargeIntSub(const l_word *src1, l_word length1, const l_word *src2, l_word length2, l_word *dest, l_word *lenout)
{
	l_dword borrow = 0; 
	gsi_u32 shorterLen = min(length1, length2);
	gsi_u32 i=0;

	GSLINT_ENTERTIMER(GSLintTimerSub);


	while(i < shorterLen)
	{
		borrow = (l_dword)src2[i] - src1[i] - borrow;
		dest[i] = (l_word)borrow;
		borrow = borrow>>63; 
		i++;
	}
	while(i < length2)
	{
		borrow = (l_dword)src2[i]-borrow;
		dest[i] = (l_word)borrow;
		borrow = borrow>>63;
		i++;
	}

	if (borrow != 0)
	{
		GSLINT_EXITTIMER(GSLintTimerSub);
		return gsi_false;
	}
	while(length1 > i) 
	{
		if (src1[i] != 0)
		{
			GSLINT_EXITTIMER(GSLintTimerSub);
			return gsi_false;
		}
		i++;
	}

	*lenout = length2;

	GSLINT_EXITTIMER(GSLintTimerSub);
	return gsi_true;
}


gsi_bool gsLargeIntMult(const gsLargeInt_t *src1, const gsLargeInt_t *src2, gsLargeInt_t *dest)
{
	gsi_bool result = gsiLargeIntMult(src1->mData, src1->mLength, src2->mData, src2->mLength, dest->mData, &dest->mLength, GS_LARGEINT_MAX_DIGITS);
	if (gsi_is_false(result))
		memset(dest, 0, sizeof(gsLargeInt_t)); 
	return result;
}

static gsi_bool gsiLargeIntMult(const l_word *data1, l_word length1, const l_word *data2, l_word length2, l_word *dest, l_word *lenout, l_word maxlen)
{
	unsigned int i=0;
	unsigned int k=0;

	gsLargeInt_t temp;
	memset(&temp, 0, sizeof(temp));
	*lenout = 0;

	GSLINT_ENTERTIMER(GSLintTimerMult);

	for(i=0; i<length2; i++)
	{
		if(data2[i] != 0)
		{
			for (k=0; k<length1; k++)
			{
				l_dword carry = (l_dword)data1[k] * data2[i];
				unsigned int digit = (unsigned int)(i+k);
				if (digit >= maxlen)
				{
					GSLINT_EXITTIMER(GSLintTimerMult);
					return gsi_false; 
				}
				while(carry)
				{
					carry += temp.mData[digit];
					temp.mData[digit] = (l_word)carry;
					carry = carry >> GS_LARGEINT_DIGIT_SIZE_BITS;
					digit++;
					if ((digit > maxlen) ||
						(digit == maxlen && carry>0))
					{
						GSLINT_EXITTIMER(GSLintTimerMult);
						return gsi_false; 
					}
				}
				if (digit > (gsi_i32)temp.mLength)
					temp.mLength = (l_word)digit;
			}
		}
	}
	while(temp.mLength>0 && temp.mData[temp.mLength-1] == 0)
		temp.mLength--; 
	*lenout = temp.mLength;
	memcpy(dest, temp.mData, (*lenout)*sizeof(l_word));

	GSLINT_EXITTIMER(GSLintTimerMult);
	return gsi_true;
}


gsi_bool gsLargeIntDiv(const gsLargeInt_t *src1, const gsLargeInt_t *divisor, gsLargeInt_t *dest, gsLargeInt_t *remainder)
{
	return gsiLargeIntDiv(src1->mData, src1->mLength, divisor, dest, remainder);
}

static gsi_bool gsiLargeIntDiv(const l_word *src, l_word len, const gsLargeInt_t *div, gsLargeInt_t *dest, gsLargeInt_t *remainder)
{
	gsi_i32 result = 0; 
	gsi_i32 divisorHighBit = GS_LARGEINT_DIGIT_SIZE_BITS-1; 

	int readIndex = 0;
	int readLength = 0;

	gsLargeInt_t quotient;

	l_word  scopy[GS_LARGEINT_MAX_DIGITS*2];  
	l_word  scopyLen = len;

	const l_word* divisorData = div->mData;
	l_word  divisorLen = div->mLength;

	gsi_bool endLoop = gsi_false;
	
	GSLINT_ENTERTIMER(GSLintTimerDiv);

	memset(scopy, 0, sizeof(scopy));

	if (scopyLen > GS_LARGEINT_MAX_DIGITS && dest != NULL)
		return gsi_false;

	while(scopyLen>0 && src[scopyLen-1]==0)
		scopyLen--;
	while(divisorLen>0 && divisorData[divisorLen-1]==0)
		divisorLen--;

	memcpy(scopy, src, scopyLen*sizeof(l_word));
	memset(&quotient, 0, sizeof(quotient)); 

	if (scopyLen==0 || divisorLen==0)
	{
		if (dest)
		{
			dest->mData[0] = 0;
			dest->mLength = 0;
		}
		if (remainder)
		{
			remainder->mData[0] = 0;
			remainder->mLength = 0;
		}

		GSLINT_EXITTIMER(GSLintTimerDiv);

		if (divisorLen == 0)
			return gsi_false; 
		else
			return gsi_true; 
	}
	if (gsiLargeIntCompare(scopy, scopyLen, divisorData, divisorLen)==-1)
	{
		if (dest)
		{
			dest->mLength = 0;
			dest->mData[0] = 0;
		}
		remainder->mLength = scopyLen;
		memcpy(remainder->mData, scopy, scopyLen*sizeof(l_word));
		GSLINT_EXITTIMER(GSLintTimerDiv);
		return gsi_true;
	}
	
	while((divisorData[divisorLen-1]&(1<<(gsi_u32)divisorHighBit))==0 && divisorHighBit>=0)
		divisorHighBit--;
	if (divisorHighBit == -1)
	{
		GSLINT_EXITTIMER(GSLintTimerDiv);
		return gsi_false; 
	}
	divisorHighBit += (divisorLen-1)*GS_LARGEINT_DIGIT_SIZE_BITS;
	
	readIndex = (int)(scopyLen - divisorLen);
	readLength = (int)divisorLen;

	
	do
	{
		result = gsiLargeIntCompare(&scopy[readIndex], (l_word)readLength, divisorData, divisorLen);
		if (result == -1)
		{
			if (readIndex > 0)
			{
				readIndex--; 
				readLength++;
			}
			else
			{
				endLoop = gsi_true;
			}
		}
		else if (result == 0)
		{
			memset(&scopy[readIndex], 0, readLength*sizeof(l_word));
			quotient.mData[readIndex] += 1;
			if (quotient.mLength < (l_word)(readIndex+readLength))
				quotient.mLength = (l_word)(readIndex+readLength);
			readIndex -= readLength;
			readLength = 1;

			if (readIndex < 0)
				endLoop = gsi_true;; 
		}
		else
		{
			l_word quotientTemp = 0;
			if (gsi_is_false(gsiLargeIntSubDivide(&scopy[readIndex], (l_word)readLength, divisorData, divisorLen, (gsi_u32)divisorHighBit, &quotientTemp)))
			{
				GSLINT_EXITTIMER(GSLintTimerDiv);
				return gsi_false;
			}
			quotient.mData[readIndex] = (l_word)(quotient.mData[readIndex] + quotientTemp);
			if (quotient.mLength < (l_word)(readIndex+readLength))
				quotient.mLength = (l_word)(readIndex+readLength);
			while(scopy[readIndex+readLength-1] == 0 && readLength>1)
				readLength--;
			while(scopy[readIndex+readLength-1] == 0 && readIndex>1)
				readIndex--;
		}
	}
	while(gsi_is_false(endLoop));

	if (readIndex >= 0)
	{
		memcpy(remainder->mData, &scopy[readIndex], readLength*sizeof(l_word));
		remainder->mLength = (l_word)readLength;
	}
	else
	{
		remainder->mData[0] = 0;
		remainder->mLength = 0;
	}

	if (dest)
	{
		memcpy(dest->mData, quotient.mData, quotient.mLength*sizeof(l_word));
		dest->mLength = quotient.mLength;
	}
	GSLINT_EXITTIMER(GSLintTimerDiv);
	return gsi_true;
}


static gsi_bool gsiLargeIntSubDivide(l_word *src, l_word length, const l_word *divisor, l_word dlen, 
									 gsi_u32 highbit, l_word *quotient)
{
	l_dword aboveBits = 0;
	gsLargeInt_t temp; 
	gsLargeInt_t quotientCopy; 

	GSLINT_ENTERTIMER(GSLintTimerSubDivide);

	if (length > dlen)
		aboveBits = (src[length-2]&divisor[dlen-1]) | ((l_dword)src[length-1]<<GS_LARGEINT_DIGIT_SIZE_BITS);
	else
		aboveBits = src[length-1];
	aboveBits /= divisor[dlen-1];

	memset(&quotientCopy, 0, sizeof(quotientCopy));
	quotientCopy.mData[0] = (l_word)(aboveBits);
	quotientCopy.mData[1] = (l_word)(aboveBits>>GS_LARGEINT_DIGIT_SIZE_BITS);

	if (quotientCopy.mData[1] != 0)
	{
		quotientCopy.mData[0] = (l_word)(-1);
		quotientCopy.mData[1] = 0;
	}
	quotientCopy.mLength = 1;
		
	if (gsi_is_false(gsiLargeIntMult(divisor, dlen, quotientCopy.mData, quotientCopy.mLength, temp.mData, &temp.mLength, GS_LARGEINT_MAX_DIGITS)))
	{
		GSLINT_EXITTIMER(GSLintTimerSubDivide);
		return gsi_false; 
	}

	while(gsiLargeIntCompare(temp.mData, temp.mLength, src, length)==1)
	{
		quotientCopy.mData[0] = (l_word)(quotientCopy.mData[0]>>1);
		if (gsi_is_false(gsiLargeIntMult(divisor, dlen, quotientCopy.mData, quotientCopy.mLength, temp.mData, &temp.mLength, GS_LARGEINT_MAX_DIGITS)))
		{
			GSLINT_EXITTIMER(GSLintTimerSubDivide);
			return gsi_false; 
		}
	}
			
	gsiLargeIntSub(temp.mData, temp.mLength, src, length, src, &length);

	*quotient = quotientCopy.mData[0];
	GSLINT_EXITTIMER(GSLintTimerSubDivide);

	GSI_UNUSED(highbit);
	return gsi_true;
}


gsi_bool gsLargeIntKMult(const gsLargeInt_t *src1, const gsLargeInt_t *src2, gsLargeInt_t *dest)
{
	l_word len = 0;
	gsi_bool result = gsi_false;

	gsLargeInt_t temp; 

	if (src1->mLength == 0 || src2->mLength == 0)
	{
		dest->mLength = 0;
		return gsi_true;
	}

	if (max(src1->mLength,src2->mLength) < GS_LARGEINT_KARATSUBA_CUTOFF)
		return gsLargeIntMult(src1, src2, dest);

	result = gsiLargeIntSizePower2(src1, src2, &len);
	if (gsi_is_false(result) || len>(GS_LARGEINT_MAX_DIGITS/2))
	{
		return gsLargeIntMult(src1, src2, dest); 
	}

	GSLINT_ENTERTIMER(GSLintTimerKMult);

	memset(&temp, 0, sizeof(gsLargeInt_t));
	temp.mLength = 0;

	if (src1->mLength != len || src2->mLength != len)
	{
		gsLargeInt_t src1Copy;
		gsLargeInt_t src2Copy;
		memcpy(&src1Copy, src1, sizeof(gsLargeInt_t));
		memcpy(&src2Copy, src2, sizeof(gsLargeInt_t));
		gsiLargeIntResize(&src1Copy, len);
		gsiLargeIntResize(&src2Copy, len);

		result = gsiLargeIntKMult(src1Copy.mData, src2Copy.mData, len, temp.mData, &temp.mLength, GS_LARGEINT_MAX_DIGITS);
	}
	else
	{
		result = gsiLargeIntKMult(src1->mData, src2->mData, len, temp.mData, &temp.mLength, GS_LARGEINT_MAX_DIGITS);
	}
	if (gsi_is_true(result))
	{
		gsiLargeIntStripLeadingZeroes(&temp);
		memcpy(dest, &temp, sizeof(gsLargeInt_t));
	}
	GSLINT_EXITTIMER(GSLintTimerKMult);
	return result;
}


static gsi_bool gsiLargeIntKMult(const l_word *data1, const l_word *data2, l_word length,
								 l_word *dest, l_word *lenout, l_word maxlen)
{

	if (length <= GS_LARGEINT_KARATSUBA_CUTOFF)
		return gsiLargeIntMult(data1, length, data2, length, dest, lenout, maxlen);
	else
	{
		gsLargeInt_t temp1, temp2, temp3;
		l_word halfLen = (l_word)(length>>1);

		temp1.mLength = 0;
		temp2.mLength = 0;
		temp3.mLength = 0;





		gsiLargeIntKMult(&data1[halfLen], &data2[halfLen], halfLen, &dest[length], lenout, (l_word)(maxlen-length));

		gsiLargeIntKMult(data1, data2, halfLen, dest, lenout, maxlen);

		gsiLargeIntAdd(&data1[halfLen], halfLen, data1, halfLen, temp1.mData, &temp1.mLength, GS_LARGEINT_MAX_DIGITS);

		gsiLargeIntAdd(&data2[halfLen], halfLen, data2, halfLen, temp2.mData, &temp2.mLength, GS_LARGEINT_MAX_DIGITS);

		memset(&temp3, 0, sizeof(gsLargeInt_t));
		
		if (temp1.mLength > halfLen || temp2.mLength > halfLen)
			gsiLargeIntMult(temp1.mData, temp1.mLength, temp2.mData, temp2.mLength, &temp3.mData[halfLen], &temp3.mLength, (l_word)(GS_LARGEINT_MAX_DIGITS-halfLen));
		else
		{
			gsi_bool result = gsiLargeIntSizePower2(&temp1, &temp2, lenout);
			if (gsi_is_false(result))
				return gsi_false; 
			gsiLargeIntResize(&temp1, *lenout); 
			gsiLargeIntResize(&temp2, *lenout); 
			gsiLargeIntKMult(temp1.mData, temp2.mData, *lenout, &temp3.mData[halfLen], &temp3.mLength, (l_word)(GS_LARGEINT_MAX_DIGITS-halfLen));
		}
		temp3.mLength = (l_word)(temp3.mLength + halfLen); 
		memset(temp3.mData, 0, halfLen*sizeof(l_word));

		gsiLargeIntSub(&dest[length], length, &temp3.mData[halfLen], (l_word)(temp3.mLength-halfLen), &temp3.mData[halfLen], &temp3.mLength);
		temp3.mLength = (l_word)(temp3.mLength + halfLen);
		gsiLargeIntSub( dest        , length, &temp3.mData[halfLen], (l_word)(temp3.mLength-halfLen), &temp3.mData[halfLen], &temp3.mLength);
		temp3.mLength = (l_word)(temp3.mLength + halfLen);

		gsiLargeIntAdd(dest, (l_word)(length*2), temp3.mData, temp3.mLength, dest, lenout, maxlen);
	}
	while(*lenout > 0 && dest[*lenout-1] == 0)
		*lenout = (l_word)(*lenout-1);

	return gsi_true;
}


gsi_bool gsLargeIntSquareMod(const gsLargeInt_t *lint, const gsLargeInt_t *mod, gsLargeInt_t *dest)
{
	int i = 0;
	int k = 0;
	int len = (int)lint->mLength; 
	l_dword carry = 0;
	int oldShiftBit = 0;
	int newShiftBit = 0;
	gsi_bool result = gsi_false;
	unsigned int mask = (unsigned int)1<<(GS_LARGEINT_DIGIT_SIZE_BITS-1);

	l_word squareSums[GS_LARGEINT_MAX_DIGITS*2];   
	l_word otherSums[GS_LARGEINT_MAX_DIGITS*2];    
	l_word squareLen = 0;
	l_word otherLen = 0;

	GSLINT_ENTERTIMER(GSLintTimerSquareMod);

	memset(&squareSums, 0, sizeof(squareSums));
	memset(&otherSums, 0, sizeof(otherSums));

	for (i=1; i < len; i++)
	{
		for(k=0; k < i; k++)
		{
			carry += (l_dword)lint->mData[i]*lint->mData[k] + otherSums[i+k];
			otherSums[i+k] = (l_word)carry;
			carry  = carry >> GS_LARGEINT_DIGIT_SIZE_BITS;
		}
		if(carry)
		{
			otherSums[i+k] = (l_word)carry;
			carry = carry >> GS_LARGEINT_DIGIT_SIZE_BITS;
		}
	}

	for (i=0; i < (2*len); i++)
	{
		newShiftBit = (otherSums[i] & mask)==mask?1:0; 
		otherSums[i] = (l_word)((otherSums[i] << 1) + oldShiftBit); 
		oldShiftBit = newShiftBit;
	}

	for (i=0; i <len; i++)
	{
		carry = (l_dword)lint->mData[i] * lint->mData[i];
		squareSums[i*2] = (l_word)carry;
		squareSums[i*2+1] = (l_word)(carry >> GS_LARGEINT_DIGIT_SIZE_BITS);
	}
	squareLen = (l_word)(2*len);
	otherLen = (l_word)(2*len); 

	result = gsiLargeIntAdd(otherSums, otherLen, squareSums, squareLen, squareSums, &squareLen, GS_LARGEINT_MAX_DIGITS*2);
	result = gsiLargeIntDiv(squareSums, squareLen, mod, NULL, dest);

	GSLINT_EXITTIMER(GSLintTimerSquareMod);
	return result;
}

#ifdef NEWEXP


gsi_bool gsLargeIntPowerMod(const gsLargeInt_t *b, const gsLargeInt_t *p, const gsLargeInt_t *m, gsLargeInt_t *dest)
{
	gsLargeInt_t base;
	gsLargeInt_t power;
	gsLargeInt_t mod;
	gsLargeInt_t one;

	gsi_u32 expHighBit; 

	int i = 0;        
	int k = 0;        
	int pow2k = 0;    
	int kmask = 0;    
	int kdigits = 0;  

	l_word modPrime;
	gsLargeInt_t R;     

	gsLargeInt_t * lut = NULL;

	GSLINT_ENTERTIMER(GSLintTimerPowerMod);

	memcpy(&base, b, sizeof(base));
	memcpy(&power, p, sizeof(power));
	memcpy(&mod, m, sizeof(mod));
	memset(&R, 0, sizeof(R));
	
	gsLargeIntSetValue(&one, 1);

	if (mod.mLength == 0)
	{
		dest->mLength = 0;
		GSLINT_EXITTIMER(GSLintTimerPowerMod);
		return gsi_false;
	}
	else if (mod.mLength==1 && mod.mData[0]==1)
	{
		dest->mLength = 0;
		dest->mData[0] = 0;
		GSLINT_EXITTIMER(GSLintTimerPowerMod);
		return gsi_true;
	}
	else if (power.mLength == 0)
	{
		dest->mLength = 1;
		dest->mData[0] = 1;
		GSLINT_EXITTIMER(GSLintTimerPowerMod);
		return gsi_true;
	}
	else if ((mod.mData[0]&1) == 0)
	{
		dest->mLength = 0;
		dest->mData[0] = 0;
		GSLINT_EXITTIMER(GSLintTimerPowerMod);
		return gsi_false;
	}
	if (gsiLargeIntCompare(base.mData, base.mLength, mod.mData, mod.mLength)!=-1)
	{
		gsLargeIntDiv(&base, &mod, NULL, &base);
	}
	if (base.mLength == 0)
	{
		dest->mLength = 0;
		dest->mData[0] = 0;
		GSLINT_EXITTIMER(GSLintTimerPowerMod);
		return gsi_true;
	}

	expHighBit=GS_LARGEINT_DIGIT_SIZE_BITS;
	while(((1<<(expHighBit-1))&power.mData[power.mLength-1]) == 0)
		expHighBit--;
	expHighBit += ((power.mLength-1) * GS_LARGEINT_DIGIT_SIZE_BITS); 

	k=8; 
	while ( (k > 1) && 
		(gsi_u32)((k - 1) * (k << ((k - 1) << 1)) / ((1 << k) - k - 1)) >= expHighBit - 1
		)
    {
      --k;
    }
	pow2k = 1 << k;
	kmask = pow2k-1;
	kdigits = (expHighBit+(k-1)) / k;  

	memset(&R, 0, sizeof(R));
	R.mLength = (l_word)(mod.mLength+1);
	if (R.mLength > GS_LARGEINT_MAX_DIGITS)
		return gsi_false; 
	R.mData[R.mLength-1] = 1; 

	gsiLargeIntInverseMod(&mod, &modPrime);

	{

		int i=0;
		int valuesNeeded = pow2k;
		int spaceneeded = sizeof(gsLargeInt_t) * valuesNeeded;

		lut = (gsLargeInt_t*)gsimalloc(spaceneeded);
		if (lut == NULL)
			return gsi_false; 
		memset(lut, 0x00, spaceneeded);

		gsLargeIntSetValue(&lut[0], 1);
		if (gsi_is_false(gsLargeIntMult(&base, &R, &lut[1])) ||
			gsi_is_false(gsLargeIntDiv(&lut[1], &mod, NULL, &lut[1])) )
		{
			gsifree(lut);
			GSLINT_EXITTIMER(GSLintTimerPowerMod);
			return gsi_false;
		}

		for (i=2; i < valuesNeeded; i++)
		{
			if (gsi_is_false(gsiLargeIntMultM(&lut[1], &lut[i-1], &mod, modPrime, &lut[i])) )
			{
				gsifree(lut);
				GSLINT_EXITTIMER(GSLintTimerPowerMod);
				return gsi_false;
			}
		}
	}

	if (gsi_is_false(gsLargeIntMult(&base, &R, dest)) ||      
		gsi_is_false(gsLargeIntDiv(dest, &mod, NULL, dest)) ) 
	{
		gsifree(lut);
		return gsi_false;
	}

	for (i=0; i < kdigits; i++)
	{
		int bitReadIndex = expHighBit - (i*k); 
		int l_index; 
		int l_firstbit;
		l_dword twodigits;
		l_dword mask;
		l_word digitval;

		l_index = ((bitReadIndex-1)/GS_LARGEINT_DIGIT_SIZE_BITS); 

		if ((bitReadIndex % k) != 0)
			bitReadIndex += k - (bitReadIndex % k); 
		if (i != 0)
		{
			if (bitReadIndex - (l_index*GS_LARGEINT_DIGIT_SIZE_BITS)> GS_LARGEINT_DIGIT_SIZE_BITS)
				l_index++;
		}

		if (i==0) 
		{
			l_firstbit = l_index * GS_LARGEINT_DIGIT_SIZE_BITS; 
			twodigits = p->mData[l_index];
		}
		else if (l_index > 0)
		{
			l_firstbit = (l_index-1) * GS_LARGEINT_DIGIT_SIZE_BITS; 
			twodigits = (l_dword)((l_dword)p->mData[l_index] << GS_LARGEINT_DIGIT_SIZE_BITS) | p->mData[l_index-1];
		}
		else if (l_index == 0 && p->mLength > 1)
		{
			l_firstbit = 0;
			twodigits = (l_dword)(p->mData[l_index+1] << GS_LARGEINT_DIGIT_SIZE_BITS) | p->mData[l_index];
		}
		else
		{
			l_firstbit = l_index * GS_LARGEINT_DIGIT_SIZE_BITS; 
			twodigits = p->mData[l_index];
		}
		mask = (l_dword)kmask << (bitReadIndex-l_firstbit-k);
		digitval = (l_word)((twodigits & mask) >> (bitReadIndex-l_firstbit-k));

		{
			static int twotab[] =
			{0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0,
			 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
			 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0,
			 3, 0, 1, 0, 2, 0, 1, 0, 7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
			 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 6, 0, 1, 0, 2, 0, 1, 0,
			 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
			 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0};


			static USHORT oddtab[] =
			{0, 1, 1, 3, 1, 5, 3, 7, 1, 9, 5, 11, 3, 13, 7, 15, 1, 17, 9, 19, 5, 21, 11, 23, 3, 25, 13, 27, 7, 29, 15, 31, 1,
			 33, 17, 35, 9, 37, 19, 39, 5, 41, 21, 43, 11, 45, 23, 47, 3, 49, 25, 51, 13, 53, 27, 55, 7, 57, 29, 59, 15,
			 61, 31, 63, 1, 65, 33, 67, 17, 69, 35, 71, 9, 73, 37, 75, 19, 77, 39, 79, 5, 81, 41, 83, 21, 85, 43, 87, 11,
			 89, 45, 91, 23, 93, 47, 95, 3, 97, 49, 99, 25, 101, 51, 103, 13, 105, 53, 107, 27, 109, 55, 111, 7, 113,
			 57, 115, 29, 117, 59, 119, 15, 121, 61, 123, 31, 125, 63, 127, 1, 129, 65, 131, 33, 133, 67, 135, 17,
			 137, 69, 139, 35, 141, 71, 143, 9, 145, 73, 147, 37, 149, 75, 151, 19, 153, 77, 155, 39, 157, 79, 159,
			 5, 161, 81, 163, 41, 165, 83, 167, 21, 169, 85, 171, 43, 173, 87, 175, 11, 177, 89, 179, 45, 181, 91,
			 183, 23, 185, 93, 187, 47, 189, 95, 191, 3, 193, 97, 195, 49, 197, 99, 199, 25, 201, 101, 203, 51, 205,
			 103, 207, 13, 209, 105, 211, 53, 213, 107, 215, 27, 217, 109, 219, 55, 221, 111, 223, 7, 225, 113,
			 227, 57, 229, 115, 231, 29, 233, 117, 235, 59, 237, 119, 239, 15, 241, 121, 243, 61, 245, 123, 247, 31,
			 249, 125, 251, 63, 253, 127, 255};


			if (i==0)
			{
				int counter = 0;

				memcpy(dest, &lut[oddtab[digitval]], sizeof(gsLargeInt_t));

				for (counter = twotab[digitval]; counter> 0; counter--)
				{
					if (gsi_is_false(gsiLargeIntMultM(dest,dest, &mod, modPrime, dest)))
					{
						gsifree(lut);
						return gsi_false;
					}
				}
			}
			else if (digitval != 0)
			{
				int counter = 0;
				int lutindex = oddtab[digitval]; 

				for (counter = (int)(k-twotab[digitval]); counter> 0; counter--)
				{
					if (gsi_is_false(gsiLargeIntMultM(dest,dest, &mod, modPrime, dest)))
					{
						gsifree(lut);
						return gsi_false;
					}
				}
		
				if (gsi_is_false(gsiLargeIntMultM(dest, &lut[lutindex], &mod, modPrime, dest)))
				{
					gsifree(lut);
					return gsi_false;
				}
				for (counter = twotab[digitval]; counter> 0; counter--)
				{
					if (gsi_is_false(gsiLargeIntMultM(dest,dest, &mod, modPrime, dest)))
					{
						gsifree(lut);
						return gsi_false;
					}
				}
			}
			else
			{
				int counter = 0;
				for (counter = k; counter > 0; counter--)
				{
					if (gsi_is_false(gsiLargeIntMultM(dest,dest, &mod, modPrime, dest)))
					{
						gsifree(lut);
						return gsi_false;
					}
				}
			}
		}
	}

	if (gsi_is_false(gsiLargeIntMultM(dest, &one, &mod, modPrime, dest)))
		return gsi_false;

	gsifree(lut);
	return gsi_true;
}

#else


gsi_bool gsLargeIntPowerMod(const gsLargeInt_t *b, const gsLargeInt_t *p, const gsLargeInt_t *m, gsLargeInt_t *dest)
{
	int i=0; 
	int digitNum=0; 
	int digitBit=0;
	
	l_word modPrime;

	gsi_u32 expHighBit; 

	gsLargeInt_t R; 
	gsLargeInt_t Rmod;   
	gsLargeInt_t R2mod;  
	gsLargeInt_t temp;
	gsLargeInt_t xwiggle; 

	gsLargeInt_t base;
	gsLargeInt_t power;
	gsLargeInt_t mod;

	GSLINT_ENTERTIMER(GSLintTimerPowerMod);

	memset(&R, 0, sizeof(R));
	memset(&Rmod, 0, sizeof(Rmod));
	memset(&R2mod, 0, sizeof(R2mod));
	memset(&temp, 0, sizeof(temp));
	memset(&xwiggle, 0, sizeof(xwiggle));

	memcpy(&base, b, sizeof(base));
	memcpy(&power, p, sizeof(power));
	memcpy(&mod, m, sizeof(mod));

	gsiLargeIntStripLeadingZeroes(&base);
	gsiLargeIntStripLeadingZeroes(&power);
	gsiLargeIntStripLeadingZeroes(&mod);

	if (mod.mLength == 0)
	{
		dest->mLength = 0;
		GSLINT_EXITTIMER(GSLintTimerPowerMod);
		return gsi_false;
	}
	else if (mod.mLength==1 && mod.mData[0]==1)
	{
		dest->mLength = 0;
		dest->mData[0] = 0;
		GSLINT_EXITTIMER(GSLintTimerPowerMod);
		return gsi_true;
	}
	else if (power.mLength == 0)
	{
		dest->mLength = 1;
		dest->mData[0] = 1;
		GSLINT_EXITTIMER(GSLintTimerPowerMod);
		return gsi_true;
	}
	else if ((mod.mData[0]&1) == 0)
	{
		dest->mLength = 0;
		dest->mData[0] = 0;
		GSLINT_EXITTIMER(GSLintTimerPowerMod);
		return gsi_false;
	}
	if (gsiLargeIntCompare(base.mData, base.mLength, mod.mData, mod.mLength)!=-1)
	{
		gsLargeIntDiv(&base, &mod, NULL, &base);
	}
	if (base.mLength == 0)
	{
		dest->mLength = 0;
		dest->mData[0] = 0;
		GSLINT_EXITTIMER(GSLintTimerPowerMod);
		return gsi_true;
	}

	expHighBit=GS_LARGEINT_DIGIT_SIZE_BITS;
	while(((1<<(expHighBit-1))&power.mData[power.mLength-1]) == 0)
		expHighBit--;
	expHighBit += ((power.mLength-1) * GS_LARGEINT_DIGIT_SIZE_BITS); 
	


	memset(&R, 0, sizeof(R));
	R.mLength = (l_word)(mod.mLength+1);
	if (R.mLength > GS_LARGEINT_MAX_DIGITS)
		return gsi_false; 
	R.mData[R.mLength-1] = 1; 

	gsiLargeIntInverseMod(&mod, &modPrime);

	if (gsi_is_false(gsLargeIntDiv(&R, &mod, NULL, &Rmod)))
	{
		GSLINT_EXITTIMER(GSLintTimerPowerMod);
		return gsi_false;
	}

	if (gsi_is_false(gsLargeIntSquareMod(&Rmod, &mod, &R2mod)))
	{
		GSLINT_EXITTIMER(GSLintTimerPowerMod);
		return gsi_false;
	}

	if (gsi_is_false(gsiLargeIntMultM(&base, &R2mod, &mod, modPrime, &xwiggle)))
	{
		GSLINT_EXITTIMER(GSLintTimerPowerMod);
		return gsi_false;
	}

	memcpy(dest, &Rmod, sizeof(gsLargeInt_t)); 
	for (i=(int)(expHighBit-1); i>=0; i--)
	{
		gsiLargeIntMultM(dest, dest, &mod, modPrime, dest);
		digitNum = (gsi_i32)(i/GS_LARGEINT_DIGIT_SIZE_BITS);    
		digitBit = (gsi_i32)(i % GS_LARGEINT_DIGIT_SIZE_BITS);  
		
		{
			GS_LARGEINT_DIGIT_TYPE digit = power.mData[digitNum];
			GS_LARGEINT_DIGIT_TYPE mask = (GS_LARGEINT_DIGIT_TYPE)(1<<digitBit); 
			GS_LARGEINT_DIGIT_TYPE masked = digit & mask; 

			if (mask == masked)
				gsiLargeIntMultM(dest, &xwiggle, &mod, modPrime, dest);
		}
	}

	temp.mLength = 1;
	temp.mData[0] = 1;
	gsiLargeIntMultM(dest, &temp, &mod, modPrime, dest);

	GSLINT_EXITTIMER(GSLintTimerPowerMod);
	return gsi_true;
}

#endif


#define NEWMULTM
#ifdef  NEWMULTM

gsi_bool gsiLargeIntMultM(gsLargeInt_t *x, gsLargeInt_t *y, const gsLargeInt_t *m, gsi_u32 modPrime, gsLargeInt_t *dest)
{
	l_word tempLen = 0;
	l_word temp[GS_LARGEINT_MAX_DIGITS*2];

	l_word* lasttnptr;
	const l_word* lastnptr;
	l_word* tptr;
	const l_word* nptr;
	l_word* tiptr;
	
	l_dword carry = 0;
	l_word mi = 0;

	l_word logB_r = m->mLength;

	memset(temp, 0, sizeof(temp));

	if (gsi_is_false(gsiLargeIntMult(x->mData, x->mLength, y->mData, y->mLength, temp, &tempLen, GS_LARGEINT_MAX_DIGITS*2)))
		return gsi_false;
	
	lasttnptr = &temp[m->mLength-1];
	lastnptr = &m->mData[m->mLength-1];

	if (tempLen < m->mLength*2)
	{
		memset(&temp[tempLen], 0, (m->mLength*2 - tempLen) * GS_LARGEINT_DIGIT_SIZE_BYTES);
		tempLen = (l_word)(m->mLength*2);
	}

	for (tptr = &temp[0]; tptr <= lasttnptr; tptr++)
	{
		carry = 0;
		mi = (l_word)((l_dword)modPrime * (l_dword)*tptr);
		tiptr = tptr;
		for (nptr = &m->mData[0]; nptr <= lastnptr; nptr++, tiptr++)
		{
			carry = (l_dword)mi * (l_dword)*nptr +
				    (l_dword)*tiptr + (l_dword)(l_word)(carry >> GS_LARGEINT_DIGIT_SIZE_BITS);
			*tiptr = (l_word)(carry);
		}

		for (; ((carry >> GS_LARGEINT_DIGIT_SIZE_BITS) > 0) && tiptr <= &temp[tempLen-1]; tiptr++)
		{
			*tiptr = (l_word)(carry = (l_dword)*tiptr + (l_dword)(l_word)(carry >> GS_LARGEINT_DIGIT_SIZE_BITS));
		}

		if (((carry >> GS_LARGEINT_DIGIT_SIZE_BITS) > 0))
		{
			*tiptr = (l_word)(carry >> GS_LARGEINT_DIGIT_SIZE_BITS);
			tempLen++;
		}
	}

	if (gsiLargeIntCompare(&temp[logB_r], tempLen - logB_r, m->mData, m->mLength) != -1)
	{
		if (gsi_is_false(gsiLargeIntSub(m->mData, m->mLength, &temp[logB_r], tempLen - logB_r, dest->mData, &dest->mLength)))
		{
			memset(temp, 0, sizeof(temp));
			memset(dest, 0, sizeof(gsLargeInt_t));
			return gsi_false;
		}
	}
	else
	{
		memset(dest, 0, sizeof(gsLargeInt_t));
		dest->mLength = m->mLength;
		memcpy(dest->mData, &temp[logB_r], (tempLen - logB_r)*GS_LARGEINT_DIGIT_SIZE_BYTES);
		memset(temp, 0, sizeof(temp));
	}

	return gsi_true;
}

#else


gsi_bool gsiLargeIntMultM(gsLargeInt_t *x, gsLargeInt_t *y, const gsLargeInt_t *m, gsi_u32 modPrime, gsLargeInt_t *dest)
{
	int i=0;
	l_dword xiy0;
	l_word u = 0;

	gsLargeInt_t A;
	gsLargeInt_t xiy;
	gsLargeInt_t temp; 

	GSLINT_ENTERTIMER(GSLintTimerMultM);

	gsiLargeIntStripLeadingZeroes(x);
	gsiLargeIntStripLeadingZeroes(y);

	i=(int)(m->mLength);
	while(i>0 && m->mData[i-1]==0)
		i--;
	if (i==0)
	{
		dest->mData[0] = 0;
		dest->mLength = 0;
		GSLINT_EXITTIMER(GSLintTimerMultM);
		return gsi_false;
	}
	if (x->mLength==0)
	{
		dest->mLength = 0;
		dest->mData[0] = 0;
		GSLINT_EXITTIMER(GSLintTimerMultM);
		return gsi_true;
	}
	if (y->mLength==0)
	{
		dest->mLength = 0;
		dest->mData[0] = 0;
		GSLINT_EXITTIMER(GSLintTimerMultM);
		return gsi_true;
	}

	gsiLargeIntResize(x, m->mLength);
	gsiLargeIntResize(y, m->mLength);

	memset(&A, 0, sizeof(A));
	memset(&temp, 0, sizeof(temp));
	memset(&xiy, 0, sizeof(xiy));
	
	for (i=0; (gsi_u32)i < m->mLength; i++)
	{
		xiy0 = (l_dword)x->mData[i]*y->mData[0];  
		u = (l_word)((xiy0+A.mData[0])*modPrime); 

		memset(temp.mData, 0, y->mLength*sizeof(l_word)); 
		temp.mData[0] = x->mData[i];
		temp.mLength = y->mLength; 
		if (gsi_is_false(gsiLargeIntMult(temp.mData, temp.mLength, y->mData, y->mLength, xiy.mData, &xiy.mLength, GS_LARGEINT_MAX_DIGITS)))
		{
			dest->mLength = 0;
			dest->mData[0] = 0;
			GSLINT_EXITTIMER(GSLintTimerMultM);
			return gsi_false;
		}
		memset(temp.mData, 0, m->mLength*sizeof(l_word)); 
		temp.mData[0] = u;
		temp.mLength = m->mLength;
		if (gsi_is_false(gsLargeIntKMult(&temp, m, &temp)))
		{
			dest->mLength = 0;
			dest->mData[0] = 0;
			GSLINT_EXITTIMER(GSLintTimerMultM);
			return gsi_false;
		}
		if (gsi_is_false(gsiLargeIntAdd(xiy.mData, xiy.mLength, A.mData, A.mLength, A.mData, &A.mLength, GS_LARGEINT_MAX_DIGITS)))
		{
			dest->mLength = 0;
			dest->mData[0] = 0;
			GSLINT_EXITTIMER(GSLintTimerMultM);
			return gsi_false;
		}
		if (gsi_is_false(gsiLargeIntAdd(temp.mData, temp.mLength, A.mData, A.mLength, A.mData, &A.mLength, GS_LARGEINT_MAX_DIGITS)))
		{
			dest->mLength = 0;
			dest->mData[0] = 0;
			GSLINT_EXITTIMER(GSLintTimerMultM);
			return gsi_false;
		}
		if (A.mLength > 1)
		{
			memmove(&A.mData[0], &A.mData[1], (A.mLength-1)*sizeof(l_word));
			A.mData[A.mLength-1] = 0;
			A.mLength--;
		}
		else
		{
			A.mLength = 0;
			A.mData[0] = 0;
		}
	}

	if (gsiLargeIntCompare(A.mData, A.mLength, m->mData, m->mLength)!=-1)
		gsiLargeIntSub(m->mData, m->mLength, A.mData, A.mLength, dest->mData, &dest->mLength);
	else
		memcpy(dest, &A, sizeof(A));
	GSLINT_EXITTIMER(GSLintTimerMultM);
	return gsi_true;
}

#endif


gsi_bool gsiLargeIntInverseMod(const gsLargeInt_t *mod, l_word *dest)
{
	l_dword x=2;
	l_dword y=1;
	l_dword check = 0;

	gsi_u32 i;
	for (i = 2; i <= GS_LARGEINT_DIGIT_SIZE_BITS; i++)
	{
		check = (l_dword)mod->mData[0] * (l_dword)y;
		if (x < (check & ((x<<1)-1)))
			y += x;
		x = x << 1;
	}
	*dest = (l_word)(x-y);
	return gsi_true;
}


gsi_bool gsLargeIntPrint(FILE* logFile, const gsLargeInt_t *lint)
{
	return gsiLargeIntPrint(logFile, lint->mData, lint->mLength);
}


gsi_bool gsiLargeIntPrint(FILE* logFile, const l_word *data, l_word length)
{
#if !defined(_NITRO)
	while(length >0)
	{
		fprintf(logFile, "%08X", data[length-1]);
		length--;
	}
	fprintf(logFile, "\r\n");
	return gsi_true;
#else
	GSI_UNUSED(logFile);
	GSI_UNUSED(data);
	GSI_UNUSED(length);
	return gsi_false;			
#endif
}


gsi_bool gsLargeIntSetFromHexString(gsLargeInt_t *lint, const char* hexstream)
{
	l_word* writePos = lint->mData;
	gsi_u32 temp;
	int len = 0;
	int byteIndex = 0;

	GS_ASSERT(hexstream != NULL);
	
	len = (int)strlen(hexstream);
	if (len == 0)
	{
		lint->mLength = 0;
		lint->mData[0] = 0;
		return gsi_true;
	}
	if ((len/2) > (GS_LARGEINT_MAX_DIGITS*GS_LARGEINT_DIGIT_SIZE_BYTES))
		return gsi_false;
	
	lint->mLength = (l_word)((len+(2*GS_LARGEINT_DIGIT_SIZE_BYTES-1))/(2*GS_LARGEINT_DIGIT_SIZE_BYTES));
	lint->mData[lint->mLength-1] = 0; 
	
	while(len > 0)
	{
		if(len >= 2)
			sscanf((char*)(hexstream+len-2), "%02x", &temp); 
		else
			sscanf((char*)(hexstream+len-1), "%01x", &temp); 
		if(byteIndex == 0)
			*writePos = 0;
		*writePos |= (temp << (byteIndex * 8));
		if(++byteIndex == GS_LARGEINT_DIGIT_SIZE_BYTES)
		{
			writePos++;
			byteIndex = 0;
		}
		len-=min(2,len);
	}
	return gsi_true;
}


gsi_bool gsLargeIntReverseBytes(gsLargeInt_t *lint)
{
#if defined(GSI_LITTLE_ENDIAN)
	char *left = (char*)&lint->mData[0];
	char *right = ((char*)&lint->mData[lint->mLength])-1;
	char  temp;
#else
	l_word *left = lint->mData;
	l_word *right = lint->mData + (lint->mLength - 1);
	l_word  temp;
#endif


	if (lint->mLength == 0)
		return gsi_true;

	while(left < right)
	{
		temp = *left;
		(*left++) = (*right);
		(*right--) = temp;
	}
	return gsi_true;
}


void gsLargeIntAddToMD5(const gsLargeInt_t * _lint, MD5_CTX * md5)
{
	int byteLength = 0;
	gsi_u8 * dataStart = NULL;

	gsLargeInt_t lint;
	memcpy(&lint, _lint, sizeof(lint));

	byteLength = (int)gsLargeIntGetByteLength(&lint);
	if (byteLength == 0)
		return; 

	dataStart = (gsi_u8*)lint.mData;
	if ((byteLength % GS_LARGEINT_DIGIT_SIZE_BYTES) != 0)
		dataStart += GS_LARGEINT_DIGIT_SIZE_BYTES - (byteLength % GS_LARGEINT_DIGIT_SIZE_BYTES);

	gsLargeIntReverseBytes(&lint);
	MD5Update(md5, dataStart, (unsigned int)byteLength);
	gsLargeIntReverseBytes(&lint);
} 


gsi_u32  gsLargeIntGetByteLength(const gsLargeInt_t *lint)
{
	int intSize = (int)lint->mLength;
	int byteSize = 0;
	int i=0;
	l_word mask = 0xFF;

	while(intSize > 0 && lint->mData[intSize-1] == 0)
		intSize --;
	if (intSize == 0)
		return 0; 

	byteSize = intSize * (gsi_i32)sizeof(l_word);

	mask = 0xFF;
	for (i=1; i < GS_LARGEINT_DIGIT_SIZE_BYTES; i++)
	{
		if (lint->mData[intSize-1] <= mask)
		{
			byteSize -= sizeof(l_word)-i;
			break;
		}
		mask = (l_word)((mask << 8) | 0xFF);
	}

	return (gsi_u32)byteSize;
}


gsi_bool gsLargeIntSetFromMemoryStream(gsLargeInt_t *lint, const gsi_u8* data, gsi_u32 len)
{
	lint->mData[0] = 0;
	memcpy(((char*)lint->mData)+(4-len%4)%4, data, len);

	lint->mLength = (unsigned int)((len+(GS_LARGEINT_DIGIT_SIZE_BYTES-1))/GS_LARGEINT_DIGIT_SIZE_BYTES);

	return gsLargeIntReverseBytes(lint);
}


gsi_bool gsLargeIntWriteToMemoryStream(const gsLargeInt_t *lint, gsi_u8* data)
{
	gsLargeInt_t copy;
	memcpy(&copy, lint, sizeof(gsLargeInt_t));

	gsLargeIntReverseBytes(&copy);

	memcpy(data, copy.mData, copy.mLength * GS_LARGEINT_DIGIT_SIZE_BYTES);
	return gsi_true;
}
