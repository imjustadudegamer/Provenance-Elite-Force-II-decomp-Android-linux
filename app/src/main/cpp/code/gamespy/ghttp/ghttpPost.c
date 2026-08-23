
#include "ghttpPost.h"
#include "ghttpMain.h"
#include "ghttpConnection.h"
#include "ghttpCommon.h"

#include "../common/gsCrypt.h"
#include "../common/gsSSL.h"
#include "../common/gsXML.h"


#define GHI_MULTIPART_BOUNDARY          "Qr4G823s23d---<<><><<<>--7d118e0536"
#define GHI_MULTIPART_BOUNDARY_BASE     "--" GHI_MULTIPART_BOUNDARY
#define GHI_MULTIPART_BOUNDARY_FIRST    GHI_MULTIPART_BOUNDARY_BASE CRLF
#define GHI_MULTIPART_BOUNDARY_NORMAL   CRLF GHI_MULTIPART_BOUNDARY_BASE CRLF
#define GHI_MULTIPART_BOUNDARY_END      CRLF GHI_MULTIPART_BOUNDARY_BASE "--" CRLF

#define GHI_LEGAL_URLENCODED_CHARS      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_@-.*"
#define GHI_DIGITS                      "0123456789ABCDEF"

#define GHI_DIME_VERSION         (0x1<<3) 
#define GHI_DIMEFLAG_FIRSTRECORD (1<<2)
#define GHI_DIMEFLAG_LASTRECORD  (1<<1)
#define GHI_DIMEFLAG_CHUNKED     (1<<0)
#define GHI_DIMETYPE_T_UNCHANGED (0x0 << 4)
#define GHI_DIMETYPE_T_MEDIA     (0x1 << 4)
#define GHI_DIMETYPE_T_URI       (0x2 << 4)
#define GHI_DIMETYPE_T_UNKNOWN   (0x3 << 4)
#define GHI_DIMETYPE_T_EMPTY     (0x4 << 4) 

#define GHI_DIME_SOAPID "cid:id0"
#define GHI_DIME_SOAPTYPE "http://schemas.xmlsoap.org/soap/envelope/"

typedef struct GSIDimeHeader
{
	gsi_u8 mVersionAndFlags;
	gsi_u8 mTypeT;
	gsi_u16 mOptionsLength;
	gsi_u16 mIdLength;
	gsi_u16 mTypeLength;
	gsi_u32 mDataLength;
} GHIDimeHeader;

typedef enum
{
	GHIString,      
	GHIFileDisk,    
	GHIFileMemory,  
	GHIXmlData      
} GHIPostDataType;

typedef struct GHIPost
{
	DArray data;
	ghttpPostCallback callback;
	void * param;
	GHTTPBool hasFiles;
	GHTTPBool hasSoap;
	GHTTPBool useDIME;
	GHTTPBool autoFree;
} GHIPost;

typedef struct GHIPostStringData
{
	char * string;
	int len;
	GHTTPBool invalidChars;
	int extendedChars;
} GHIPostStringData;

typedef struct GHIPostFileDiskData
{
	char * filename;
	char * reportFilename;
	char * contentType;
} GHIPostFileDiskData;

typedef struct GHIPostFileMemoryData
{
	const char * buffer;
	int len;
	char * reportFilename;
	char * contentType;
} GHIPostFileMemoryData;

typedef struct GHIPostXmlData
{
	GSXmlStreamWriter xml;
} GHIPostXmlData;

typedef struct GHIPostData
{
	GHIPostDataType type;
	char * name;
	union
	{
		GHIPostStringData string;
		GHIPostFileDiskData fileDisk;
		GHIPostFileMemoryData fileMemory;
		GHIPostXmlData xml;
	} data;
} GHIPostData;


typedef struct GHIPostFileDiskState
{
	FILE * file;
	long len;
} GHIPostFileDiskState;




typedef struct GHIPostState
{
	GHIPostData * data;
	int pos;
	union
	{
		GHIPostFileDiskState fileDisk;
	} state;
} GHIPostState;

static void ghiPostDataFree
(
	void * elem
)
{
	GHIPostData * data = (GHIPostData *)elem;

	if (data->type != GHIXmlData)
		gsifree(data->name);

	if(data->type == GHIString)
	{
		gsifree(data->data.string.string);
	}
	else if(data->type == GHIFileDisk)
	{
		gsifree(data->data.fileDisk.filename);
		gsifree(data->data.fileDisk.reportFilename);
		gsifree(data->data.fileDisk.contentType);
	}
	else if(data->type == GHIFileMemory)
	{
		gsifree(data->data.fileMemory.reportFilename);
		gsifree(data->data.fileMemory.contentType);
	}
	else if(data->type == GHIXmlData)
	{
		gsXmlFreeWriter(data->data.xml.xml);
	}
	else
	{
		assert(0);
	}
}

