/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#include "gsPlatform.h"
#include "gsMemory.h"
#include "gsXML.h"
#include "gsStringUtil.h"
#include "../darray.h"

#define GS_XML_INITIAL_ELEMENT_ARRAY_COUNT	    32
#define GS_XML_INITIAL_ATTRIBUTE_ARRAY_COUNT    16

#define GS_XML_WHITESPACE    "\x20\x09\x0D\x0A"

#define GS_XML_CHECK(a)  { if (gsi_is_false(a)) return gsi_false; }

#define GS_XML_BASE64_ENCODING_TYPE             0

#define GS_XML_SOAP_BUFFER_INITIAL_SIZE	    (1 * 1024)
#define GS_XML_SOAP_BUFFER_INCREMENT_SIZE   (1 * 1024)
#define GS_XML_SOAP_INITIAL_NAMESPACE_COUNT 6

#define GS_XML_SOAP_HEADER     "<?xml version=\"1.0\" encoding=\"UTF-8\"?><SOAP-ENV:Envelope"
#define GS_XML_SOAP_BODY_TAG   "<SOAP-ENV:Body>"
#define GS_XML_SOAP_FOOTER     "</SOAP-ENV:Body></SOAP-ENV:Envelope>"
#define GS_XML_SOAP_NAMESPACE_PREFIX "xmlns:"

#define GS_XML_SOAP_DEFAULT_NAMESPACE_COUNT 4
const char * GS_XML_SOAP_DEFAULT_NAMESPACES[GS_XML_SOAP_DEFAULT_NAMESPACE_COUNT] =
{
	"SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\"",
	"SOAP-ENC=\"http://schemas.xmlsoap.org/soap/encoding/\"",
	"xsi=\"http://www.w3.org/2001/XMLSchema-instance\"",
	"xsd=\"http://www.w3.org/2001/XMLSchema\""
};


typedef struct GSIXmlString
{
	const gsi_u8 * mData;
	int mLen;
} GSIXmlString;

typedef struct GSIXmlAttribute
{
	GSIXmlString mName;
	GSIXmlString mValue;

	int mIndex;
	int mParentIndex;
} GSIXmlAttribute;

typedef struct GSIXmlElement
{
	GSIXmlString mName;
	GSIXmlString mValue; 

	int mIndex;
	int mParentIndex;
} GSIXmlElement;

typedef struct GSIXmlStreamReader
{
	DArray mElementArray;
	DArray mAttributeArray;

	int mElemReadIndex;   
	int mValueReadIndex;  
} GSIXmlStreamReader;

typedef struct GSIXmlStreamWriter
{
	char * mBuffer;
	int mLen;
	int mCapacity;
	gsi_bool mClosed; 
} GSIXmlStreamWriter;



static gsi_bool gsiXmlUtilSkipWhiteSpace(GSIXmlStreamReader * stream, const char * buffer, int len, int * pos);
static gsi_bool gsiXmlUtilParseName     (GSIXmlStreamReader * stream, const char * buffer, int len, int * pos, GSIXmlString * strOut);
static gsi_bool gsiXmlUtilParseString   (GSIXmlStreamReader * stream, char * buffer, int len, int * pos, GSIXmlString * strOut);
static gsi_bool gsiXmlUtilParseValue    (GSIXmlStreamReader * stream, char * buffer, int len, int * pos, GSIXmlString * strOut);
static gsi_bool gsiXmlUtilParseElement  (GSIXmlStreamReader * stream, char * buffer, int len, int * pos, int parentIndex);
static gsi_bool gsiXmlUtilTagMatches(const char * matchtag, GSIXmlString * xmlstr);

static gsi_bool gsiXmlUtilDecodeString(char * buffer, int * len);

static void gsiXmlUtilElementFree(void * elem);
static void gsiXmlUtilAttributeFree(void * elem);

static gsi_bool gsiXmlUtilWriteChar(GSIXmlStreamWriter * stream, char ch);
static gsi_bool gsiXmlUtilWriteString(GSIXmlStreamWriter * stream, const char * str);
static gsi_bool gsiXmlUtilWriteXmlSafeString(GSIXmlStreamWriter * stream, const char * str);
static gsi_bool gsiXmlUtilGrowBuffer(GSIXmlStreamWriter * stream);
#ifdef GSI_UNICODE
static gsi_bool gsiXmlUtilWriteAsciiString(GSIXmlStreamWriter * stream, const gsi_char * str);
#endif
static gsi_bool gsiXmlUtilWriteUnicodeString(GSIXmlStreamWriter * stream, const unsigned short * str);


static int gsUnicodeStringLen(const unsigned short * str)
{
	const unsigned short * end = str;
	while(*end++)
		{}
	return (end - str - 1);
}


GSXmlStreamWriter gsXmlCreateStreamWriter(const char ** namespaces, int count)
{
	GSIXmlStreamWriter * newStream = NULL;
	int initialCapacity = GS_XML_SOAP_BUFFER_INCREMENT_SIZE;
	int namespaceLen = 0;
	int i=0;

	GS_ASSERT((namespaces == NULL && count == 0) || (namespaces != NULL && count != 0));

	newStream = (GSIXmlStreamWriter*)gsimalloc(sizeof(GSIXmlStreamWriter));
	if (newStream == NULL)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Memory, GSIDebugLevel_HotError,
			"Out of memory in gsXmlCreateStreamWriter, needed %d bytes", sizeof(GSIXmlStreamWriter));
		return NULL; 
	}

	for (i=0; i < GS_XML_SOAP_DEFAULT_NAMESPACE_COUNT; i++)
	{
		GS_ASSERT(GS_XML_SOAP_DEFAULT_NAMESPACES[i] != NULL);
		namespaceLen += strlen(GS_XML_SOAP_NAMESPACE_PREFIX)+1; 
		namespaceLen += strlen(GS_XML_SOAP_DEFAULT_NAMESPACES[i]);
	}
	for (i=0; i < count; i++)
	{
		GS_ASSERT(namespaces[i] != NULL);
		namespaceLen += strlen(GS_XML_SOAP_NAMESPACE_PREFIX)+1; 
		namespaceLen += strlen(namespaces[i]);
	}
	while (initialCapacity < namespaceLen)
		initialCapacity += GS_XML_SOAP_BUFFER_INCREMENT_SIZE;

	newStream->mBuffer = (char*)gsimalloc((size_t)initialCapacity);
	if (newStream->mBuffer == NULL)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Memory, GSIDebugLevel_HotError,
			"Out of memory in gsXmlCreateStreamWriter, needed %d bytes", initialCapacity);
		return NULL; 
	}
	newStream->mCapacity = initialCapacity;
	newStream->mLen = 0;
	newStream->mBuffer[0] = '\0';
	newStream->mClosed = gsi_false;

	if (gsi_is_false(gsiXmlUtilWriteString(newStream, GS_XML_SOAP_HEADER)))
	{
		gsifree(newStream->mBuffer);
		gsifree(newStream);
		return NULL; 
	}
	for (i=0; i < GS_XML_SOAP_DEFAULT_NAMESPACE_COUNT; i++)
	{
		if (gsi_is_false(gsiXmlUtilWriteChar(newStream, ' ')) ||
			gsi_is_false(gsiXmlUtilWriteString(newStream, GS_XML_SOAP_NAMESPACE_PREFIX)) ||
			gsi_is_false(gsiXmlUtilWriteString(newStream, GS_XML_SOAP_DEFAULT_NAMESPACES[i])))
		{
			gsifree(newStream->mBuffer);
			gsifree(newStream);
			return NULL; 
		}
	}
	for (i=0; i < count; i++)
	{
		if (gsi_is_false(gsiXmlUtilWriteChar(newStream, ' ')) ||
			gsi_is_false(gsiXmlUtilWriteString(newStream, GS_XML_SOAP_NAMESPACE_PREFIX)) ||
			gsi_is_false(gsiXmlUtilWriteString(newStream, namespaces[i])) )
		{
			gsifree(newStream->mBuffer);
			gsifree(newStream);
			return NULL; 
		}
	}
	if (gsi_is_false(gsiXmlUtilWriteChar(newStream, '>')) ||
		gsi_is_false(gsiXmlUtilWriteString(newStream, GS_XML_SOAP_BODY_TAG)) )
	{
		gsifree(newStream->mBuffer);
		gsifree(newStream);
		return NULL; 
	}

	return (GSXmlStreamWriter)newStream;
}


