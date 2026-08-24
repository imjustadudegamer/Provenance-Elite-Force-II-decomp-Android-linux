/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */

#include "ghttpBuffer.h"
#include "ghttpConnection.h"
#include "ghttpMain.h"
#include "ghttpCommon.h"
#include "../common/gsCrypt.h"
#include "../common/gsSSL.h"


GHTTPBool ghiResizeBuffer
(
	GHIBuffer * buffer,
	int sizeIncrement
)
{
	char * tempPtr;
	int newSize;

	assert(buffer);
	assert(sizeIncrement > 0);
	assert(buffer->fixed == GHTTPFalse); 

	if(!buffer)
		return GHTTPFalse;
	if(sizeIncrement <= 0)
		return GHTTPFalse;

	newSize = (buffer->size + sizeIncrement);
	tempPtr = (char *)gsirealloc(buffer->data, (unsigned int)newSize);
	if(!tempPtr)
		return GHTTPFalse;

	buffer->data = tempPtr;
	buffer->size = newSize;

	return GHTTPTrue;
}

GHTTPBool ghiInitBuffer
(
	struct GHIConnection * connection,
	GHIBuffer * buffer,
	int initialSize,
	int sizeIncrement
)
{
	GHTTPBool bResult;

	assert(connection);
	assert(buffer);
	assert(initialSize > 0);
	assert(sizeIncrement > 0);

	if(!connection)
		return GHTTPFalse;
	if(!buffer)
		return GHTTPFalse;
	if(initialSize <= 0)
		return GHTTPFalse;
	if(sizeIncrement <= 0)
		return GHTTPFalse;

	buffer->connection = connection;
	buffer->data = NULL;
	buffer->size = 0;
	buffer->len = 0;
	buffer->pos = 0;
	buffer->sizeIncrement = sizeIncrement;
	buffer->fixed = GHTTPFalse;
	buffer->dontFree = GHTTPFalse;
	buffer->readOnly = GHTTPFalse;

	bResult = ghiResizeBuffer(buffer, initialSize);
	if(!bResult)
		return GHTTPFalse;

	*buffer->data = '\0';

	return GHTTPTrue;
}

GHTTPBool ghiInitFixedBuffer
(
	struct GHIConnection * connection,
	GHIBuffer * buffer,
	char * userBuffer,
	int size
)
{
	assert(connection);
	assert(buffer);
	assert(userBuffer);
	assert(size > 0);

	if(!connection)
		return GHTTPFalse;
	if(!buffer)
		return GHTTPFalse;
	if(!userBuffer)
		return GHTTPFalse;
	if(size <= 0)
		return GHTTPFalse;

	buffer->connection = connection;
	buffer->data = userBuffer;
	buffer->size = size;
	buffer->len = 0;
	buffer->pos = 0;
	buffer->sizeIncrement = 0;
	buffer->fixed = GHTTPTrue;
	buffer->dontFree = GHTTPTrue;
	buffer->readOnly = GHTTPFalse;

	*buffer->data = '\0';

	return GHTTPTrue;
}

GHTTPBool ghiInitReadOnlyBuffer
(
	struct GHIConnection * connection,  
	GHIBuffer * buffer,  
	const char * userBuffer,   
	int size             
)
{
	assert(connection);
	assert(buffer);
	assert(userBuffer);
	assert(size > 0);

	if(!connection)
		return GHTTPFalse;
	if(!buffer)
		return GHTTPFalse;
	if(!userBuffer)
		return GHTTPFalse;
	if(size <= 0)
		return GHTTPFalse;

	buffer->connection = connection;
	buffer->data = (char*)userBuffer; 
	buffer->size = size;
	buffer->pos = 0;
	buffer->sizeIncrement = 0;
	buffer->fixed = GHTTPTrue;
	buffer->dontFree = GHTTPTrue;
	buffer->readOnly = GHTTPTrue;

	buffer->len = size; 

	return GHTTPTrue;
}