GHTTPPost ghiNewPost
(
	void
)
{
	GHIPost * post;

	post = (GHIPost *)gsimalloc(sizeof(GHIPost));
	if(!post)
		return NULL;

	memset(post, 0, sizeof(GHIPost));
	post->autoFree = GHTTPTrue;

	post->data = ArrayNew(sizeof(GHIPostData), 0, ghiPostDataFree);
	if(!post->data)
	{
		gsifree(post);
		return NULL;
	}

	return (GHTTPPost)post;
}

void ghiPostSetAutoFree
(
	GHTTPPost _post,
	GHTTPBool autoFree
)
{
	GHIPost * post = (GHIPost *)_post;

	post->autoFree = autoFree;
}

GHTTPBool ghiIsPostAutoFree
(
	GHTTPPost _post
)
{
	GHIPost * post = (GHIPost *)_post;

	return post->autoFree;
}

void ghiFreePost
(
	GHTTPPost _post
)
{
	GHIPost * post = (GHIPost *)_post;

	ArrayFree(post->data);

	gsifree(post);
}

GHTTPBool ghiPostAddString
(
	GHTTPPost _post,
	const char * name,
	const char * string
)
{
	GHIPost * post = (GHIPost *)_post;
	GHIPostData data;
	int len;
	int rcode;

	name = goastrdup(name);
	string = goastrdup(string);
	if(!name || !string)
	{
		gsifree((char *)name);
		gsifree((char *)string);
		return GHTTPFalse;
	}

	memset(&data, 0, sizeof(GHIPostData));
	data.type = GHIString;
	data.name = (char *)name;
	data.data.string.string = (char *)string;
	len = (int)strlen(string);
	data.data.string.len = len;
	data.data.string.invalidChars = GHTTPFalse;

	rcode = (int)strspn(string, GHI_LEGAL_URLENCODED_CHARS);
	if(rcode != len)
	{
		int i;
		int count = 0;

		data.data.string.invalidChars = GHTTPTrue;

		for(i = 0 ; string[i] ; i++)
			if(!strchr(GHI_LEGAL_URLENCODED_CHARS, string[i]) && (string[i] != ' '))
				count++;

		data.data.string.extendedChars = count;
	}

	ArrayAppend(post->data, &data);

	return GHTTPTrue;
}

GHTTPBool ghiPostAddFileFromDisk
(
	GHTTPPost _post,
	const char * name,
	const char * filename,
	const char * reportFilename,
	const char * contentType
)
{
	GHIPost * post = (GHIPost *)_post;
	GHIPostData data;

	name = goastrdup(name);
	filename = goastrdup(filename);
	reportFilename = goastrdup(reportFilename);
	contentType = goastrdup(contentType);
	if(!name || !filename || !reportFilename || !contentType)
	{
		gsifree((char *)name);
		gsifree((char *)filename);
		gsifree((char *)reportFilename);
		gsifree((char *)contentType);
		return GHTTPFalse;
	}

	memset(&data, 0, sizeof(GHIPostData));
	data.type = GHIFileDisk;
	data.name = (char *)name;
	data.data.fileDisk.filename = (char *)filename;
	data.data.fileDisk.reportFilename = (char *)reportFilename;
	data.data.fileDisk.contentType = (char *)contentType;

	ArrayAppend(post->data, &data);

	post->hasFiles = GHTTPTrue;

	if (post->hasSoap == GHTTPTrue)
		post->useDIME = GHTTPTrue;

	return GHTTPTrue;
}

GHTTPBool ghiPostAddFileFromMemory
(
	GHTTPPost _post,
	const char * name,
	const char * buffer,
	int bufferLen,
	const char * reportFilename,
	const char * contentType
)
{
	GHIPost * post = (GHIPost *)_post;
	GHIPostData data;

	name = goastrdup(name);
	reportFilename = goastrdup(reportFilename);
	contentType = goastrdup(contentType);
	if(!name || !reportFilename || !contentType)
	{
		gsifree((char *)name);
		gsifree((char *)reportFilename);
		gsifree((char *)contentType);
		return GHTTPFalse;
	}

	memset(&data, 0, sizeof(GHIPostData));
	data.type = GHIFileMemory;
	data.name = (char *)name;
	data.data.fileMemory.buffer = (char *)buffer;
	data.data.fileMemory.len = bufferLen;
	data.data.fileMemory.reportFilename = (char *)reportFilename;
	data.data.fileMemory.contentType = (char *)contentType;

	ArrayAppend(post->data, &data);

	post->hasFiles = GHTTPTrue;

	if (post->hasSoap == GHTTPTrue)
		post->useDIME = GHTTPTrue;

	return GHTTPTrue;
}