GSXmlStreamReader gsXmlCreateStreamReader()
{
	GSIXmlStreamReader * newStream = NULL;

	newStream = (GSIXmlStreamReader*)gsimalloc(sizeof(GSIXmlStreamReader));
	if (newStream == NULL)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Memory, GSIDebugLevel_HotError,
			"Out of memory in gsXmlCreateStream, needed %d bytes", sizeof(GSIXmlStreamReader));
		return NULL; 
	}
	
	newStream->mElementArray = ArrayNew(sizeof(GSIXmlElement), GS_XML_INITIAL_ELEMENT_ARRAY_COUNT, gsiXmlUtilElementFree);
	if (newStream->mElementArray == NULL)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Memory, GSIDebugLevel_HotError,
			"Out of memory in gsXmlCreateStream with mElementArray=ArrayNew()");
		gsifree(newStream);
		return NULL; 
	}

	newStream->mAttributeArray = ArrayNew(sizeof(GSIXmlAttribute), GS_XML_INITIAL_ATTRIBUTE_ARRAY_COUNT, gsiXmlUtilAttributeFree);
	if (newStream->mAttributeArray == NULL)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Memory, GSIDebugLevel_HotError,
			"Out of memory in gsXmlCreateStream with mElementArray=ArrayNew()");
		ArrayFree(newStream->mElementArray);
		gsifree(newStream);
		return NULL; 
	}

	gsXmlMoveToStart(newStream);
	return (GSXmlStreamReader)newStream;
}


gsi_bool gsXmlParseBuffer(GSXmlStreamReader stream, char * data, int len)
{
	GSIXmlStreamReader * reader;
	int readPos = 0;

	GS_ASSERT(data != NULL);
	GS_ASSERT(len > 0);

	reader = (GSIXmlStreamReader*)stream;

	gsXmlResetReader(stream);

	while(readPos < len)
	{
		if (gsi_is_false(gsiXmlUtilParseElement(reader, data, len, &readPos, -1)))
			return gsi_false;
	}

	return gsi_true;
}


void gsXmlFreeWriter(GSXmlStreamWriter stream)
{
	GSIXmlStreamWriter * writer = (GSIXmlStreamWriter*)stream;
	gsifree(writer->mBuffer);
	gsifree(writer);
}


void gsXmlFreeReader(GSXmlStreamReader stream)
{
	GSIXmlStreamReader * reader = (GSIXmlStreamReader*)stream;
	ArrayFree(reader->mAttributeArray);
	ArrayFree(reader->mElementArray);
	gsifree(reader);
}


void gsXmlResetReader(GSXmlStreamReader stream)
{
	GSIXmlStreamReader * reader = (GSIXmlStreamReader*)stream;
	ArrayClear(reader->mAttributeArray);
	ArrayClear(reader->mElementArray);
	gsXmlMoveToStart(stream);
}


gsi_bool gsXmlCloseWriter(GSXmlStreamWriter stream)
{
	GSIXmlStreamWriter* writer = (GSIXmlStreamWriter*)stream;
	GS_ASSERT(stream != NULL);
	GS_ASSERT(gsi_is_false(writer->mClosed));

	if (gsi_is_false(gsiXmlUtilWriteString(writer, GS_XML_SOAP_FOOTER)))
		return gsi_false;
	
	writer->mClosed = gsi_true;
	return gsi_true;
}


static void gsiXmlUtilElementFree(void * elem)
{
	GSI_UNUSED(elem);
}


static void gsiXmlUtilAttributeFree(void * elem)
{
	GSI_UNUSED(elem);
}

const char * gsXmlWriterGetData (GSXmlStreamWriter stream)
{
	GSIXmlStreamWriter * writer = (GSIXmlStreamWriter*)stream;
	GS_ASSERT(stream != NULL)
	return writer->mBuffer;
}


int gsXmlWriterGetDataLength(GSXmlStreamWriter stream)
{
	GSIXmlStreamWriter * writer = (GSIXmlStreamWriter*)stream;
	GS_ASSERT(stream != NULL);
	return writer->mLen;
}


static gsi_bool gsiXmlUtilSkipWhiteSpace(GSIXmlStreamReader * stream, const char * buffer, int len, int * pos)
{
	GS_ASSERT(buffer != NULL);
	GS_ASSERT(len > 0);

	while(*pos < len)
	{
		if (NULL == strchr(GS_XML_WHITESPACE, buffer[*pos]))
			return gsi_true;
		(*pos)++; 
	}

	GSI_UNUSED(stream);
	return gsi_false;
}