void ghiFreeBuffer
(
	GHIBuffer * buffer
)
{
	assert(buffer);

	if(!buffer)
		return;
	if(!buffer->data)
		return;

	if(!buffer->dontFree)
		gsifree(buffer->data);
	memset(buffer, 0, sizeof(GHIBuffer));
}

GHTTPBool ghiAppendDataToBuffer
(
	GHIBuffer * buffer,
	const char * data,
	int dataLen
)
{
	GHTTPBool bResult;
	int newLen;

	assert(buffer);
	assert(data);
	assert(dataLen >= 0);

	if(!buffer)
		return GHTTPFalse;
	if(!data)
		return GHTTPFalse;
	if(dataLen < 0)
		return GHTTPFalse;
	if (buffer->readOnly)
		return GHTTPFalse;

	if(dataLen == 0)
		dataLen = (int)strlen(data);

	newLen = (buffer->len + dataLen);

	while(newLen >= buffer->size)
	{
		if(buffer->fixed)
		{
			buffer->connection->completed = GHTTPTrue;
			buffer->connection->result = GHTTPBufferOverflow;
			return GHTTPFalse;
		}

		bResult = ghiResizeBuffer(buffer, buffer->sizeIncrement);
		if(!bResult)
		{
			buffer->connection->completed = GHTTPTrue;
			buffer->connection->result = GHTTPOutOfMemory;
			return GHTTPFalse;
		}
	}

	memcpy(buffer->data + buffer->len, data, (unsigned int)dataLen);
	buffer->len = newLen;
	buffer->data[buffer->len] = '\0';
	return GHTTPTrue;
}


GHTTPBool ghiEncryptDataToBuffer
(
	GHIBuffer * buffer,
	const char * data,
	int dataLen
)
{
	GHIEncryptionResult result;
	int bufSpace = 0;
	int pos = 0;

	assert(buffer);
	assert(data);
	assert(dataLen >= 0);

	if(!buffer)
		return GHTTPFalse;
	if(!data)
		return GHTTPFalse;
	if(dataLen < 0)
		return GHTTPFalse;
	if (buffer->readOnly)
		return GHTTPFalse;

	if (buffer->connection->encryptor.mEngine == GHTTPEncryptionEngine_None ||
		buffer->connection->encryptor.mSessionEstablished == GHTTPFalse)
	{
		return ghiAppendDataToBuffer(buffer, data, dataLen);
	}

	if(dataLen == 0)
		dataLen = (int)strlen(data);
	if (dataLen == 0)
		return GHTTPTrue; 
	bufSpace = buffer->size - buffer->len;

	do 
	{	
		int fragmentLen = min(dataLen, GS_SSL_MAX_CONTENTLENGTH);
		
		result = buffer->connection->encryptor.mEncryptFunc(buffer->connection, &buffer->connection->encryptor, 
													&data[pos], dataLen,
													&buffer->data[buffer->len], &bufSpace);
		if (result == GHIEncryptionResult_BufferTooSmall)
		{
			if (ghiResizeBuffer(buffer, buffer->sizeIncrement) == GHTTPFalse)
				return GHTTPFalse;
			bufSpace = buffer->size - buffer->len;
		}
		else if (result == GHIEncryptionResult_Success)
		{
			pos += fragmentLen;
			buffer->len = buffer->size - bufSpace;
		}
		else
		{
			gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_WarmError,
				"ghiEncryptDataToBuffer encountered unhandled return code: %d\r\n", result);
			return GHTTPFalse;
		}
	} while(pos < dataLen);

	return GHTTPTrue;
}

GHTTPBool ghiAppendHeaderToBuffer
(
	GHIBuffer * buffer,
	const char * name,
	const char * value
)
{
	if(!ghiAppendDataToBuffer(buffer, name, 0))
		return GHTTPFalse;
	if(!ghiAppendDataToBuffer(buffer, ": ", 2))
		return GHTTPFalse;
	if(!ghiAppendDataToBuffer(buffer, value, 0))
		return GHTTPFalse;
	if(!ghiAppendDataToBuffer(buffer, CRLF, 2))
		return GHTTPFalse;

	return GHTTPTrue;
}