GHTTPBool ghiPostAddXml
(
	GHTTPPost _post,
	GSXmlStreamWriter xml
)
{
	GHIPostData data;

	GHIPost * post = (GHIPost *)_post;

	data.type = GHIXmlData;
	data.data.xml.xml = xml;
	ArrayAppend(post->data, &data);
	post->hasSoap = GHTTPTrue;

	if (post->hasFiles == GHTTPTrue)
		post->useDIME = GHTTPTrue;

	return GHTTPTrue;
}

void ghiPostSetCallback
(
	GHTTPPost _post,
	ghttpPostCallback callback,
	void * param
)
{
	GHIPost * post = (GHIPost *)_post;

	post->callback = callback;
	post->param = param;
}

const char * ghiPostGetContentType
(
	struct GHIConnection * connection
)
{
	GHIPost * post = connection->post;

	assert(post);
	if(!post)
		return "";

	if(post->useDIME)
		return ("application/dime");
	else if (post->hasFiles)
	{
		GS_ASSERT(!post->hasSoap);
		return ("multipart/form-data; boundary=" GHI_MULTIPART_BOUNDARY);
	}
	else if (post->hasSoap)
	{
		GS_ASSERT(!post->hasFiles);
		return ("text/xml");
	}
	else
	{
		GS_ASSERT(!post->hasSoap);
		GS_ASSERT(!post->hasFiles);
		return "application/x-www-form-urlencoded";
	}
}

static int ghiPostGetNoFilesContentLength
(
	struct GHIConnection * connection
)
{
	GHIPost * post = connection->post;
	GHIPostData * data;
	int i;
	int num;
	int total = 0;
	int foundSoapAlready = 0;

	num = ArrayLength(post->data);
	if(!num)
		return 0;

	for(i = 0 ; i < num ; i++)
	{
		data = (GHIPostData *)ArrayNth(post->data, i);

		GS_ASSERT(data->type == GHIString || data->type == GHIXmlData);

		if (data->type == GHIString)
		{
			total += (int)strlen(data->name);
			total += data->data.string.len;
			total += (data->data.string.extendedChars * 2);
			total++;  
		}
		else if (data->type == GHIXmlData)
		{
			GS_ASSERT(foundSoapAlready == 0); 
			foundSoapAlready = 1;
			total += gsXmlWriterGetDataLength(data->data.xml.xml);
		}
	}

	total += (num - 1);  

	GSI_UNUSED(foundSoapAlready);
	return total;
}

static int ghiPostGetHasFilesContentLength
(
	struct GHIConnection * connection
)
{
	GHIPost * post = connection->post;
	GHIPostData * data;
	int i;
	int num;
	int total = 0;
	int foundSoapAlready = 0;
	static int boundaryLen;
	static int stringBaseLen;
	static int fileBaseLen;
	static int endLen;
	static int xmlBaseLen;
	
	if(!boundaryLen)
	{
		if (post->useDIME)
		{
			GS_ASSERT(post->hasSoap);
			GS_ASSERT(post->hasFiles);
			boundaryLen = sizeof(GHIDimeHeader);
			stringBaseLen = boundaryLen;
			fileBaseLen = boundaryLen;
			xmlBaseLen = boundaryLen;
			endLen = 0;
		}
		else
		{
			GS_ASSERT(!post->hasSoap);
			boundaryLen = (int)strlen(GHI_MULTIPART_BOUNDARY_BASE);
			stringBaseLen = (boundaryLen + 47);  
			fileBaseLen = (boundaryLen + 76);  
			xmlBaseLen = 0; 
			endLen = (boundaryLen + 4);
		}
	}

	num = ArrayLength(post->data);

	for(i = 0 ; i < num ; i++)
	{
		data = (GHIPostData *)ArrayNth(post->data, i);

		if(data->type == GHIString)
		{
			total += stringBaseLen;
			total += (int)strlen(data->name);
			total += data->data.string.len;
		}
		else if(data->type == GHIFileDisk)
		{
			GHIPostState * state;

			total += fileBaseLen;
			total += (int)strlen(data->name);
			total += (int)strlen(data->data.fileDisk.contentType);
			state = (GHIPostState *)ArrayNth(connection->postingState.states, i);
			assert(state);
			total += (int)state->state.fileDisk.len;

			if (!post->useDIME)
				total += (int)strlen(data->data.fileDisk.reportFilename);

			if (post->useDIME)
			{
				int padBytes = 0;

				padBytes = 4-(int)strlen(data->name)%4;
				if (padBytes != 4)
					total += padBytes;
				padBytes = 4-(int)strlen(data->data.fileDisk.contentType)%4;
				if (padBytes != 4)
					total += padBytes;
				padBytes = 4-(int)state->state.fileDisk.len%4;
				if (padBytes != 4)
					total += padBytes;
			}
		}
		else if(data->type == GHIFileMemory)
		{
			total += fileBaseLen;
			total += (int)strlen(data->name);
			total += (int)strlen(data->data.fileMemory.contentType);
			total += data->data.fileMemory.len;

			if (!post->useDIME)
				total += (int)strlen(data->data.fileMemory.reportFilename);

			if (post->useDIME)
			{
				int padBytes = 0;

				padBytes = 4-(int)strlen(data->name)%4;
				if (padBytes != 4)
					total += padBytes;
				padBytes = 4-(int)strlen(data->data.fileMemory.contentType)%4;
				if (padBytes != 4)
					total += padBytes;
				padBytes = 4-(int)data->data.fileMemory.len%4;
				if (padBytes != 4)
					total += padBytes;
			}
		}
		else if(data->type == GHIXmlData)
		{
			int padBytes = 0;

			GS_ASSERT(foundSoapAlready == 0); 
			GS_ASSERT(post->useDIME); 
			foundSoapAlready = 1;
			total += xmlBaseLen;
			total += gsXmlWriterGetDataLength(data->data.xml.xml);

			padBytes = 4-(int)gsXmlWriterGetDataLength(data->data.xml.xml)%4;
			if (padBytes != 4)
				total += padBytes;
			total += (int)strlen(GHI_DIME_SOAPID);
			padBytes = 4-(int)strlen(GHI_DIME_SOAPID)%4;
			if (padBytes != 4)
				total += padBytes;
			total += (int)strlen(GHI_DIME_SOAPTYPE);
			padBytes = 4-(int)strlen(GHI_DIME_SOAPTYPE)%4;
			if (padBytes != 4)
				total += padBytes;
		}
		else
		{
			assert(0);
			return 0;
		}
	}

	total += endLen;

	GSI_UNUSED(foundSoapAlready);
	return total;
}