static gsi_bool gsiXmlUtilParseElement(GSIXmlStreamReader * stream, char * buffer, 
									  int len, int * pos, int parentIndex)
{
	GSIXmlElement newElem;
	int startPos = 0;
	gsi_bool storeElement = gsi_true;

	GS_ASSERT(stream != NULL);
	GS_ASSERT(buffer != NULL);
	GS_ASSERT(len > 0);
	GS_ASSERT(*pos < len);

	gsiXmlUtilSkipWhiteSpace(stream, buffer, len, pos);
	if (*pos >= len)
		return gsi_true; 

	memset(&newElem, 0, sizeof(newElem));

	if (buffer[*pos] != '<')
		return gsi_false;
	(*pos)++;
	if (*pos >= len)
		return gsi_false;
	GS_XML_CHECK(gsiXmlUtilSkipWhiteSpace(stream, buffer, len, pos));

	if (buffer[*pos] == '!' || buffer[*pos] == '?' ||
		buffer[*pos] == '%' || buffer[*pos] == '/')
	{
		storeElement = gsi_false;
		(*pos)++;
		startPos = (*pos)-2;
	}
	else
		startPos = (*pos)-1; 

	GS_XML_CHECK(gsiXmlUtilParseName(stream, buffer, len, pos, &newElem.mName));
	GS_XML_CHECK(gsiXmlUtilSkipWhiteSpace(stream, buffer, len, pos));

	if (storeElement)
	{
		GS_ASSERT(newElem.mName.mData != NULL);
		newElem.mIndex = ArrayLength(stream->mElementArray);
		newElem.mParentIndex = parentIndex;
		ArrayAppend(stream->mElementArray, &newElem);
	}

	while (*pos < len && isalnum(buffer[*pos]))
	{
		GSIXmlAttribute newAttr;
		memset(&newAttr, 0, sizeof(newAttr));
		GS_XML_CHECK(gsiXmlUtilParseName(stream, buffer, len, pos, &newAttr.mName));
		GS_XML_CHECK(gsiXmlUtilSkipWhiteSpace(stream, buffer, len, pos));
		if (buffer[*pos] != '=')
			return gsi_false;
		(*pos)++; 
		GS_XML_CHECK(gsiXmlUtilParseString(stream, buffer, len, pos, &newAttr.mValue));
		GS_XML_CHECK(gsiXmlUtilSkipWhiteSpace(stream, buffer, len, pos));

		if (storeElement) 
		{
			GS_ASSERT(newElem.mName.mData != NULL);
			GS_ASSERT(newElem.mIndex != -1);
			GS_ASSERT(newAttr.mName.mData != NULL);
			GS_ASSERT(newAttr.mValue.mData != NULL);

			newAttr.mIndex = ArrayLength(stream->mAttributeArray);
			newAttr.mParentIndex = newElem.mIndex;
			ArrayAppend(stream->mAttributeArray, &newAttr);
		}
	}

	GS_XML_CHECK(gsiXmlUtilSkipWhiteSpace(stream, buffer, len, pos));

	if ( (!isalnum(buffer[startPos+1]) && buffer[*pos] == buffer[startPos+1]) 
			|| buffer[*pos] == '/')
	{
		(*pos)++;
		GS_XML_CHECK(gsiXmlUtilSkipWhiteSpace(stream, buffer, len, pos));
		if (buffer[*pos] != '>')
			return gsi_false; 
		(*pos)++;
		return gsi_true; 
	}

	if (buffer[*pos] != '>')
		return gsi_false;
	(*pos)++;
	GS_XML_CHECK(gsiXmlUtilSkipWhiteSpace(stream, buffer, len, pos));

	if (buffer[*pos] != '<')
	{
		GS_XML_CHECK(gsiXmlUtilParseValue(stream, buffer, len, pos, &newElem.mValue));
		if (storeElement)
			ArrayReplaceAt(stream->mElementArray, &newElem, newElem.mIndex);
		GS_XML_CHECK(gsiXmlUtilSkipWhiteSpace(stream, buffer, len, pos));
	}

	while (*pos < len)
	{
		int childStartPos = *pos;
		if (buffer[*pos] != '<')
			return gsi_false;
		(*pos)++;
		GS_XML_CHECK(gsiXmlUtilSkipWhiteSpace(stream, buffer, len, pos));
		if (buffer[*pos] == '/')
		{
			(*pos)++;
			GS_XML_CHECK(gsiXmlUtilSkipWhiteSpace(stream, buffer, len, pos));
			if ((*pos)+newElem.mName.mLen >= len)
				return gsi_false; 
			if (0 != strncmp((const char*)newElem.mName.mData, &buffer[*pos], (size_t)newElem.mName.mLen))
				return gsi_false; 
			(*pos) += newElem.mName.mLen;
			GS_XML_CHECK(gsiXmlUtilSkipWhiteSpace(stream, buffer, len, pos));
			if (buffer[*pos] != '>')
				return gsi_false;
			(*pos)++;
			return gsi_true;
		}
		else
		{
			if (newElem.mValue.mData != NULL)
				return gsi_false; 

			*pos = childStartPos;
			GS_XML_CHECK(gsiXmlUtilParseElement(stream, buffer, len, pos, newElem.mIndex));
			gsiXmlUtilSkipWhiteSpace(stream, buffer, len, pos);
		}
	}
	return gsi_false; 
}


static gsi_bool gsiXmlUtilParseName(GSIXmlStreamReader * stream, const char * buffer, 
								   int len, int * pos, GSIXmlString * strOut)
{
	gsi_bool haveNamespace = gsi_false;
	GS_ASSERT(buffer != NULL);
	GS_ASSERT(len > 0);

	GS_XML_CHECK(gsiXmlUtilSkipWhiteSpace(stream, buffer, len, pos));

	if (*pos >= len)
		return gsi_false;

	if (!isalnum(buffer[*pos]) || isdigit(buffer[*pos]))
		return gsi_false;

	strOut->mData = (gsi_u8*)&buffer[*pos];
	strOut->mLen = 1;
	(*pos)++;

	while(*pos < len && NULL==strchr(GS_XML_WHITESPACE, buffer[*pos]))
	{
		if (buffer[*pos] == ':')
		{
			if (gsi_is_true(haveNamespace))
				return gsi_false; 
			haveNamespace = gsi_true;
		}
		else if ((buffer[*pos] != '_') && (buffer[*pos] != '-') && (!isalnum(buffer[*pos])))
			return gsi_true; 

		strOut->mLen++;
		(*pos)++;
	}

	return gsi_true;	
}


static gsi_bool gsiXmlUtilParseString(GSIXmlStreamReader * stream, char * buffer,
									 int len, int * pos, GSIXmlString * strOut)
{
	char startCh = '\0';
	char * strStart = NULL;


	GS_ASSERT(stream != NULL);
	GS_XML_CHECK(gsiXmlUtilSkipWhiteSpace(stream, buffer, len, pos));

	startCh = buffer[*pos];
	if (startCh != '\"' && startCh != '\'')
		return gsi_false;

	(*pos)++;
	strStart = &buffer[*pos];  
	strOut->mLen = 0;

	if (*pos >= len)
		return gsi_false; 
	if (buffer[*pos] == startCh)
	{
		strOut->mData = (const gsi_u8*)strStart;
		(*pos)++; 
		return gsi_true; 
	}

	while(buffer[*pos] != startCh)
	{
		if (*pos >= len)
			return gsi_false; 


		(*pos)++;
		strOut->mLen++;
	} 
	(*pos)++; 

	if (gsi_is_false(gsiXmlUtilDecodeString(strStart, &strOut->mLen)))
		return gsi_false;

	strOut->mData = (const gsi_u8*)strStart;
	return gsi_true;

}


static gsi_bool gsiXmlUtilDecodeString(char * buffer, int * len)
{
	int readPos = 0;

	while(readPos < *len)
	{
		int charsToRemove = 0;

		if (buffer[readPos] != '&')
		{
			readPos++;
			continue;
		}

		if (strncmp(&buffer[readPos], "&amp;", 5)==0)
		{
			buffer[readPos++] = '&';
			charsToRemove = 5-1;
		}
		else if (strncmp(&buffer[readPos], "&quot;", 6)==0)
		{
			buffer[readPos++] = '\"';
			charsToRemove = 6-1;
		}
		else if (strncmp(&buffer[readPos], "&apos;", 6)==0)
		{
			buffer[readPos++] = '\'';
			charsToRemove = 6-1;
		}
		else if (strncmp(&buffer[readPos], "&lt;", 4)==0)
		{
			buffer[readPos++] = '<';
			charsToRemove = 4-1;
		}
		else if (strncmp(&buffer[readPos], "&gt;", 4)==0)
		{
			buffer[readPos++] = '>';
			charsToRemove = 4-1;
		}
		else if (strncmp(&buffer[readPos], "&#x", 3)==0)
		{
			unsigned int digitValue = 0;
			char ch = ' ';
			gsi_bool haveWritten = gsi_false;
			int i=0;
			unsigned int mask = 0xFF000000;

			char * digitEnd = strchr(&buffer[readPos+3], ';');
			if (digitEnd == NULL)
				return gsi_false; 
			if (digitEnd - &buffer[readPos+3] > 8)
				return gsi_false; 

			sscanf(&buffer[readPos+3], "%08x", &digitValue);

			for (i=0; i < 4; i++)
			{
				ch = (char)((digitValue & mask) >> ((3-i)*8));  
				if (haveWritten || ch != 0x00)
				{
					buffer[readPos++] = ch;
					haveWritten = gsi_true;
				}
				mask = mask >> 8;
			}

			charsToRemove = digitEnd - &buffer[readPos] + 1; 
		}
		else if (strncmp(&buffer[readPos], "&#", 2)==0)
		{
			unsigned int digitValue = 0;
			char ch = ' ';
			gsi_bool haveWritten = gsi_false;
			int i=0;
			unsigned int mask = 0xFF000000;

			char * digitEnd = strchr(&buffer[readPos+2], ';');
			if (digitEnd == NULL)
				return gsi_false; 

			digitValue = (unsigned int)atoi(&buffer[readPos+2]);

			for (i=0; i < 4; i++)
			{
				ch = (char)((digitValue & mask) >> ((3-i)*8));  
				if (haveWritten || ch != 0x00)
				{
					buffer[readPos++] = ch;
					haveWritten = gsi_true;
				}
				mask = mask >> 8;
			}

			charsToRemove = digitEnd - &buffer[readPos] + 1; 

		}
		else
			return gsi_false; 

		memmove(&buffer[readPos], &buffer[readPos+charsToRemove], (size_t)(*len-(readPos+charsToRemove)));
		memset(&buffer[*len-charsToRemove], ' ', (size_t)charsToRemove);
		(*len) -= charsToRemove; 
	}

	return gsi_true;
}


