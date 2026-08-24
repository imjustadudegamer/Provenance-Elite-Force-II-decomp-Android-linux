/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#ifndef _GT_ENCODE_H
#define _GT_ENCODE_H

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(UNDER_CE) || defined(__mips64) || defined(_PSP)
#define ALIGNED_COPY
#endif




typedef unsigned short GTMessageType;

int gtEncode(GTMessageType msgType, const char *fmtString, char *outBuffer, int outLength, ...);
int gtEncodeV(GTMessageType msgType, const char *fmtString, char *outBuffer, int outLength, va_list *args);
int gtEncodeNoType(const char *fmtString, char *outBuffer, int outLength, ...);
int gtEncodeNoTypeV(const char *fmtString, char *outBuffer, int outLength, va_list *args);

int gtDecode(const char *fmtString, char *inBuffer, int inLength, ...);
int gtDecodeV(const char *fmtString, char *inBuffer, int inLength, 	va_list *args);
int gtDecodeNoType(const char *fmtString, char *inBuffer, int inLength, ...);
int gtDecodeNoTypeV(const char *fmtString, char *inBuffer, int inLength, va_list *args);

GTMessageType	gtEncodedMessageType	(char *inBuffer);
void			gtEncodedMessageTypeSet	(char *inBuffer, GTMessageType newtype);

void gt2MemCopy16(char *out, char const *in);
void gt2MemCopy32(char *out, char const *in);
void gt2MemCopy64(char *out, char const *in);
void gt2MemCopy(char *out, char const *in, int size);


#define GT_INT		'i'
#define GT_INT_		"i"
#define GT_INT_TYPE	int
#define GT_UINT		'u'
#define GT_UINT_	"u"
#define GT_UINT_TYPE	unsigned int
#define GT_SHORT	'o'
#define GT_SHORT_	"o"
#define GT_SHORT_TYPE	short
#define GT_USHORT	'p'
#define GT_USHORT_	"p"
#define GT_USHORT_TYPE	unsigned short
#define GT_CHAR		'c'
#define GT_CHAR_	"c"
#define GT_CHAR_TYPE	signed char
#define GT_UCHAR	'b'
#define GT_UCHAR_	"b"
#define GT_UCHAR_TYPE	unsigned char
#define GT_FLOAT	'f'
#define GT_FLOAT_	"f"
#define GT_FLOAT_TYPE	float
#define GT_DOUBLE	'd'
#define GT_DOUBLE_	"d"
#define GT_DOUBLE_TYPE	double
#define GT_CSTR		's'
#define GT_CSTR_	"s"
#define GT_CSTR_TYPE	char *
#define GT_CSTR_PTR 'S'
#define GT_CSTR_PTR_ "S"
#define GT_CSTR_PTR_TYPE	char **
#define GT_DBSTR	'w'
#define GT_DBSTR_	"w"
#define GT_DBSTR_TYPE	short *
#define GT_DBSTR_PTR 'W'
#define GT_DBSTR_PTR_ "W"
#define GT_DBSTR_PTR_TYPE	short **
#define GT_CSTR_ARRAY 'a'
#define GT_CSTR_ARRAY_ "a"
#define GT_CSTR_ARRAY_TYPE  char *
#define GT_CSTR_ARRAY_PTR        'A'
#define GT_CSTR_ARRAY_PTR_       "A"
#define GT_CSTR_ARRAY_PTR_TYPE   char **
#define GT_RAW		'r' 
#define GT_RAW_		"r" 
#define GT_RAW_TYPE		char *
#define GT_RAW_PTR	'R'
#define GT_RAW_PTR_	"R"
#define GT_RAW_PTR_TYPE		char **
#define GT_BIT		'z'
#define GT_BIT_		"z"
#define GT_BIT_TYPE		unsigned char


#ifdef __cplusplus
}
#endif

#endif