static int ghiPostGetContentLength
(
	struct GHIConnection * connection
)
{
	GHIPost * post = connection->post;

	assert(post);
	if(!post)
		return 0;

	if(post->hasFiles)
		return ghiPostGetHasFilesContentLength(connection);

	return ghiPostGetNoFilesContentLength(connection);
}

static GHTTPBool ghiPostStateInit
(
	GHIPostState * state
)
{
	GHIPostDataType type;

	type = state->data->type;

	state->pos = -1;

	if(type == GHIString)
	{
	}
	else if(type == GHIFileDisk)
	{
#ifndef NOFILE
		state->state.fileDisk.file = fopen(state->data->data.fileDisk.filename, "rb");
#endif
		if(!state->state.fileDisk.file)
			return GHTTPFalse;

		if(fseek(state->state.fileDisk.file, 0, SEEK_END) != 0)
			return GHTTPFalse;
		state->state.fileDisk.len = ftell(state->state.fileDisk.file);
		if(state->state.fileDisk.len == EOF)
			return GHTTPFalse;
		rewind(state->state.fileDisk.file);
	}
	else if(type == GHIFileMemory)
	{
	}
	else if(type == GHIXmlData)
	{
	}
	else
	{
		assert(0);

		return GHTTPFalse;
	}

	return GHTTPTrue;
}

static void ghiPostStateCleanup
(
	GHIPostState * state
)
{
	GHIPostDataType type;

	type = state->data->type;

	if(type == GHIString)
	{
	}
	else if(type == GHIFileDisk)
	{
		if(state->state.fileDisk.file)
			fclose(state->state.fileDisk.file);
		state->state.fileDisk.file = NULL;
	}
	else if(type == GHIFileMemory)
	{
	}
	else if(type == GHIXmlData)
	{
	}
	else
	{
		assert(0);
	}
}

GHTTPBool ghiPostInitState
(
	struct GHIConnection * connection
)
{
	int i;
	int len;
	GHIPostData * data;
	GHIPostState state;
	GHIPostState * pState;

	assert(connection->post);
	if(!connection->post)
		return GHTTPFalse;

	connection->postingState.index = 0;
	connection->postingState.bytesPosted = 0;
	connection->postingState.totalBytes = 0;
	connection->postingState.completed = GHTTPFalse;
	connection->postingState.callback = connection->post->callback;
	connection->postingState.param = connection->post->param;
	len = ArrayLength(connection->post->data);
	connection->postingState.states = ArrayNew(sizeof(GHIPostState), len, NULL);
	if(!connection->postingState.states)
		return GHTTPFalse;

	for(i = 0 ; i < len ; i++)
	{
		data = (GHIPostData *)ArrayNth(connection->post->data, i);

		memset(&state, 0, sizeof(GHIPostState));
		state.data = data;

		if(!ghiPostStateInit(&state))
		{
			for(i-- ; i >= 0 ; i--)
			{
				pState = (GHIPostState *)ArrayNth(connection->postingState.states, i);
				ghiPostStateCleanup(pState);
			}

			ArrayFree(connection->postingState.states);
			connection->postingState.states = NULL;

			return GHTTPFalse;
		}

		ArrayAppend(connection->postingState.states, &state);
	}

	assert(ArrayLength(connection->post->data) == ArrayLength(connection->postingState.states));

	connection->postingState.totalBytes = ghiPostGetContentLength(connection);

	if (connection->post->hasSoap == GHTTPTrue)
		connection->postingState.waitPostContinue = GHTTPTrue;
	else
		connection->postingState.waitPostContinue = GHTTPFalse;

	return GHTTPTrue;
}