GHTTPBool ghiAppendCharToBuffer
(
	GHIBuffer * buffer,
	int c
)
{
	GHTTPBool bResult;
	assert(buffer);

	if(!buffer)
		return GHTTPFalse;
	if (buffer->readOnly)
		return GHTTPFalse;

	if((buffer->len + 1) >= buffer->size)
	{
		if(buffer->fixed)
		{
			buffer->connection->completed = GHTTPTrue;
			buffer->connection->result = GHTTPBufferOverflow;
			return GHTTPFalse;
		}

		bResult = ghiResizeBuffer(buffer, buffer->sizeIncrement);
		if(!bResult)
		{
			buffer->connection->completed = GHTTPTrue;
			buffer->connection->result = GHTTPOutOfMemory;
			return GHTTPFalse;
		}
	}

	buffer->data[buffer->len] = (char)(c & 0xFF);
	buffer->len++;
	buffer->data[buffer->len] = '\0';

	return GHTTPTrue;
}

GHTTPBool ghiAppendIntToBuffer
(
	GHIBuffer * buffer,
	int i
)
{
	char intValue[16];

	sprintf(intValue, "%d", i);

	return ghiAppendDataToBuffer(buffer, intValue, 0);
}

void ghiResetBuffer
(
	GHIBuffer * buffer
)
{
	assert(buffer);

	buffer->len = 0;
	buffer->pos = 0;

	if (!buffer->readOnly)
		*buffer->data = '\0';
}

GHTTPBool ghiSendBufferedData
(
	struct GHIConnection * connection
)
{
	int rcode;
	int writeFlag;
	int exceptFlag;
	char * data;
	int len;

	do
	{
		rcode = GSISocketSelect(connection->socket, NULL, &writeFlag, &exceptFlag);
		if((gsiSocketIsError(rcode)) || ((rcode == 1) && exceptFlag))
		{
			connection->completed = GHTTPTrue;
			connection->result = GHTTPSocketFailed;
			if(gsiSocketIsError(rcode))
				connection->socketError = GOAGetLastError(connection->socket);
			else
				connection->socketError = 0;
			return GHTTPFalse;
		}
		if((rcode < 1) || !writeFlag)
		{
			return GHTTPTrue;
		}

		data = (connection->sendBuffer.data + connection->sendBuffer.pos);
		len = (connection->sendBuffer.len - connection->sendBuffer.pos);

		rcode = ghiDoSend(connection, data, len);
		if(gsiSocketIsError(rcode))
			return GHTTPFalse;

		connection->sendBuffer.pos += rcode;
	}
	while(connection->sendBuffer.pos < connection->sendBuffer.len);

	return GHTTPTrue;
}


GHTTPBool ghiReadDataFromBuffer
(
	GHIBuffer * bufferIn,    
	char        bufferOut[], 
	int *       len          
)
{
	int bytesAvailable = 0;
	int bytesToCopy    = 0;
	
	
	assert(bufferIn != NULL);
	assert(len != NULL);
	if (*len == 0)
		return GHTTPFalse;

	bytesAvailable = (int)bufferIn->len - bufferIn->pos;
	if (bytesAvailable <= 0)
		return GHTTPFalse;

	bytesToCopy = min(*len-1, bytesAvailable);

	memcpy(bufferOut, bufferIn->data + bufferIn->pos, (size_t)bytesToCopy);
	bufferOut[bytesToCopy] = '\0';
	*len = bytesToCopy;

	bufferIn->pos += bytesToCopy;
	return GHTTPTrue;
}


GHTTPBool ghiReadDataFromBufferFixed
(
	GHIBuffer * bufferIn,    
	char        bufferOut[], 
	int         bytesToCopy  
)
{
	assert(bufferIn != NULL);
	if (bytesToCopy == 0)
		return GHTTPTrue;

	if (bufferIn->len < bytesToCopy)
		return GHTTPFalse;

	memcpy(bufferOut, bufferIn->data + bufferIn->pos, (size_t)bytesToCopy);

	bufferIn->pos += bytesToCopy;
	return GHTTPTrue;
}