static gsi_bool gsiXmlUtilParseValue(GSIXmlStreamReader * stream, char * buffer,
									 int len, int * pos, GSIXmlString * strOut)
{
	char * strStart = NULL;

	GS_ASSERT(stream != NULL);
	GS_XML_CHECK(gsiXmlUtilSkipWhiteSpace(stream, buffer, len, pos));

	if (buffer[*pos] != '<')
	{
		strStart = &buffer[*pos]; 
		strOut->mData = (const gsi_u8*)strStart;
	}

	while(*pos < len)
	{
		if (buffer[*pos] == '<')
		{
			if (gsi_is_false(gsiXmlUtilDecodeString(strStart, &strOut->mLen)))
				return gsi_false;
			return gsi_true; 
		}
		(*pos)++;
		strOut->mLen++;
	} 
	return gsi_false; 
}


gsi_bool gsXmlWriteOpenTag(GSXmlStreamWriter stream, const char * namespaceName, 
						   const char * tag)
{
	GSIXmlStreamWriter * writer = (GSIXmlStreamWriter*)stream;

	GS_ASSERT(stream != NULL);
	GS_ASSERT(namespaceName != NULL);
	GS_ASSERT(tag != NULL);
	GS_ASSERT(gsi_is_false(writer->mClosed));

	if ( gsi_is_false(gsiXmlUtilWriteChar(writer, '<')) ||
		 gsi_is_false(gsiXmlUtilWriteString(writer, namespaceName)) ||
		 gsi_is_false(gsiXmlUtilWriteChar(writer, ':')) ||
		 gsi_is_false(gsiXmlUtilWriteString(writer, tag)) ||
		 gsi_is_false(gsiXmlUtilWriteChar(writer, '>')) 
		 )
	{
		return gsi_false;
	}
	return gsi_true;
}


gsi_bool gsXmlWriteCloseTag(GSXmlStreamWriter stream,	const char * namespaceName,	
							const char * tag)
{
	GSIXmlStreamWriter * writer = (GSIXmlStreamWriter*)stream;

	GS_ASSERT(stream != NULL);
	GS_ASSERT(namespaceName != NULL);
	GS_ASSERT(tag != NULL);
	GS_ASSERT(gsi_is_false(writer->mClosed));

	if ( gsi_is_false(gsiXmlUtilWriteChar(writer, '<')) ||
		 gsi_is_false(gsiXmlUtilWriteChar(writer, '/')) ||
 		 gsi_is_false(gsiXmlUtilWriteString(writer, namespaceName)) ||
		 gsi_is_false(gsiXmlUtilWriteChar(writer, ':')) ||
		 gsi_is_false(gsiXmlUtilWriteString(writer, tag)) ||
		 gsi_is_false(gsiXmlUtilWriteChar(writer, '>'))
		 )
	{
		return gsi_false;
	}
	return gsi_true;
}


gsi_bool gsXmlWriteStringElement(GSXmlStreamWriter stream, const char * namespaceName, 
								 const char * tag, const char * value)
{
	GSIXmlStreamWriter * writer = (GSIXmlStreamWriter*)stream;
	int i=0;
	int len = 0;

	GS_ASSERT(stream != NULL);
	GS_ASSERT(namespaceName != NULL);
	GS_ASSERT(tag != NULL);
	GS_ASSERT(value != NULL);
	GS_ASSERT(gsi_is_false(writer->mClosed));

	len = (int)strlen(value);
	for (i=0; i < len; i++)
	{
		if ((unsigned char)value[i] < 0x20)
		{
			if ((unsigned char)value[i] != 0x09
				&& ((unsigned char)value[i] != 0x0A)
				&& ((unsigned char)value[i] != 0x0D)
				)
			{
				return gsi_false;
			}
		}
	}

	if ( gsi_is_false(gsXmlWriteOpenTag(stream, namespaceName, tag)) ||
		 gsi_is_false(gsiXmlUtilWriteXmlSafeString(writer, value)) ||
		 gsi_is_false(gsXmlWriteCloseTag(stream, namespaceName, tag))
		 )
	{
		return gsi_false;
	}
	return gsi_true;
}


gsi_bool gsXmlWriteAsciiStringElement(GSXmlStreamWriter stream, const char * namespaceName, 
								 const char * tag, const gsi_char * value)
{
	GSIXmlStreamWriter * writer = (GSIXmlStreamWriter*)stream;
	int i=0;
	int len = 0;

	GS_ASSERT(stream != NULL);
	GS_ASSERT(namespaceName != NULL);
	GS_ASSERT(tag != NULL);
	GS_ASSERT(value != NULL);
	GS_ASSERT(gsi_is_false(writer->mClosed));

	len = (int)_tcslen(value);
	for (i=0; i < len; i++)
	{
		if ((unsigned char)value[i] < 0x20)
		{
			if ((unsigned char)value[i] != 0x09
				&& ((unsigned char)value[i] != 0x0A)
				&& ((unsigned char)value[i] != 0x0D)
				)
			{
				return gsi_false;
			}
		}
	}

#ifdef GSI_UNICODE
	if ( gsi_is_false(gsXmlWriteOpenTag(stream, namespaceName, tag)) ||
		 gsi_is_false(gsiXmlUtilWriteAsciiString(writer, value)) ||
		 gsi_is_false(gsXmlWriteCloseTag(stream, namespaceName, tag))
		 )
	{
		return gsi_false;
	}
#else
	if ( gsi_is_false(gsXmlWriteOpenTag(stream, namespaceName, tag)) ||
		 gsi_is_false(gsiXmlUtilWriteXmlSafeString(writer, value)) ||
		 gsi_is_false(gsXmlWriteCloseTag(stream, namespaceName, tag))
		 )
	{
		return gsi_false;
	}
#endif
	return gsi_true;
}


gsi_bool gsXmlWriteUnicodeStringElement(GSXmlStreamWriter stream, const char * namespaceName, 
										const char * tag, const unsigned short * value)
{
	GSIXmlStreamWriter * writer = (GSIXmlStreamWriter*)stream;
	int i = 0;
	int len = 0;

	GS_ASSERT(stream != NULL);
	GS_ASSERT(namespaceName != NULL);
	GS_ASSERT(tag != NULL);
	GS_ASSERT(value != NULL);
	GS_ASSERT(gsi_is_false(writer->mClosed));

	len = gsUnicodeStringLen(value);
	for (i=0; i < len; i++)
	{
		if (value[i] < 0x0020)
		{
			if ((value[i] != 0x0009) && (value[i] != 0x0A) && (value[i] != 0x0D))
				return gsi_false; 
		}
		else if (value[i] > 0xD7FF && value[i] < 0xE000)
			return gsi_false; 
		else if (value[i] > 0xFFFD)
			return gsi_false; 
	}

	if ( gsi_is_false(gsXmlWriteOpenTag(stream, namespaceName, tag)) ||
		 gsi_is_false(gsiXmlUtilWriteUnicodeString(writer, value)) ||
		 gsi_is_false(gsXmlWriteCloseTag(stream, namespaceName, tag))
		 )
	{
		return gsi_false;
	}
	return gsi_true;
}