void ghiPostCleanupState
(
	struct GHIConnection * connection
)
{
	int i;
	int len;
	GHIPostState * state;

	if(connection->postingState.states)
	{
		len = ArrayLength(connection->postingState.states);
		for(i = 0 ; i < len ; i++)
		{
			state = (GHIPostState *)ArrayNth(connection->postingState.states, i);
			ghiPostStateCleanup(state);
		}

		ArrayFree(connection->postingState.states);
		connection->postingState.states = NULL;
	}

	if(connection->post && connection->post->autoFree)
	{
		ghiFreePost(connection->post);
		connection->post = NULL;
	}
}

static GHIPostingResult ghiPostStringStateDoPosting
(
	GHIPostState * state,
	GHIConnection * connection
)
{
	
	assert(state->pos >= 0);

	if(state->data->data.string.len == 0)
		return GHIPostingDone;

	assert(state->pos < state->data->data.string.len);

	if(!connection->post->hasFiles && !connection->post->hasSoap && state->data->data.string.invalidChars)
	{
		int i;
		int c;
		const char * string = state->data->data.string.string;
		char hex[4] = "%00";
		GHIBuffer *writeBuffer;

		if (connection->encryptor.mEngine == GHTTPEncryptionEngine_None)
			writeBuffer = &connection->sendBuffer; 
		else
			writeBuffer = &connection->encodeBuffer;

		for(i = 0 ; (c = string[i]) != 0 ; i++)
		{
			if(strchr(GHI_LEGAL_URLENCODED_CHARS, c))
			{
				ghiAppendCharToBuffer(writeBuffer, c);
			}
			else if(c == ' ')
			{
				ghiAppendCharToBuffer(writeBuffer, '+');
			}
			else
			{
				assert((c / 16) < 16);
				hex[1] = GHI_DIGITS[c / 16];
				hex[2] = GHI_DIGITS[c % 16];
				ghiAppendDataToBuffer(writeBuffer, hex, 3);
			}
		}
	}
	else
	{
		GHITrySendResult result = ghiTrySendThenBuffer(connection, 
			state->data->data.string.string, state->data->data.string.len);
		if (result == GHITrySendError)
			return GHIPostingError;
		else
			return GHIPostingDone;
	}

	if (connection->encryptor.mEngine == GHTTPEncryptionEngine_None)
	{
		if (!ghiSendBufferedData(connection))
			return GHIPostingError;

		if (connection->sendBuffer.pos == connection->sendBuffer.len)
			ghiResetBuffer(&connection->sendBuffer);
		return GHIPostingDone;
	}
	else
	{
		return GHIPostingDone;
	}
}

static GHIPostingResult ghiPostXmlStateDoPosting
(
	GHIPostState * state,
	GHIConnection * connection
)
{
	GSXmlStreamWriter xml = state->data->data.xml.xml;
	char pad[3] = { '\0', '\0', '\0' };
	int padlen = 0;
	
	GS_ASSERT(state->pos >= 0);
	GS_ASSERT(connection->post != NULL);

	if (connection->post->useDIME)
	{
		padlen = 4-(gsXmlWriterGetDataLength(xml)%4);
		if (padlen == 4)
			padlen = 0;
	}

	if (connection->encryptor.mEngine != GHTTPEncryptionEngine_None &&
		connection->encryptor.mEncryptOnBuffer == GHTTPTrue)
	{
		GS_ASSERT(connection->encodeBuffer.len >= 0); 
		if (!ghiAppendDataToBuffer(&connection->encodeBuffer, gsXmlWriterGetData(xml), gsXmlWriterGetDataLength(xml)) ||
			!ghiAppendDataToBuffer(&connection->encodeBuffer, pad, padlen) ||
			!ghiEncryptDataToBuffer(&connection->sendBuffer, connection->encodeBuffer.data, connection->encodeBuffer.len)
			)
		{
			return GHIPostingError;
		}

		ghiResetBuffer(&connection->encodeBuffer);

		if (GHTTPFalse == ghiSendBufferedData(connection))
			return GHIPostingError;

		if (connection->sendBuffer.pos == connection->sendBuffer.len)
			ghiResetBuffer(&connection->sendBuffer);

		return GHIPostingDone;
	}
	else
	{
		GHITrySendResult result;

		result = ghiTrySendThenBuffer(connection, gsXmlWriterGetData(xml), gsXmlWriterGetDataLength(xml));
		if (result == GHITrySendError)
			return GHIPostingError;
		result = ghiTrySendThenBuffer(connection, pad, padlen);
		if (result == GHITrySendError)
			return GHIPostingError;
		return GHIPostingDone;
	}
}

