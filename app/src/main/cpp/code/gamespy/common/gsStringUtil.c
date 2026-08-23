#include "gsCommon.h"
#include "gsStringUtil.h"

#ifdef __cplusplus
extern "C" {
#endif	

int _ReadUCS2CharFromUTF8String(const UTF8String theUTF8String,  UCS2Char* theUnicodeChar, int theMaxLength)
{
#ifndef _PS2
	assert(theUnicodeChar != NULL);
#endif

	if (theMaxLength == 0)
	{
		*theUnicodeChar = (UCS2Char)REPLACE_INVALID_CHAR;
		return 0; 
	}

	if (UTF8_IS_SINGLE_BYTE(theUTF8String[0]))
	{
		*theUnicodeChar = (UCS2Char)theUTF8String[0];
		return 1;
	}

	else if (UTF8_IS_TWO_BYTE(theUTF8String[0]))
	{
		if (theMaxLength < 2)
		{
			*theUnicodeChar = (UCS2Char)REPLACE_INVALID_CHAR;
			return 0; 
		}

		if (UTF8_IS_FOLLOW_BYTE(theUTF8String[1]))
		{
			*theUnicodeChar  =	(unsigned short)(((theUTF8String[0] & UTF8_TWO_BYTE_MASK) << 6) +
								((theUTF8String[1] & UTF8_FOLLOW_BYTE_MASK)));
			return 2;
		}
	}

	else if (UTF8_IS_THREE_BYTE(theUTF8String[0]))
	{
		if (theMaxLength < 3)
		{
			*theUnicodeChar = (UCS2Char)REPLACE_INVALID_CHAR;
			return 0; 
		}

		if (UTF8_IS_FOLLOW_BYTE(theUTF8String[1]) &&
			UTF8_IS_FOLLOW_BYTE(theUTF8String[2]))
		{
			*theUnicodeChar  =	(unsigned short)(((theUTF8String[0] & UTF8_THREE_BYTE_MASK) << 12) +
								((theUTF8String[1] & UTF8_FOLLOW_BYTE_MASK) << 6) +
								((theUTF8String[2] & UTF8_FOLLOW_BYTE_MASK)));
			return 3;	
		}
	}

	*theUnicodeChar = (UCS2Char)REPLACE_INVALID_CHAR;

	return 1; 
}


int _UCS2CharToUTF8String(UCS2Char theUCS2Char, UTF8String theUTF8String)
{
#ifndef _PS2
	assert(theUTF8String != NULL);
#endif

	if (theUCS2Char <= 0x7F)
	{
		theUTF8String[0] = (char)(UTF8ByteType)theUCS2Char;
		return 1;
	}
	else if (theUCS2Char <= 0x07FF)
	{
		theUTF8String[0] = (char)(UTF8ByteType)(UTF8_TWO_BYTE_TAG | (theUCS2Char >> 6));				 
		theUTF8String[1] = (char)(UTF8ByteType)(UTF8_FOLLOW_BYTE_TAG | (theUCS2Char & UTF8_FOLLOW_BYTE_MASK));	
		return 2;
	}
	else
	{
		theUTF8String[0] = (char)(UTF8ByteType)(UTF8_THREE_BYTE_TAG |  (theUCS2Char >> 12));					 
		theUTF8String[1] = (char)(UTF8ByteType)(UTF8_FOLLOW_BYTE_TAG | ((theUCS2Char >> 6) & UTF8_FOLLOW_BYTE_MASK));	
		theUTF8String[2] = (char)(UTF8ByteType)(UTF8_FOLLOW_BYTE_TAG | ((theUCS2Char) & UTF8_FOLLOW_BYTE_MASK));			
		return 3;
	}
}


int AsciiToUTF8String(const char* theAsciiString, UTF8String theUTF8String)
{
	if (theAsciiString == NULL)
	{
		*theUTF8String = 0x00;
		return 1;
	}
	else
	{
		unsigned int aLength = 0;
		while (*theAsciiString != '\0')
		{
			*(theUTF8String++) = *(theAsciiString++);
			aLength++;
		} 

		*theUTF8String = '\0';
		aLength++;

		return (int)aLength;
	}
}


int UTF8ToAsciiString(const UTF8String theUTF8String, char* theAsciiString)
{
	const unsigned char* anInStream = (const unsigned char*)theUTF8String;
	unsigned int   aNumBytesWritten = 0;

	if (theUTF8String == NULL)
	{
		*theAsciiString = 0x00;
		return 1;
	}

	while (*anInStream != '\0')
	{
		if (UTF8_IS_SINGLE_BYTE(*anInStream))
			theAsciiString[aNumBytesWritten++] = (char)*anInStream;
		else
			theAsciiString[aNumBytesWritten++] = REPLACE_INVALID_CHAR;

		anInStream++;
	}

	theAsciiString[aNumBytesWritten++] = '\0';
	return (int)aNumBytesWritten;
}


int UCS2ToUTF8String(const UCS2String theUCS2String, UTF8String theUTF8String)
{
	unsigned int	aTotalBytesWritten	= 0;
	unsigned int	aUTF8CharLength		= 0;
	const UCS2Char*	anInStream			= theUCS2String;
	unsigned char*	anOutStream			= (unsigned char*)theUTF8String;

	if (theUCS2String == NULL)
	{
		*anOutStream = 0x00;
		return 1;
	}

	while(*anInStream != 0)
	{
		aUTF8CharLength = (unsigned int)_UCS2CharToUTF8String(*anInStream, (UTF8String)anOutStream);

		anOutStream += aUTF8CharLength;

		anInStream++;

		aTotalBytesWritten += aUTF8CharLength;
	}
	
	*anOutStream = '\0';
	aTotalBytesWritten++;

	return (int)aTotalBytesWritten;
}


int UTF8ToUCS2String(const UTF8String theUTF8String, UCS2String theUCS2String)
{
	return UTF8ToUCS2StringLen(theUTF8String, theUCS2String, (gsi_i32)strlen(theUTF8String));
}


int _UTF8ToUCS2ConversionLengthOnly(const UTF8String theUTF8String)
{
	int length = 0;
	const UTF8String theReadPos = theUTF8String;

	assert(theUTF8String != NULL);
	if (theUTF8String == NULL)
		return 0;

	while (*theReadPos != '\0')
	{
		if (UTF8_IS_TWO_BYTE(theReadPos[0]) && UTF8_IS_FOLLOW_BYTE(theReadPos[1]))
			theReadPos += 2;

		else if (UTF8_IS_THREE_BYTE(theReadPos[0]) && 
				 UTF8_IS_FOLLOW_BYTE(theReadPos[1]) &&
				 UTF8_IS_FOLLOW_BYTE(theReadPos[2]))
		{
			theReadPos += 3;
		}
		else
			theReadPos++;

		length++;
	}

	return length;
}


int _UCS2ToUTF8ConversionLengthOnly(const UCS2String theUCS2String)
{
	int length = 0;
	const UCS2String theReadPos = theUCS2String;
	assert(theUCS2String != NULL);
	while (*theReadPos != 0x0000)
	{
		if (*theReadPos <= 0x7F)
			length++;
		else if (*theReadPos <= 0x07FF) 
			length += 2;
		else
			length += 3;

		theReadPos++;
	}

	return length;
}


UCS2String UTF8ToUCS2StringAlloc(const UTF8String theUTF8String)
{
	if (theUTF8String == NULL)
		return NULL;
	else
	{
		int newLength = _UTF8ToUCS2ConversionLengthOnly(theUTF8String);
		UCS2String aUCS2String = (UCS2String)gsimalloc(sizeof(UCS2Char)*(newLength + 1));

		UTF8ToUCS2String(theUTF8String, aUCS2String);

		return aUCS2String;
	}
}


UTF8String UCS2ToUTF8StringAlloc(const UCS2String theUCS2String)
{
	if (theUCS2String == NULL)
		return NULL;
	else
	{
		int newLength			= _UCS2ToUTF8ConversionLengthOnly(theUCS2String);
		UTF8String aUTF8String	= (UTF8String)gsimalloc(sizeof(char)*(newLength + 1));

		UCS2ToUTF8String(theUCS2String, aUTF8String);

		return aUTF8String;
	}
}


UCS2String* UTF8ToUCS2StringArrayAlloc(const UTF8String* theUTF8StringArray, int theNumStrings)
{
	if(theUTF8StringArray == NULL || theNumStrings == 0)
		return NULL;
	else
	{
		UCS2String* aUCS2StringArray = (UCS2String*)gsimalloc(sizeof(UCS2String)*theNumStrings);
		int stringNum = 0;
		while(stringNum < theNumStrings)
		{
			aUCS2StringArray[stringNum] = UTF8ToUCS2StringAlloc(theUTF8StringArray[stringNum]);
			stringNum++;
		}

		return aUCS2StringArray;
	}
}


UTF8String* UCS2ToUTF8StringArrayAlloc(const UCS2String* theUCS2StringArray, int theNumStrings)
{
	if (theUCS2StringArray == NULL || theNumStrings == 0)
		return NULL;
	else
	{
		UTF8String* aUTF8StringArray = (UTF8String*)gsimalloc(sizeof(UTF8String)*theNumStrings);
		int stringNum = 0;
		while(stringNum < theNumStrings)
		{
			aUTF8StringArray[stringNum] = UCS2ToUTF8StringAlloc(theUCS2StringArray[stringNum]);
			stringNum++;
		}

		return aUTF8StringArray;
	}
}


int UCS2ToAsciiString(const UCS2String theUCS2String, char* theAsciiString)
{
	int length = 0;
	const UCS2String aReadPos = theUCS2String;
	char* aWritePos = theAsciiString;

	assert(theAsciiString != NULL);

	if (theUCS2String == NULL)
	{
		*theAsciiString = '\0';
		return 1;
	}

	while(*aReadPos != '\0')
	{
		(*aWritePos++) = (char)(0x00FF & (*aReadPos++));
		length++;
	}

	*aWritePos = '\0';
	length++;

	return length;
}


int AsciiToUCS2String(const char* theAsciiString, UCS2String theUCS2String)
{
	int length = 0;
	const char* aReadPos = theAsciiString;
	UCS2String aWritePos = theUCS2String;

	assert(theUCS2String != NULL);

	if (theAsciiString == NULL)
	{
		*theUCS2String = 0x0000;
		return 1;
	}

	while(*aReadPos != '\0')
	{
		(*aWritePos++) = (unsigned short)(0x00FF & (*aReadPos++)); 
		length++;
	}

	*aWritePos = '\0';
	length++;

	return length;
}


int UTF8ToUCS2StringLen(const UTF8String theUTF8String, UCS2String theUCS2String, int theMaxLength)
{
	int aNumCharsWritten	= 0;
	int aNumBytesRead		= 0;
	int aTotalBytesRead     = 0;
	const unsigned char* anInStream	= (const unsigned char*)theUTF8String;
	UCS2Char*            anOutStream= theUCS2String;

	if (theUTF8String == NULL)
	{
		*anOutStream = 0x0000;
		return 1;
	}

	while (*anInStream != '\0' && theMaxLength > aTotalBytesRead)
	{
		aNumBytesRead = _ReadUCS2CharFromUTF8String((UTF8String)anInStream, anOutStream, theMaxLength-aTotalBytesRead);
		if (aNumBytesRead == 0)
		{
			theUCS2String[0] = 0x0000;
			return 0;
		}
		aTotalBytesRead += aNumBytesRead;

		anInStream += aNumBytesRead;

		aNumCharsWritten++;

		anOutStream++;
	}

	*anOutStream = 0x0000;
	aNumCharsWritten++;
	
	return aNumCharsWritten;
}


#ifdef __cplusplus
} 
#endif	