gsi_bool gsXmlWriteIntElement(GSXmlStreamWriter stream, const char * namespaceName, 
							  const char * tag,	gsi_u32 value)
{
	GSIXmlStreamWriter * writer = (GSIXmlStreamWriter*)stream;
	char buf[32];

	GS_ASSERT(stream != NULL);
	GS_ASSERT(namespaceName != NULL);
	GS_ASSERT(tag != NULL);
	GS_ASSERT(gsi_is_false(writer->mClosed));

	sprintf(buf, "%d", value);

	if ( gsi_is_false(gsXmlWriteOpenTag(stream, namespaceName, tag)) ||
		 gsi_is_false(gsiXmlUtilWriteString(writer, buf)) ||
		 gsi_is_false(gsXmlWriteCloseTag(stream, namespaceName, tag))
		 )
	{
		return gsi_false;
	}
	return gsi_true;
}


gsi_bool gsXmlWriteInt64Element(GSXmlStreamWriter stream, const char * namespaceName, 
								const char * tag,	gsi_i64 value)
{
	GSIXmlStreamWriter * writer = (GSIXmlStreamWriter*)stream;
	char buf[33];

	GS_ASSERT(stream != NULL);
	GS_ASSERT(namespaceName != NULL);
	GS_ASSERT(tag != NULL);
	GS_ASSERT(gsi_is_false(writer->mClosed));

	gsiInt64ToString(buf, value);

	if ( gsi_is_false(gsXmlWriteOpenTag(stream, namespaceName, tag)) ||
		gsi_is_false(gsiXmlUtilWriteString(writer, buf)) ||
		gsi_is_false(gsXmlWriteCloseTag(stream, namespaceName, tag))
		)
	{
		return gsi_false;
	}
	return gsi_true;
}


gsi_bool gsXmlWriteFloatElement(GSXmlStreamWriter stream, const char * namespaceName, 
								const char * tag, float value)
{
	GSIXmlStreamWriter * writer = (GSIXmlStreamWriter*)stream;
	char buf[32];

	GS_ASSERT(stream != NULL);
	GS_ASSERT(namespaceName != NULL);
	GS_ASSERT(tag != NULL);
	GS_ASSERT(gsi_is_false(writer->mClosed));

	sprintf(buf, "%f", value);

	if ( gsi_is_false(gsXmlWriteOpenTag(stream, namespaceName, tag)) ||
		 gsi_is_false(gsiXmlUtilWriteString(writer, buf)) ||
		 gsi_is_false(gsXmlWriteCloseTag(stream, namespaceName, tag))
		 )
	{
		return gsi_false;
	}
	return gsi_true;
}


gsi_bool gsXmlWriteHexBinaryElement(GSXmlStreamWriter stream, const char * namespaceName, const char * tag, const gsi_u8 * data, int len)
{
	GSIXmlStreamWriter * writer = (GSIXmlStreamWriter*)stream;
	int pos = 0;
	int temp = 0;
	
	char hex[3];
	hex[2] = '\0';

	GS_ASSERT(stream != NULL);
	GS_ASSERT(namespaceName != NULL);
	GS_ASSERT(tag != NULL);
	GS_ASSERT(data != NULL);
	GS_ASSERT(len > 0);
	GS_ASSERT(gsi_is_false(writer->mClosed));

	if (gsi_is_false(gsXmlWriteOpenTag(stream, namespaceName, tag)))
		return gsi_false;

	while (pos < len)
	{
		temp = data[pos]; 
		sprintf(hex, "%02x", temp);
		pos++;
		if (gsi_is_false(gsiXmlUtilWriteChar(writer, hex[0])))
			return gsi_false;
		if (gsi_is_false(gsiXmlUtilWriteChar(writer, hex[1])))
			return gsi_false;
	}
	
	if (gsi_is_false(gsXmlWriteCloseTag(stream, namespaceName, tag)))
		return gsi_false;
	
	return gsi_true;
}


gsi_bool gsXmlWriteBase64BinaryElement(GSXmlStreamWriter stream, const char * namespaceName, const char * tag, const gsi_u8 * data, int len)
{
	GSIXmlStreamWriter * writer = (GSIXmlStreamWriter*)stream;
	B64StreamData streamData;
	char b64[5];

	GS_ASSERT(stream != NULL);
	GS_ASSERT(namespaceName != NULL);
	GS_ASSERT(tag != NULL);
	GS_ASSERT(data != NULL);
	GS_ASSERT(gsi_is_false(writer->mClosed));

	if (gsi_is_false(gsXmlWriteOpenTag(stream, namespaceName, tag)))
		return gsi_false;

	B64InitEncodeStream(&streamData, (const char*)data, len, GS_XML_BASE64_ENCODING_TYPE);
	while(B64EncodeStream(&streamData, b64))
	{
		b64[4] = '\0';
		if (gsi_is_false(gsiXmlUtilWriteString(writer, b64)))
			return gsi_false;
	}
	
	if (gsi_is_false(gsXmlWriteCloseTag(stream, namespaceName, tag)))
		return gsi_false;
	
	return gsi_true;
}


gsi_bool gsXmlWriteDateTimeElement(GSXmlStreamWriter stream, const char * namespaceName, const char * tag, time_t value)
{
	GSIXmlStreamWriter * writer = (GSIXmlStreamWriter*)stream;
	char timeString[21];
	struct tm *timePtr;

	timePtr = gsiSecondsToDate(&value);

	sprintf(timeString, "%d-%02d-%02dT%02d:%02d:%02dZ",
		timePtr->tm_year + 1900, timePtr->tm_mon + 1, timePtr->tm_mday,
		timePtr->tm_hour, timePtr->tm_min, timePtr->tm_sec);

	GS_ASSERT(stream != NULL);
	GS_ASSERT(namespaceName != NULL);
	GS_ASSERT(tag != NULL);
	GS_ASSERT(gsi_is_false(writer->mClosed));

	if ( gsi_is_false(gsXmlWriteOpenTag(stream, namespaceName, tag)) ||
		 gsi_is_false(gsiXmlUtilWriteString(writer, timeString)) ||
		 gsi_is_false(gsXmlWriteCloseTag(stream, namespaceName, tag))
		 )
	{
		return gsi_false;
	}
	return gsi_true;
}


gsi_bool gsXmlWriteLargeIntElement(GSXmlStreamWriter stream, const char * namespaceName, const char * tag, const struct gsLargeInt_s * lint)
{
	GSIXmlStreamWriter * writer = (GSIXmlStreamWriter*)stream;
	int readPos = (int)(lint->mLength - 1);
	const l_word *readBuf = (const l_word*)lint->mData;
	unsigned int temp;
	int i;
	char hex[3];
	gsi_bool first = gsi_true;

	if (gsi_is_false(gsXmlWriteOpenTag(stream, namespaceName, tag)))
		return gsi_false;

	while(readPos >= 0 && readBuf[readPos] == 0)
		readPos--;

	for (; readPos >= 0; readPos--)
	{
		if(gsi_is_true(first))
		{
			for(i = 0 ; i < GS_LARGEINT_DIGIT_SIZE_BYTES ; i++)
			{
				temp = ((lint->mData[readPos] >> (8 * (GS_LARGEINT_DIGIT_SIZE_BYTES - i - 1))) & 0xFF);
				if(temp != 0)
					break;
			}
			first = gsi_false;
		}
		else
		{
			i = 0;
		}

		for (; i < GS_LARGEINT_DIGIT_SIZE_BYTES; i++)
		{
			temp = ((lint->mData[readPos] >> (8 * (GS_LARGEINT_DIGIT_SIZE_BYTES - i - 1))) & 0xFF);
			sprintf(hex, "%02x", temp);
			if (gsi_is_false(gsiXmlUtilWriteChar(writer, hex[0])))
				return gsi_false;
			if (gsi_is_false(gsiXmlUtilWriteChar(writer, hex[1])))
				return gsi_false;
		}
	}

	if (gsi_is_false(gsXmlWriteCloseTag(stream, namespaceName, tag)))
		return gsi_false;

	return gsi_true;
}