static GHIPostingResult ghiPostFileDiskStateDoPosting
(
	GHIPostState * state,
	GHIConnection * connection
)
{
	char buffer[4096];
	int len;
	GHITrySendResult result;

	assert(state->pos >= 0);
	assert(state->pos < state->state.fileDisk.len);
	assert(state->pos == (int)ftell(state->state.fileDisk.file));

	do
	{
		len = (int)fread(buffer, 1, sizeof(buffer), state->state.fileDisk.file);
		if(len <= 0)
		{
			connection->completed = GHTTPTrue;
			connection->result = GHTTPFileReadFailed;
			return GHIPostingError;
		}

		state->pos += len;

		if(state->pos > state->state.fileDisk.len)
		{
			connection->completed = GHTTPTrue;
			connection->result = GHTTPFileReadFailed;
			return GHIPostingError;
		}

		result = ghiTrySendThenBuffer(connection, buffer, len);
		if(result == GHITrySendError)
			return GHIPostingError;

		if(state->pos == state->state.fileDisk.len)
		{
			if (connection->post->useDIME)
			{
				char pad[3] = { '\0', '\0', '\0' };
				int padlen = 4-state->state.fileDisk.len%4;
				if (padlen != 4 && padlen > 0)
				{
					if (GHITrySendError == ghiTrySendThenBuffer(connection, pad, padlen))
						return GHIPostingError;
				}
			}
			return GHIPostingDone;
		}
	}
	while(result == GHITrySendSent);

	return GHIPostingPosting;
}

static GHIPostingResult ghiPostFileMemoryStateDoPosting
(
	GHIPostState * state,
	GHIConnection * connection
)
{
	int rcode;
	int len;

	assert(state->pos >= 0);

	if(state->data->data.fileMemory.len == 0)
		return GHIPostingDone;

	assert(state->pos < state->data->data.fileMemory.len);

	if (connection->encryptor.mEngine == GHTTPEncryptionEngine_None)
	{
		do
		{
			len = (state->data->data.fileMemory.len - state->pos);
			rcode = ghiDoSend(connection, state->data->data.fileMemory.buffer + state->pos, len);
			if(gsiSocketIsError(rcode))
				return GHIPostingError;

			state->pos += rcode;

			if(state->data->data.fileMemory.len == state->pos)
			{
				if (connection->post->useDIME)
				{
					char pad[3] = { '\0', '\0', '\0' };
					int padlen = 4-state->data->data.fileMemory.len%4;
					if (padlen != 4 && padlen > 0)
					{
						if (GHITrySendError == ghiTrySendThenBuffer(connection, pad, padlen))
							return GHIPostingError;
					}
				}
				return GHIPostingDone;
			}
		}
		while(rcode);
		return GHIPostingPosting; 
	}
	else
	{
		GHITrySendResult result;
		do 
		{
			len = (state->data->data.fileMemory.len - state->pos);
			len = min(len, GS_SSL_MAX_CONTENTLENGTH);
			result = ghiTrySendThenBuffer(connection, state->data->data.fileMemory.buffer + state->pos, len);
			if (result == GHITrySendError)
				return GHIPostingError;
			
			state->pos += len;

			if(state->data->data.fileMemory.len == state->pos)
			{
				if (connection->post->useDIME)
				{
					char pad[3] = { '\0', '\0', '\0' };
					int padlen = 4-state->data->data.fileMemory.len%4;
					if (padlen != 4 && padlen > 0)
					{
						if (GHITrySendError == ghiTrySendThenBuffer(connection, pad, padlen))
							return GHIPostingError;
					}
				}
				return GHIPostingDone;
			}
		} 
		while(result == GHITrySendSent);
		return GHIPostingPosting;
	}
}

