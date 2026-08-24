/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#ifndef __GSXML_H__
#define __GSXML_H__


#include "gsPlatform.h"
#include "gsLargeInt.h"  


#if defined(__cplusplus)
extern "C"
{
#endif




#define	GS_XML_MAX_ELEMENT_NAME_LENGTH		128
#define GS_XML_MAX_ATTRIBUTE_NAME_LENGTH	128
#define GS_XML_MAX_ATTRIBUTE_VALUE_LENGTH	1024


typedef void* GSXmlStreamReader;
typedef void* GSXmlStreamWriter;

struct gsLargeInt_s; 


GSXmlStreamWriter gsXmlCreateStreamWriter(const char ** namespaces, int count);
GSXmlStreamReader gsXmlCreateStreamReader();
void gsXmlFreeReader(GSXmlStreamReader stream);
void gsXmlFreeWriter(GSXmlStreamWriter stream);

void gsXmlResetReader(GSXmlStreamReader stream); 

const char * gsXmlWriterGetData       (GSXmlStreamWriter stream);
int      gsXmlWriterGetDataLength     (GSXmlStreamWriter stream);
gsi_bool gsXmlCloseWriter       (GSXmlStreamWriter stream);
gsi_bool gsXmlWriteStringElement(GSXmlStreamWriter stream, const char * namespaceName, const char * tag, const char * value);
gsi_bool gsXmlWriteAsciiStringElement(GSXmlStreamWriter stream, const char * namespaceName, const char * tag, const gsi_char * value);
gsi_bool gsXmlWriteUnicodeStringElement(GSXmlStreamWriter stream, const char * namespaceName, const char * tag, const unsigned short * value);
gsi_bool gsXmlWriteIntElement   (GSXmlStreamWriter stream, const char * namespaceName, const char * tag, gsi_u32 value);
gsi_bool gsXmlWriteInt64Element   (GSXmlStreamWriter stream, const char * namespaceName, const char * tag, gsi_i64 value);
gsi_bool gsXmlWriteFloatElement (GSXmlStreamWriter stream, const char * namespaceName, const char * tag, float value);
gsi_bool gsXmlWriteHexBinaryElement(GSXmlStreamWriter stream, const char * namespaceName, const char * tag, const gsi_u8 * data, int len);
gsi_bool gsXmlWriteBase64BinaryElement(GSXmlStreamWriter stream, const char * namespaceName, const char * tag, const gsi_u8 * data, int len);
gsi_bool gsXmlWriteDateTimeElement(GSXmlStreamWriter stream, const char * namespaceName, const char * tag, time_t value);
gsi_bool gsXmlWriteLargeIntElement(GSXmlStreamWriter stream, const char * namespaceName, const char * tag, const struct gsLargeInt_s * lint);
gsi_bool gsXmlWriteOpenTag      (GSXmlStreamWriter stream, const char * namespaceName, const char * tag);
gsi_bool gsXmlWriteCloseTag     (GSXmlStreamWriter stream, const char * namespaceName, const char * tag);

gsi_bool gsXmlParseBuffer       (GSXmlStreamReader stream, char * buffer, int len);
gsi_bool gsXmlMoveToStart       (GSXmlStreamReader stream);
gsi_bool gsXmlMoveToParent      (GSXmlStreamReader stream);
gsi_bool gsXmlMoveToNext        (GSXmlStreamReader stream, const char * matchtag);
gsi_bool gsXmlMoveToSibling     (GSXmlStreamReader stream, const char * matchtag);
gsi_bool gsXmlMoveToChild       (GSXmlStreamReader stream, const char * matchtag);
gsi_bool gsXmlReadChildAsString   (GSXmlStreamReader stream, const char * matchtag, const char ** valueOut, int * lenOut);
gsi_bool gsXmlReadChildAsStringNT (GSXmlStreamReader stream, const char * matchtag, char valueOut[], int maxLen);
gsi_bool gsXmlReadChildAsUnicodeStringNT (GSXmlStreamReader stream, const char * matchtag, gsi_char valueOut[], int maxLen);
gsi_bool gsXmlReadChildAsInt      (GSXmlStreamReader stream, const char * matchtag, int * valueOut);
gsi_bool gsXmlReadChildAsInt64    (GSXmlStreamReader stream, const char * matchtag, gsi_i64 * valueOut);
gsi_bool gsXmlReadChildAsFloat    (GSXmlStreamReader stream, const char * matchtag, float * valueOut);
gsi_bool gsXmlReadChildAsDateTimeElement(GSXmlStreamReader stream, const char * matchtag, time_t * valueOut);
gsi_bool gsXmlResetChildReadPosition(GSXmlStreamReader stream); 

gsi_bool gsXmlReadChildAsHexBinary(GSXmlStreamReader stream, const char * matchtag, gsi_u8 valueOut[], int maxLen, int * lenOut);

gsi_bool gsXmlReadChildAsBase64Binary(GSXmlStreamReader stream, const char * matchtag, gsi_u8 valueOut[], int * lenOut);

gsi_bool gsXmlReadChildAsLargeInt(GSXmlStreamReader stream, const char * matchtag, struct gsLargeInt_s * valueOut);

int      gsXmlCountChildren     (GSXmlStreamReader stream, const char * matchtag);

#ifdef GSI_UNICODE
	#define gsXmlWriteTStringElement(s,n,t,v)	gsXmlWriteUnicodeStringElement(s,n,t,v)
	#define gsXmlReadChildAsTStringNT(s,m,v,l)	gsXmlReadChildAsUnicodeStringNT(s,m,v,l)
#else
	#define gsXmlWriteTStringElement(s,n,t,v)	gsXmlWriteStringElement(s,n,t,v)
	#define gsXmlReadChildAsTStringNT(s,m,v,l)	gsXmlReadChildAsStringNT(s,m,v,l)
#endif


#if defined(__cplusplus)
} 
#endif

#endif 