static gsi_bool gsiXmlUtilWriteChar(GSIXmlStreamWriter * stream, char ch)
{
	GS_ASSERT(gsi_is_false(stream->mClosed));

	if (stream->mLen >= stream->mCapacity)
	{
		if (gsi_is_false(gsiXmlUtilGrowBuffer(stream)))
			return gsi_false; 
	}
	stream->mBuffer[stream->mLen] = ch;
	stream->mLen++;

	return gsi_true;
}


static gsi_bool gsiXmlUtilWriteString(GSIXmlStreamWriter * stream, const char * str)
{
	int strLen = 0;

	GS_ASSERT(str != NULL);
	GS_ASSERT(gsi_is_false(stream->mClosed));

	strLen = (int)strlen(str);
	if (strLen == 0)
		return gsi_true;

	while ((stream->mCapacity - stream->mLen) <= strLen)
	{
		if (gsi_is_false(gsiXmlUtilGrowBuffer(stream)))
			return gsi_false; 
	}

	strcpy(&stream->mBuffer[stream->mLen], str);
	stream->mLen += strLen;
	return gsi_true;
}


#ifdef GSI_UNICODE
static gsi_bool gsiXmlUtilWriteAsciiString(GSIXmlStreamWriter * stream, const gsi_char * str)
{
	int strLen = 0;

	GS_ASSERT(str != NULL);
	GS_ASSERT(gsi_is_false(stream->mClosed));

	strLen = (int)_tcslen(str);
	if (strLen == 0)
		return gsi_true;

	while ((stream->mCapacity - stream->mLen) <= strLen)
	{
		if (gsi_is_false(gsiXmlUtilGrowBuffer(stream)))
			return gsi_false; 
	}

	UCS2ToAsciiString(str, &stream->mBuffer[stream->mLen]);
	stream->mLen += strLen;
	return gsi_true;
}
#endif

static gsi_bool gsiXmlUtilWriteUnicodeString(GSIXmlStreamWriter * stream, const unsigned short * str)
{
	int strLen = 0;
	int pos = 0;
	int utf8Len = 0;
	char utf8String[4] = { '\0' };

	GS_ASSERT(str != NULL);
	GS_ASSERT(gsi_is_false(stream->mClosed));

	strLen = gsUnicodeStringLen(str);
	utf8String[3] = '\0';
	
	for (pos = 0; pos < strLen; pos++)
	{
		utf8Len = _UCS2CharToUTF8String(str[pos], utf8String);
		utf8String[utf8Len] = '\0'; 
		if (gsi_is_false(gsiXmlUtilWriteXmlSafeString(stream, utf8String)))
			return gsi_false;	
	}
	return gsi_true;
}


static gsi_bool gsiXmlUtilWriteXmlSafeString(GSIXmlStreamWriter * stream, const char * str)
{
	int strLen = 0;
	int pos = 0;
	gsi_bool result = gsi_false;

	GS_ASSERT(str != NULL);
	GS_ASSERT(gsi_is_false(stream->mClosed));

	strLen = (int)strlen(str);

	for (pos = 0; pos < strLen; pos++)
	{
		if (str[pos] == '&')
			result = gsiXmlUtilWriteString(stream, "&amp;");
		else if (str[pos] == '\'')
			result = gsiXmlUtilWriteString(stream, "&apos;");
		else if (str[pos] == '"')
			result = gsiXmlUtilWriteString(stream, "&quot;");
		else if (str[pos] == '<')
			result = gsiXmlUtilWriteString(stream, "&lt;");
		else if (str[pos] == '>')
			result = gsiXmlUtilWriteString(stream, "&gt;");
		else if (str[pos] == ' ')
			result = gsiXmlUtilWriteString(stream, "&#x20;");
		else if (str[pos] < 0x20 || ((unsigned char)str[pos]) > 0x7F)
		{
			char numeric[7];
			sprintf(numeric, "&#x%02x;", (unsigned char)str[pos]);
			numeric[6] = '\0';
			result = gsiXmlUtilWriteString(stream, numeric);
		}
		else
			result = gsiXmlUtilWriteChar(stream, str[pos]);

		if (gsi_is_false(result))
			return gsi_false;
	}
	return gsi_true;
}


static gsi_bool gsiXmlUtilGrowBuffer(GSIXmlStreamWriter * stream)
{
	int newCapacity = stream->mCapacity + GS_XML_SOAP_BUFFER_INCREMENT_SIZE;
	void* newBuf = NULL;

	newBuf = gsirealloc(stream->mBuffer, (size_t)newCapacity);
	if (newBuf == NULL)
		return gsi_false;
	if (newBuf != stream->mBuffer)
		stream->mBuffer = (char*)newBuf;
	stream->mCapacity = newCapacity;
	return gsi_true;
}


gsi_bool gsXmlMoveToStart(GSXmlStreamReader stream)
{
	GSIXmlStreamReader * reader = (GSIXmlStreamReader*)stream;
	reader->mElemReadIndex = -1; 
	reader->mValueReadIndex = -1;
	return gsi_true;
}


gsi_bool gsXmlMoveToNext(GSXmlStreamReader stream, const char * matchtag)
{
	GSIXmlStreamReader * reader = (GSIXmlStreamReader*)stream;
	int i=0;

	for (i=(reader->mElemReadIndex+1); i < ArrayLength(reader->mElementArray); i++)
	{
		GSIXmlElement * elem = (GSIXmlElement*)ArrayNth(reader->mElementArray, i);
		if (gsi_is_true(gsiXmlUtilTagMatches(matchtag, &elem->mName)))
		{
			reader->mElemReadIndex = i;
			reader->mValueReadIndex = -1;
			return gsi_true;
		}
	}
	return gsi_false;
}


gsi_bool gsXmlMoveToParent(GSXmlStreamReader stream)
{
	GSIXmlStreamReader * reader = (GSIXmlStreamReader*)stream;

	if (reader->mElemReadIndex >= ArrayLength(reader->mElementArray) ||
		reader->mElemReadIndex == -1)
	{
		return gsi_false; 
	}
	else
	{
		GSIXmlElement * elem = (GSIXmlElement*)ArrayNth(reader->mElementArray, reader->mElemReadIndex);
		if (elem->mParentIndex == -1)
			return gsi_false; 
		if (elem->mParentIndex >= ArrayLength(reader->mElementArray))
			return gsi_false; 
		reader->mElemReadIndex = elem->mParentIndex;
		reader->mValueReadIndex = -1;
		return gsi_true;
	}
}


gsi_bool gsXmlMoveToSibling (GSXmlStreamReader stream, const char * matchtag)
{
	GSIXmlStreamReader * reader = (GSIXmlStreamReader*)stream;
	int i=0;

	int curElemParent = -1;
	GSIXmlElement * searchElem = NULL;
	
	if (reader->mElemReadIndex < ArrayLength(reader->mElementArray))
	{
		GSIXmlElement * curElem = (GSIXmlElement*)ArrayNth(reader->mElementArray, reader->mElemReadIndex);
		curElemParent = curElem->mParentIndex;
	}
	else
		curElemParent = -1;

	for (i=(reader->mElemReadIndex+1); i < ArrayLength(reader->mElementArray); i++)
	{
		searchElem = (GSIXmlElement*)ArrayNth(reader->mElementArray, i);
		if (searchElem->mParentIndex == curElemParent)
		{
			if (gsi_is_true(gsiXmlUtilTagMatches(matchtag, &searchElem->mName)))
			{
				reader->mElemReadIndex = i;
				reader->mValueReadIndex = -1;
				return gsi_true;
			}
		}
		if (searchElem->mParentIndex < reader->mElemReadIndex)
			return gsi_false;
	}

	return gsi_false;
}