static GHIPostingResult ghiPostStateDoPosting
(
	GHIPostState * state,
	GHIConnection * connection,
	GHTTPBool first,
	GHTTPBool last
)
{
	int len = 0;
	GHITrySendResult result;

	if(state->pos == -1)
	{
		char buffer[2048];
		
		state->pos = 0;

		if(!connection->post->hasFiles && !connection->post->hasSoap)
		{
			assert(state->data->type == GHIString);

			if(first)
				sprintf(buffer, "%s=", state->data->name);
			else
				sprintf(buffer, "&%s=", state->data->name);
		}
		else
		{
			if(state->data->type == GHIString)
			{
				sprintf(buffer,
					"%s"
					"Content-Disposition: form-data; "
					"name=\"%s\"" CRLF
					CRLF,
					first?GHI_MULTIPART_BOUNDARY_FIRST:GHI_MULTIPART_BOUNDARY_NORMAL,
					state->data->name);
			}
			else if(state->data->type == GHIXmlData)
			{
				if (connection->post->useDIME)
				{
					int writePos = 0;
					int padBytes = 0;
					GHIDimeHeader header;

					header.mVersionAndFlags = GHI_DIME_VERSION;
					if (first)
						header.mVersionAndFlags |= GHI_DIMEFLAG_FIRSTRECORD;
					if (last)
						header.mVersionAndFlags |= GHI_DIMEFLAG_LASTRECORD;
					header.mTypeT = GHI_DIMETYPE_T_URI;
					header.mOptionsLength = 0;
					header.mIdLength = htons((short)strlen(GHI_DIME_SOAPID));
					header.mTypeLength = htons((short)strlen(GHI_DIME_SOAPTYPE));
					header.mDataLength = htonl(gsXmlWriterGetDataLength(state->data->data.xml.xml));

					memcpy(&buffer[writePos], &header, sizeof(GHIDimeHeader));
					writePos += sizeof(GHIDimeHeader);

					strcpy(&buffer[writePos], GHI_DIME_SOAPID);
					writePos += strlen(GHI_DIME_SOAPID);
					padBytes = (int)(4-strlen(GHI_DIME_SOAPID)%4);
					if (padBytes != 4)
					{
						while(padBytes-- > 0)
							buffer[writePos++] = '\0';
					}

					strcpy(&buffer[writePos], GHI_DIME_SOAPTYPE);
					writePos += strlen(GHI_DIME_SOAPTYPE);
					padBytes = (int)(4-strlen(GHI_DIME_SOAPTYPE)%4);
					if (padBytes != 4)
					{
						while(padBytes-- > 0)
							buffer[writePos++] = '\0';
					}

					len = writePos;
				}
				else
					buffer[0] = '\0';
			}
			else if((state->data->type == GHIFileDisk) || (state->data->type == GHIFileMemory))
			{
				const char * filename;
				const char * contentType;
				int filelen;

				if(state->data->type == GHIFileDisk)
				{
					filelen = state->state.fileDisk.len;
					filename = state->data->data.fileDisk.reportFilename;
					contentType = state->data->data.fileDisk.contentType;
				}
				else
				{
					filelen = state->data->data.fileMemory.len;
					filename = state->data->data.fileMemory.reportFilename;
					contentType = state->data->data.fileMemory.contentType;
				}

				if (connection->post->useDIME)
				{
					int writePos = 0;
					int padBytes = 0;
					GHIDimeHeader header;

					header.mVersionAndFlags = GHI_DIME_VERSION;
					if (first)
						header.mVersionAndFlags |= GHI_DIMEFLAG_FIRSTRECORD;
					if (last)
						header.mVersionAndFlags |= GHI_DIMEFLAG_LASTRECORD;
					header.mTypeT = GHI_DIMETYPE_T_MEDIA;
					header.mOptionsLength = 0;
					header.mIdLength = htons((short)strlen(state->data->name));
					header.mTypeLength = htons((short)strlen(contentType));
					header.mDataLength = htonl(filelen);

					memcpy(&buffer[writePos], &header, sizeof(GHIDimeHeader));
					writePos += sizeof(GHIDimeHeader);

					strcpy(&buffer[writePos], state->data->name);
					writePos += strlen(state->data->name);
					padBytes = (int)(4-strlen(state->data->name)%4);
					if (padBytes != 4)
					{
						while(padBytes-- > 0)
							buffer[writePos++] = '\0';
					}

					strcpy(&buffer[writePos], contentType);
					writePos += strlen(contentType);
					padBytes = (int)(4-strlen(contentType)%4);
					if (padBytes != 4)
					{
						while(padBytes-- > 0)
							buffer[writePos++] = '\0';
					}

					len = writePos;
				}
				else
				{
					sprintf(buffer,
						"%s"
						"Content-Disposition: form-data; "
						"name=\"%s\"; "
						"filename=\"%s\"" CRLF
						"Content-Type: %s" CRLF CRLF,
						first?GHI_MULTIPART_BOUNDARY_FIRST:GHI_MULTIPART_BOUNDARY_NORMAL,
						state->data->name,
						filename,
						contentType);
				}
			}
			else
			{
				assert(0);
			}
		}

		if (connection->encryptor.mEngine != GHTTPEncryptionEngine_None &&
			connection->encryptor.mEncryptOnBuffer == GHTTPTrue)
		{
			if (len == 0)
				len = (int)strlen(buffer);
			if (GHTTPFalse == ghiEncryptDataToBuffer(&connection->sendBuffer, buffer, len))
				return GHIPostingError;
			if (GHTTPFalse == ghiSendBufferedData(connection))
				return GHIPostingError;

			if (connection->sendBuffer.pos < connection->sendBuffer.len)
				return GHIPostingPosting;

			ghiResetBuffer(&connection->sendBuffer);
		}
		else
		{
			if (len == 0)
				len = (int)strlen(buffer);
			result = ghiTrySendThenBuffer(connection, buffer, len);
			if(result == GHITrySendError)
				return GHIPostingError;

			if(result == GHITrySendBuffered)
				return GHIPostingPosting;

			ghiResetBuffer(&connection->sendBuffer);
		}
	}

	if(state->data->type == GHIString)
		return ghiPostStringStateDoPosting(state, connection);

	if(state->data->type == GHIXmlData)
		return ghiPostXmlStateDoPosting(state, connection);

	if(state->data->type == GHIFileDisk)
		return ghiPostFileDiskStateDoPosting(state, connection);

	assert(state->data->type == GHIFileMemory);
	return ghiPostFileMemoryStateDoPosting(state, connection);
}

GHIPostingResult ghiPostDoPosting
(
	struct GHIConnection * connection
)
{
	GHIPostingResult postingResult;
	GHITrySendResult trySendResult;
	GHIPostingState * postingState;
	GHIPostState * postState;
	int len;

	assert(connection);
	assert(connection->post);
	assert(connection->postingState.states);
	assert(ArrayLength(connection->post->data) == ArrayLength(connection->postingState.states));
	assert(connection->postingState.index >= 0);
	assert(connection->postingState.index <= ArrayLength(connection->postingState.states));

	postingState = &connection->postingState;
	len = ArrayLength(postingState->states);

	if(connection->sendBuffer.pos < connection->sendBuffer.len)
	{
		if(!ghiSendBufferedData(connection))
			return GHIPostingError;

		if(connection->sendBuffer.pos < connection->sendBuffer.len)
			return GHIPostingPosting;

		ghiResetBuffer(&connection->sendBuffer);

		if (connection->postingState.waitPostContinue)
			return GHIPostingWaitForContinue;

		if(connection->postingState.index == len)
			return GHIPostingDone;
	}

	if (connection->postingState.waitPostContinue)
	{
		if (connection->post->hasFiles || connection->post->hasSoap)
		{
		  	GS_ASSERT(connection->encodeBuffer.len == 0);
			trySendResult = ghiTrySendThenBuffer(connection, CRLF, (int)strlen(CRLF));
			if(trySendResult == GHITrySendError)
				return GHIPostingError;
			else if (trySendResult == GHITrySendBuffered)
				return GHIPostingPosting;
			else
			{
				if (connection->postingState.waitPostContinue == GHTTPTrue)
					return GHIPostingWaitForContinue;
			}
		}
		else
		{
			connection->postingState.waitPostContinue = GHTTPFalse;
		}
	}

	while(postingState->index < len)
	{
		postState = (GHIPostState *)ArrayNth(postingState->states, postingState->index);
		assert(postState);

		postingResult = ghiPostStateDoPosting(postState, connection, 
			(postingState->index == 0)?GHTTPTrue:GHTTPFalse,
			(postingState->index == (ArrayLength(postingState->states)-1))?GHTTPTrue:GHTTPFalse);

		if(postingResult == GHIPostingError)
		{
			assert(connection->completed && connection->result);

			return GHIPostingError;
		}

		if(postingResult == GHIPostingPosting)
			return GHIPostingPosting;

		postingState->index++;
	}

	if (connection->encryptor.mEngine != GHTTPEncryptionEngine_None)
	{
		if (connection->encodeBuffer.len > 0)
		{
			GS_ASSERT(connection->encodeBuffer.pos == 0); 
			if (GHTTPFalse == ghiEncryptDataToBuffer(&connection->sendBuffer, 
						connection->encodeBuffer.data, connection->encodeBuffer.len))
			{
				return GHIPostingError;
			}
			ghiResetBuffer(&connection->encodeBuffer);
		}
	}

	if(connection->post->hasFiles && !connection->post->useDIME)
	{
		GS_ASSERT(!connection->post->hasSoap);

		trySendResult = ghiTrySendThenBuffer(connection, GHI_MULTIPART_BOUNDARY_END, (int)strlen(GHI_MULTIPART_BOUNDARY_END));
		if(trySendResult == GHITrySendError)
			return GHIPostingError;
	}

	if(connection->sendBuffer.pos < connection->sendBuffer.len)
		return GHIPostingPosting;

	return GHIPostingDone;
}