gsi_bool gsXmlMoveToChild(GSXmlStreamReader stream, const char * matchtag)
{
	GSIXmlStreamReader * reader = (GSIXmlStreamReader*)stream;
	GSIXmlElement * searchElem = NULL;
	int i=0;

	for (i=(reader->mElemReadIndex+1); i < ArrayLength(reader->mElementArray); i++)
	{
		searchElem = (GSIXmlElement*)ArrayNth(reader->mElementArray, i);
		if (searchElem->mParentIndex == reader->mElemReadIndex)
		{
			if (gsi_is_true(gsiXmlUtilTagMatches(matchtag, &searchElem->mName)))
			{
				reader->mElemReadIndex = i;
				reader->mValueReadIndex = -1;
				return gsi_true;
			}
		}
		if (searchElem->mParentIndex < reader->mElemReadIndex)
			return gsi_false;
	}
	return gsi_false;
}

gsi_bool gsXmlReadChildAsString(GSXmlStreamReader stream, const char * matchtag, 
								 const char ** valueOut, int * lenOut)
{
	GSIXmlStreamReader * reader = (GSIXmlStreamReader*)stream;
	GSIXmlElement * searchValueElem = NULL;
	int i=0;

	if (reader->mValueReadIndex == -1)
		reader->mValueReadIndex = reader->mElemReadIndex; 

	for (i=(reader->mValueReadIndex+1); i < ArrayLength(reader->mElementArray); i++)
	{
		searchValueElem = (GSIXmlElement*)ArrayNth(reader->mElementArray, i);
		if (searchValueElem->mParentIndex == reader->mElemReadIndex)
		{
			if (gsi_is_true(gsiXmlUtilTagMatches(matchtag, &searchValueElem->mName)))
			{
				reader->mValueReadIndex = i;
				*valueOut = (const char*)searchValueElem->mValue.mData;
				*lenOut = searchValueElem->mValue.mLen;
				return gsi_true;
			}
		}
		if (searchValueElem->mParentIndex < reader->mElemReadIndex)
			return gsi_false;
	}
	return gsi_false;
}



gsi_bool gsXmlReadChildAsStringNT(GSXmlStreamReader stream, const char * matchtag, char valueOut[], int maxLen)
{
	const char * strValue = NULL;
	int strLen = 0;

	if (gsi_is_false(gsXmlReadChildAsString(stream, matchtag, &strValue, &strLen)))
	{
		valueOut[0] = '\0';
		return gsi_false;
	}
	else
	{
		strncpy(valueOut, strValue, (size_t)min(maxLen, strLen));
		valueOut[min(maxLen-1, strLen)] = '\0';
		return gsi_true;
	}
}

gsi_bool gsXmlReadChildAsUnicodeStringNT(GSXmlStreamReader stream, const char * matchtag, gsi_char valueOut[], int maxLen)
{
	const char * utf8Value = NULL;
	int unicodeLen = 0;
	int utf8Len = 0;

	if (gsi_is_false(gsXmlReadChildAsString(stream, matchtag, &utf8Value, &utf8Len)))
	{
		valueOut[0] = '\0';
		return gsi_false;
	}
	else
	{
		unicodeLen = UTF8ToUCS2StringLen(utf8Value, (unsigned short *)valueOut, utf8Len);
		valueOut[min(maxLen-1, unicodeLen)] = '\0';
		return gsi_true;
	}
}

gsi_bool gsXmlReadChildAsHexBinary(GSXmlStreamReader stream, const char * matchtag, 
								 gsi_u8 valueOut[], int maxLen, int * lenOut)
{
	GSIXmlStreamReader * reader = (GSIXmlStreamReader*)stream;
	GSIXmlElement * searchValueElem = NULL;
	int i=0;

	if (reader->mValueReadIndex == -1)
		reader->mValueReadIndex = reader->mElemReadIndex; 

	for (i=(reader->mValueReadIndex+1); i < ArrayLength(reader->mElementArray); i++)
	{
		searchValueElem = (GSIXmlElement*)ArrayNth(reader->mElementArray, i);
		if (searchValueElem->mParentIndex == reader->mElemReadIndex)
		{
			if (gsi_is_true(gsiXmlUtilTagMatches(matchtag, &searchValueElem->mName)))
			{
				gsi_u32 temp = 0;
				int writepos = 0;
				int readpos = 0;
				int bytesleft = min(maxLen*2, searchValueElem->mValue.mLen);

				if (searchValueElem->mValue.mLen == 0 || searchValueElem->mValue.mData == NULL)
				{
					valueOut[0] = 0;
					*lenOut = 0;
					return gsi_true;
				}

				if (valueOut == NULL)
				{
					*lenOut = searchValueElem->mValue.mLen / 2;

					return gsi_true;
				}

				while(bytesleft > 1)
				{
					sscanf((char*)(&searchValueElem->mValue.mData[readpos]), "%02x", &temp); 
					valueOut[writepos] = (gsi_u8)temp; 
					readpos += 2;
					writepos += 1;
					bytesleft -= 2;
				}
				if (bytesleft == 1)
				{
					sscanf((char*)(&searchValueElem->mValue.mData[readpos]), "%01x", &temp); 
					valueOut[writepos] = (gsi_u8)temp; 
					readpos += 1;
					writepos += 1;
					bytesleft -= 1;
				}
				if (lenOut != NULL)
					*lenOut = writepos;

				reader->mValueReadIndex = i; 
				return gsi_true;
			}
		}
		if (searchValueElem->mParentIndex < reader->mElemReadIndex)
			return gsi_false;
	}
	return gsi_false;
}

gsi_bool gsXmlReadChildAsBase64Binary(GSXmlStreamReader stream, const char * matchtag, gsi_u8 valueOut[], int * lenOut)
{
	GSIXmlStreamReader * reader = (GSIXmlStreamReader*)stream;
	GSIXmlElement * searchValueElem = NULL;
	int i=0;

	if (reader->mValueReadIndex == -1)
		reader->mValueReadIndex = reader->mElemReadIndex; 

	for (i=(reader->mValueReadIndex+1); i < ArrayLength(reader->mElementArray); i++)
	{
		searchValueElem = (GSIXmlElement*)ArrayNth(reader->mElementArray, i);
		if (searchValueElem->mParentIndex == reader->mElemReadIndex)
		{
			if (gsi_is_true(gsiXmlUtilTagMatches(matchtag, &searchValueElem->mName)))
			{
				if(valueOut)
				{
					reader->mValueReadIndex = i;
					if(searchValueElem->mValue.mData)
						B64Decode((char*)searchValueElem->mValue.mData, (char*)valueOut, searchValueElem->mValue.mLen, lenOut, GS_XML_BASE64_ENCODING_TYPE);
					else
						*lenOut = 0;
				}
				else
				{
					if(searchValueElem->mValue.mData)
						*lenOut = B64DecodeLen((const char*)searchValueElem->mValue.mData, GS_XML_BASE64_ENCODING_TYPE);
					else
						*lenOut = 0;
				}
				return gsi_true;
			}
		}
		if (searchValueElem->mParentIndex < reader->mElemReadIndex)
			return gsi_false;
	}
	return gsi_false;
}

gsi_bool gsXmlReadChildAsInt    (GSXmlStreamReader stream, const char * matchtag, 
								 int * valueOut)
{
	GSIXmlStreamReader * reader = (GSIXmlStreamReader*)stream;
	GSIXmlElement * searchValueElem = NULL;
	int i=0;

	if (reader->mValueReadIndex == -1)
		reader->mValueReadIndex = reader->mElemReadIndex; 

	for (i=(reader->mValueReadIndex+1); i < ArrayLength(reader->mElementArray); i++)
	{
		searchValueElem = (GSIXmlElement*)ArrayNth(reader->mElementArray, i);
		if (searchValueElem->mParentIndex == reader->mElemReadIndex)
		{
			if (gsi_is_true(gsiXmlUtilTagMatches(matchtag, &searchValueElem->mName)))
			{
				reader->mValueReadIndex = i;
				if (searchValueElem->mValue.mData == NULL)
					return gsi_false; 
				*valueOut = atoi((const char*)searchValueElem->mValue.mData);
				return gsi_true;
			}
		}
		if (searchValueElem->mParentIndex < reader->mElemReadIndex)
			return gsi_false;
	}
	return gsi_false;
}


gsi_bool gsXmlReadChildAsInt64(GSXmlStreamReader stream, const char * matchtag, 
							   gsi_i64 * valueOut)
{
	GSIXmlStreamReader * reader = (GSIXmlStreamReader*)stream;
	GSIXmlElement * searchValueElem = NULL;
	int i=0;

	if (reader->mValueReadIndex == -1)
		reader->mValueReadIndex = reader->mElemReadIndex; 

	for (i=(reader->mValueReadIndex+1); i < ArrayLength(reader->mElementArray); i++)
	{
		searchValueElem = (GSIXmlElement*)ArrayNth(reader->mElementArray, i);
		if (searchValueElem->mParentIndex == reader->mElemReadIndex)
		{
			if (gsi_is_true(gsiXmlUtilTagMatches(matchtag, &searchValueElem->mName)))
			{
				reader->mValueReadIndex = i;
				if (searchValueElem->mValue.mData == NULL)
					return gsi_false; 
				*valueOut = gsiStringToInt64((const char*)searchValueElem->mValue.mData);
				return gsi_true;
			}
		}
		if (searchValueElem->mParentIndex < reader->mElemReadIndex)
			return gsi_false;
	}
	return gsi_false;
}


gsi_bool gsXmlReadChildAsDateTimeElement    (GSXmlStreamReader stream, const char * matchtag, 
											 time_t * valueOut)
{
	GSIXmlStreamReader * reader = (GSIXmlStreamReader*)stream;
	GSIXmlElement * searchValueElem = NULL;
	int i=0;	
	struct tm timePtr;


	if (reader->mValueReadIndex == -1)
		reader->mValueReadIndex = reader->mElemReadIndex; 

	for (i=(reader->mValueReadIndex+1); i < ArrayLength(reader->mElementArray); i++)
	{
		searchValueElem = (GSIXmlElement*)ArrayNth(reader->mElementArray, i);
		if (searchValueElem->mParentIndex == reader->mElemReadIndex)
		{
			if (gsi_is_true(gsiXmlUtilTagMatches(matchtag, &searchValueElem->mName)))
			{
				reader->mValueReadIndex = i;
				if (searchValueElem->mValue.mData == NULL)
					return gsi_false; 

				sscanf((const char*)searchValueElem->mValue.mData, "%i-%02d-%02dT%02d:%02d:%02d",
					&timePtr.tm_year, &timePtr.tm_mon, &timePtr.tm_mday,
					&timePtr.tm_hour, &timePtr.tm_min, &timePtr.tm_sec);

				timePtr.tm_year -= 1900;
				timePtr.tm_mon -= 1;
				timePtr.tm_isdst = -1;
				*valueOut = gsiDateToSeconds(&timePtr);

				return gsi_true;
			}
		}
		if (searchValueElem->mParentIndex < reader->mElemReadIndex)
			return gsi_false;
	}
	return gsi_false;
}

gsi_bool gsXmlReadChildAsFloat  (GSXmlStreamReader stream, const char * matchtag, 
								 float * valueOut)
{
	GSIXmlStreamReader * reader = (GSIXmlStreamReader*)stream;
	GSIXmlElement * searchValueElem = NULL;
	int i=0;

	if (reader->mValueReadIndex == -1)
		reader->mValueReadIndex = reader->mElemReadIndex; 

	for (i=(reader->mValueReadIndex+1); i < ArrayLength(reader->mElementArray); i++)
	{
		searchValueElem = (GSIXmlElement*)ArrayNth(reader->mElementArray, i);
		if (searchValueElem->mParentIndex == reader->mElemReadIndex)
		{
			if (gsi_is_true(gsiXmlUtilTagMatches(matchtag, &searchValueElem->mName)))
			{
				reader->mValueReadIndex = i;
				if (searchValueElem->mValue.mData == NULL)
					return gsi_false; 
				*valueOut = (float)atof((const char*)searchValueElem->mValue.mData);
				return gsi_true;
			}
		}
		if (searchValueElem->mParentIndex < reader->mElemReadIndex)
			return gsi_false;
	}
	return gsi_false;
}


static gsi_bool gsiXmlUtilTagMatches(const char * matchtag, GSIXmlString * xmlstr)
{
	const char * matchNoNamespace = NULL;
	GSIXmlString xmlNoNamespace;
	int xmlNamespacePos=0;

	GS_ASSERT(xmlstr != NULL);

	if (matchtag == NULL)
		return gsi_true;
	if (matchtag[strlen(matchtag)-1] == ':')
		return gsi_false; 

	matchNoNamespace = strchr(matchtag, ':');
	if (matchNoNamespace == NULL)
		matchNoNamespace = matchtag;

	while(xmlNamespacePos < xmlstr->mLen && xmlstr->mData[xmlNamespacePos] != ':')
		xmlNamespacePos++;
	if (xmlNamespacePos == xmlstr->mLen)
		xmlNamespacePos=0;
	else
		xmlNamespacePos++; 
	xmlNoNamespace.mData = xmlstr->mData + xmlNamespacePos;
	xmlNoNamespace.mLen = xmlstr->mLen - xmlNamespacePos;

	if (0 == strncmp(matchNoNamespace, (const char*)xmlNoNamespace.mData, (size_t)xmlNoNamespace.mLen))
		return gsi_true;
	else
		return gsi_false;
}


gsi_bool gsXmlReadChildAsLargeInt(GSXmlStreamReader stream, const char * matchtag, struct gsLargeInt_s * valueOut)
{
	int len = 0;

	memset(valueOut, 0, sizeof(struct gsLargeInt_s));

	if (gsi_is_false(gsXmlReadChildAsHexBinary(stream, matchtag, (gsi_u8*)valueOut->mData, GS_LARGEINT_BINARY_SIZE/8*2, &len)))
		return gsi_false;

	valueOut->mLength = (l_word)(len/GS_LARGEINT_DIGIT_SIZE_BYTES);
	if (len%GS_LARGEINT_DIGIT_SIZE_BYTES != 0)
		valueOut->mLength++;

	if (gsi_is_false(gsLargeIntReverseBytes(valueOut)))
		return gsi_false;
	else
		return gsi_true;
}


gsi_bool gsXmlResetChildReadPosition(GSXmlStreamReader stream)
{
	GSIXmlStreamReader * reader = (GSIXmlStreamReader*)stream;
	reader->mValueReadIndex = -1; 
	return gsi_true;
}


int gsXmlCountChildren(GSXmlStreamReader stream, const char * matchtag)
{
	GSIXmlStreamReader * reader = (GSIXmlStreamReader*)stream;
	GSIXmlElement * searchElem = NULL;
	int i=0;
	int count=0;

	for (i=(reader->mElemReadIndex+1); i < ArrayLength(reader->mElementArray); i++)
	{
		searchElem = (GSIXmlElement*)ArrayNth(reader->mElementArray, i);
		if (searchElem->mParentIndex == reader->mElemReadIndex)
		{
			if ((matchtag == NULL) || gsi_is_true(gsiXmlUtilTagMatches(matchtag, &searchElem->mName)))
			{
				count++;
			}
		}
		else if (searchElem->mParentIndex < reader->mElemReadIndex)
			break;
	}
	return count;
}



