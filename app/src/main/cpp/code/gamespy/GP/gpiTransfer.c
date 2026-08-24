/*
gpiTransfer.c
GameSpy Presence SDK 
Dan "Mr. Pants" Schoenblum

Copyright 1999-2007 GameSpy Industries, Inc

devsupport@gamespy.com

***********************************************************************
Please see the GameSpy Presence SDK documentation for more information
**********************************************************************/

#include <stdlib.h>
#ifdef _WIN32
#include <sys/stat.h>
#endif
#include <string.h>
#include "gpi.h"

#define GPI_TRANSFER_VERSION      1
#define GPI_PEER_TIMEOUT_TIME     (1 * 60000)
#define GPI_KEEPALIVE_TIME        (4 * 60000)

#define GPI_DATA_SIZE             (1 * 1024)

#ifndef NOFILE
static void gpiTransferFree(void * elem)
{
	GPITransfer * transfer = (GPITransfer *)elem;

	if(transfer->message)
		freeclear(transfer->message);

	if(transfer->baseDirectory)
		gsifree(transfer->baseDirectory);

	if(transfer->files)
	{
		ArrayFree(transfer->files);
		transfer->files = NULL;
	}
}

static int gpiTransferCompare(const void * elem1, const void * elem2)
{
	GPITransfer * transfer1 = (GPITransfer *)elem1;
	GPITransfer * transfer2 = (GPITransfer *)elem2;

	return (transfer1->localID - transfer2->localID);
}

static void gpiFreeFile(void * elem)
{
	GPIFile * file = (GPIFile *)elem;
	gsifree(file->path);
	gsifree(file->name);

#ifdef GSI_UNICODE
	gsifree(file->path_W);
	gsifree(file->name_W);
#endif
}

static GPResult
gpiFinishTransferMessage(
  GPConnection * connection,
  GPITransfer * transfer,
  const char * message,
  int len
)
{
	GPResult result = gpiPeerFinishTransferMessage(connection, transfer->peer, message, len);

	transfer->lastSend = current_time();

	return result;
}

GPResult gpiInitTransfers(
  GPConnection * connection
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;

	iconnection->transfers = ArrayNew(sizeof(GPITransfer), 0, gpiTransferFree);
	if(!iconnection->transfers)
		Error(connection, GP_MEMORY_ERROR, "Out of memory.");

	return GP_NO_ERROR;
}

void gpiCleanupTransfers(
  GPConnection * connection
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;

	if(iconnection->transfers)
	{
		ArrayFree(iconnection->transfers);
		iconnection->transfers = NULL;
	}
}

static GPResult gpiNewTransfer
(
  GPConnection * connection,
  GPITransfer ** transfer,
  GPProfile profile,
  GPIBool sender
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	GPITransfer transferTemp;

	memset(&transferTemp, 0, sizeof(GPITransfer));
	transferTemp.files = ArrayNew(sizeof(GPIFile), 0, gpiFreeFile);
	if(!transferTemp.files)
		Error(connection, GP_MEMORY_ERROR, "Out of memory.");
	transferTemp.localID = iconnection->nextTransferID++;
	transferTemp.sender = sender;
	transferTemp.profile = profile;
	transferTemp.throttle = -1;
	transferTemp.currentFile = -1;

	ArrayAppend(iconnection->transfers, &transferTemp);

	*transfer = (GPITransfer *)ArrayNth(iconnection->transfers, ArrayLength(iconnection->transfers) - 1);

	return GP_NO_ERROR;
}

GPResult gpiNewSenderTransfer
(
  GPConnection * connection,
  GPITransfer ** transfer,
  GPProfile profile
)
{
	GPITransfer * pTransfer;
	GPIConnection * iconnection = (GPIConnection*)*connection;
	unsigned long now;

	CHECK_RESULT(gpiNewTransfer(connection, transfer, profile, GPITrue));

	now = current_time();

	pTransfer = *transfer;
	pTransfer->state = GPITransferPinging;
	pTransfer->transferID.profileid = iconnection->profileid;
	pTransfer->transferID.count = pTransfer->localID;
	pTransfer->transferID.time = (unsigned int)now;
	pTransfer->lastSend = now;

	return GP_NO_ERROR;
}

static GPResult gpiNewReceiverTransfer
(
  GPConnection * connection,
  GPITransfer ** transfer,
  GPProfile profile,
  GPITransferID * transferID
)
{
#ifdef WIN32
	char buffer[FILENAME_MAX];
#endif
	GPITransfer * pTransfer;

	CHECK_RESULT(gpiNewTransfer(connection, transfer, profile, GPIFalse));

	pTransfer = *transfer;
	pTransfer->state = GPITransferWaiting;
	memcpy(&pTransfer->transferID, transferID, sizeof(GPITransferID));
#ifdef WIN32
	if(GetTempPathA(sizeof(buffer), buffer) != 0)
		pTransfer->baseDirectory = goastrdup(buffer);
#endif

	return GP_NO_ERROR;
}

void gpiFreeTransfer
(
  GPConnection * connection,
  GPITransfer * transfer
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	int pos;

	pos = ArraySearch(iconnection->transfers, transfer, gpiTransferCompare, 0, 0);
	assert(pos != NOT_FOUND);
	if(pos == NOT_FOUND)
		return;

	ArrayDeleteAt(iconnection->transfers, pos);
}

void gpiCancelTransfer
(
  GPConnection * connection,
  GPITransfer * transfer
)
{
	if(transfer->peer)
	{
		if(gpiPeerStartTransferMessage(connection, transfer->peer, GPI_BM_FILE_TRANSFER_CANCEL, (GPITransferID_st)&transfer->transferID) == GP_NO_ERROR)
		{
			gpiFinishTransferMessage(connection, transfer, NULL, 0);
		}
	}
}

void gpiTransferError
(
  GPConnection * connection,
  const GPITransfer * transfer
)
{
	GPTransferCallbackArg * arg;
	GPIConnection * iconnection = (GPIConnection*)*connection;

	arg = (GPTransferCallbackArg *)gsimalloc(sizeof(GPTransferCallbackArg));
	if(arg)
	{
		memset(arg, 0, sizeof(GPTransferCallbackArg));
		arg->transfer = transfer->localID;
		arg->type = GP_TRANSFER_ERROR;
		gpiAddCallback(connection, iconnection->callbacks[GPI_TRANSFER_CALLBACK], arg, NULL, GPI_ADD_TRANSFER_CALLBACK);
	}
}

GPIFile * gpiAddFileToTransfer
(
  GPITransfer * transfer,
  const char * path,
  const char * name
)
{
	GPIFile file;
	char * str;

	assert(name && name[0]);

	memset(&file, 0, sizeof(GPIFile));

	if(path)
	{
		file.path = goastrdup(path);
		if(!file.path)
			return NULL;
	}

	file.name = goastrdup(name);
	if(!file.name)
	{
		gsifree(file.path);
		return NULL;
	}

	while((str = strchr(file.name, '\\')) != NULL)
		*str = '/';

	if(name[strlen(name) - 1] == '/')
		file.flags = GPI_FILE_DIRECTORY;

	file.size = -1;

	ArrayAppend(transfer->files, &file);

#ifdef GSI_UNICODE
	file.name_W = UTF8ToUCS2StringAlloc(file.name);
	file.path_W = UTF8ToUCS2StringAlloc(file.path);
#endif

	return (GPIFile *)ArrayNth(transfer->files, ArrayLength(transfer->files) - 1);
}

static GPResult gpiSendTransferRequest
(
  GPConnection * connection,
  GPITransfer * transfer
)
{
	char buffer[32];
	GPIFile * file;
	int i;
	int num;

	num = ArrayLength(transfer->files);

	CHECK_RESULT(gpiPeerStartTransferMessage(connection, transfer->peer, GPI_BM_FILE_SEND_REQUEST, (GPITransferID_st)&transfer->transferID));

	sprintf(buffer, "\\version\\%d\\num\\%d", GPI_TRANSFER_VERSION, num);
	CHECK_RESULT(gpiSendOrBufferString(connection, transfer->peer, buffer));
	for(i = 0 ; i < num ; i++)
	{
		file = (GPIFile *)ArrayNth(transfer->files, i);

		sprintf(buffer, "\\name%d\\", i);
		CHECK_RESULT(gpiSendOrBufferString(connection, transfer->peer, buffer));
		CHECK_RESULT(gpiSendOrBufferString(connection, transfer->peer, file->name));

		sprintf(buffer, "\\size%d\\", i);
		CHECK_RESULT(gpiSendOrBufferString(connection, transfer->peer, buffer));
		CHECK_RESULT(gpiSendOrBufferInt(connection, transfer->peer, file->size));

		sprintf(buffer, "\\mtime%d\\", i);
		CHECK_RESULT(gpiSendOrBufferString(connection, transfer->peer, buffer));
		CHECK_RESULT(gpiSendOrBufferUInt(connection, transfer->peer, file->modTime));
	}

	CHECK_RESULT(gpiFinishTransferMessage(connection, transfer, transfer->message, -1));

	return GP_NO_ERROR;
}

static GPITransfer * gpiFindTransferByTransferID
(
  GPConnection * connection,
  GPITransferID * transferID
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	int i;
	int num;
	GPITransfer * transfer;

	num = ArrayLength(iconnection->transfers);
	for(i = 0 ; i < num ; i++)
	{
		transfer = (GPITransfer *)ArrayNth(iconnection->transfers, i);
		if(memcmp(&transfer->transferID, transferID, sizeof(GPITransferID)) == 0)
			return transfer;
	}

	return NULL;
}

GPITransfer * gpiFindTransferByLocalID
(
  GPConnection * connection,
  int localID
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	int i;
	int num;
	GPITransfer * transfer;

	num = ArrayLength(iconnection->transfers);
	for(i = 0 ; i < num ; i++)
	{
		transfer = (GPITransfer *)ArrayNth(iconnection->transfers, i);
		if(transfer->localID == localID)
			return transfer;
	}

	return NULL;
}

int gpiGetTransferLocalIDByIndex
(
  GPConnection * connection,
  int index
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	GPITransfer * transfer;
	int num;

	num = ArrayLength(iconnection->transfers);
	assert(index >= 0);
	assert(index < num);
	if((index < 0) || (index >= num))
		return -1;

	transfer = (GPITransfer *)ArrayNth(iconnection->transfers, index);
	assert(transfer);
	if(!transfer)
		return -1;

	return transfer->localID;
}

void gpiSkipFile
(
  GPConnection * connection,
  GPITransfer * transfer,
  int file,
  int reason
)
{
	char buffer[32];

	if(gpiPeerStartTransferMessage(connection, transfer->peer, GP_FILE_SKIP, (GPITransferID_st)&transfer->transferID) != GP_NO_ERROR)
		return;

	sprintf(buffer, "\\file\\%d\\reason\\%d", file, reason);
	gpiSendOrBufferString(connection, transfer->peer, buffer);
	gpiFinishTransferMessage(connection, transfer, NULL, 0);
}

void gpiSkipCurrentFile
(
  GPConnection * connection,
  GPITransfer * transfer,
  int reason
)
{
	gpiSkipFile(connection, transfer, transfer->currentFile, reason);

	transfer->currentFile++;
}

static GPIBool gpiHandleSendRequest
(
  GPConnection * connection,
  GPIPeer * peer,
  GPITransferID * transferID,
  const char * headers,
  const char * buffer,
  int bufferLen
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	GPITransfer * transfer;
	GPIFile * file;
	GPTransferCallbackArg * arg;
	char key[16];
	char intValue[16];
	int version;
	int numFiles;
	char name[FILENAME_MAX];
	int size;
	unsigned int mtime;
	int i;
	size_t len;
	int totalSize = 0;

	if(!iconnection->callbacks[GPI_TRANSFER_CALLBACK].callback)
		return GPIFalse;

	if(!gpiValueForKey(headers, "\\version\\", intValue, sizeof(intValue)))
		return GPIFalse;
	version = atoi(intValue);
	if(version < 1)
		return GPIFalse;

	if(!gpiValueForKey(headers, "\\num\\", intValue, sizeof(intValue)))
		return GPIFalse;
	numFiles = atoi(intValue);
	if(numFiles < 1)
		return GPIFalse;

	if(gpiNewReceiverTransfer(connection, &transfer, peer->profile, transferID) != GP_NO_ERROR)
		return GPIFalse;

	transfer->peer = peer;

	for(i = 0 ; i < numFiles ; i++)
	{
		sprintf(key, "\\name%d\\", i);
		if(!gpiValueForKey(headers, key, name, sizeof(name)))
		{
			gpiFreeTransfer(connection, transfer);
			return GPIFalse;
		}
		len = strlen(name);
		if(strstr(name, "//") || strstr(name, "./") || (name[len - 1] == '.') || (name[0] == '/') || (strcspn(name, ":*?\"<>|\n") != len))
		{
			gpiFreeTransfer(connection, transfer);
			return GPIFalse;
		}

		sprintf(key, "\\size%d\\", i);
		if(!gpiValueForKey(headers, key, intValue, sizeof(intValue)))
		{
			gpiFreeTransfer(connection, transfer);
			return GPIFalse;
		}
		size = atoi(intValue);
		if(size < 0)
		{
			gpiFreeTransfer(connection, transfer);
			return GPIFalse;
		}
		totalSize += size;

		sprintf(key, "\\mtime%d\\", i);
		if(!gpiValueForKey(headers, key, intValue, sizeof(intValue)))
		{
			gpiFreeTransfer(connection, transfer);
			return GPIFalse;
		}
		mtime = (unsigned int)strtoul(intValue, NULL, 10);

		file = gpiAddFileToTransfer(transfer, NULL, name);
		if(!file)
		{
			gpiFreeTransfer(connection, transfer);
			return GPIFalse;
		}
		file->size = size;
		file->modTime = mtime;
	}

	arg = (GPTransferCallbackArg *)gsimalloc(sizeof(GPTransferCallbackArg));
	if(!arg)
	{
		gpiFreeTransfer(connection, transfer);
		return GPIFalse;
	}
	memset(arg, 0, sizeof(GPTransferCallbackArg));
	arg->transfer = transfer->localID;
	arg->type = GP_TRANSFER_SEND_REQUEST;
	arg->num = numFiles;
#ifndef GSI_UNICODE
	arg->message = goastrdup(buffer);
#else
	arg->message = UTF8ToUCS2StringAlloc(buffer);
#endif
	{
		GPResult aResult = gpiAddCallback(connection, iconnection->callbacks[GPI_TRANSFER_CALLBACK], arg, NULL, GPI_ADD_TRANSFER_CALLBACK);
		if (aResult != GP_NO_ERROR)
			return GPIFalse;
	}

	transfer->totalSize = totalSize;

	return GPITrue;
	
	GSI_UNUSED(bufferLen);
}

static GPIBool gpiHandleSendReply
(
  GPConnection * connection,
  GPITransfer * transfer,
  const char * headers,
  const char * buffer,
  int bufferLen
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	GPTransferCallbackArg * arg;
	char intValue[16];
	int version;
	int result;

	if(!transfer->sender)
		return GPIFalse;

	if(!gpiValueForKey(headers, "\\version\\", intValue, sizeof(intValue)))
		return GPIFalse;
	version = atoi(intValue);
	if(version < 1)
		return GPIFalse;

	if(!gpiValueForKey(headers, "\\result\\", intValue, sizeof(intValue)))
		return GPIFalse;
	result = atoi(intValue);

	arg = (GPTransferCallbackArg *)gsimalloc(sizeof(GPTransferCallbackArg));
	if(arg)
	{
		memset(arg, 0, sizeof(GPTransferCallbackArg));
		arg->transfer = transfer->localID;
		if(result == GPI_ACCEPTED)
			arg->type = GP_TRANSFER_ACCEPTED;
		else if(result == GPI_REJECTED)
			arg->type = GP_TRANSFER_REJECTED;
		else
			arg->type = GP_TRANSFER_NOT_ACCEPTING;

#ifndef GSI_UNICODE
		arg->message = goastrdup(buffer);
#else
		arg->message = UTF8ToUCS2StringAlloc(buffer);
#endif
		gpiAddCallback(connection, iconnection->callbacks[GPI_TRANSFER_CALLBACK], arg, NULL, GPI_ADD_TRANSFER_CALLBACK);
	}

	if(result == GPI_ACCEPTED)
	{
		transfer->state = GPITransferTransferring;
		transfer->currentFile = 0;
	}

	return GPITrue;

	GSI_UNUSED(bufferLen);
}

static GPIBool gpiHandleBegin
(
  GPConnection * connection,
  GPITransfer * transfer,
  const char * headers
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	GPTransferCallbackArg * arg;
	GPIFile * file;
	char intValue[16];
	int fileIndex;
	int size;
	unsigned int mtime;
	char buffer[FILENAME_MAX];
	int count;

	if(transfer->sender)
		return GPIFalse;

	if(!gpiValueForKey(headers, "\\file\\", intValue, sizeof(intValue)))
		return GPIFalse;
	fileIndex = atoi(intValue);
	if((fileIndex < 0) || (fileIndex >= ArrayLength(transfer->files)))
		return GPIFalse;
	if(fileIndex != transfer->currentFile)
		return GPIFalse;
	file = (GPIFile *)ArrayNth(transfer->files, fileIndex);

	if(file->flags & GPI_FILE_DIRECTORY)
		return GPIFalse;

	if(!gpiValueForKey(headers, "\\size\\", intValue, sizeof(intValue)))
		return GPIFalse;
	size = atoi(intValue);
	if(size < 0)
		return GPIFalse;

	transfer->totalSize -= file->size;
	transfer->totalSize += size;

	if(!gpiValueForKey(headers, "\\mtime\\", intValue, sizeof(intValue)))
		return GPIFalse;
	mtime = (unsigned int)strtoul(intValue, NULL, 10);

	MD5Init(&file->md5);
	file->modTime = mtime;
	file->size = size;

	count = 0;
	do
	{
		sprintf(buffer, "%sgpt_%d_%d_%d.gpt", transfer->baseDirectory, transfer->localID, fileIndex, rand());
		file->file = fopen(buffer, "wb");
		count++;
	}
	while(!file->file && (count < 5));

	if(file->file)
	{
		file->path = goastrdup(buffer);
		if(!file->path)
			return GPIFalse;

#ifdef GSI_UNICODE
		file->path_W = UTF8ToUCS2StringAlloc(file->path);
#endif
	}

	arg = (GPTransferCallbackArg *)gsimalloc(sizeof(GPTransferCallbackArg));
	if(arg)
	{
		memset(arg, 0, sizeof(GPTransferCallbackArg));
		arg->transfer = transfer->localID;
		arg->index = fileIndex;
		if(file->file)
		{
			arg->type = GP_FILE_BEGIN;
		}
		else
		{
			arg->type = GP_FILE_FAILED;
			arg->num = GP_FILE_WRITE_ERROR;
		}
		gpiAddCallback(connection, iconnection->callbacks[GPI_TRANSFER_CALLBACK], arg, NULL, GPI_ADD_TRANSFER_CALLBACK);
	}

	if(!file->file)
	{
		gpiSkipCurrentFile(connection, transfer, GPI_SKIP_WRITE_ERROR);
		file->flags |= GPI_FILE_FAILED;
		file->reason = GP_FILE_WRITE_ERROR;
	}

	return GPITrue;
}

static GPIBool gpiHandleEnd
(
  GPConnection * connection,
  GPITransfer * transfer,
  const char * headers
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	GPTransferCallbackArg * arg;
	GPIFile * file;
	GPIBool md5Failed = GPITrue;
	unsigned char rawMD5[16];
	char localMD5[33];
	char remoteMD5[33];
	char intValue[16];
	int fileIndex;

	if(transfer->currentFile == -1)
		return GPIFalse;

	if(!gpiValueForKey(headers, "\\file\\", intValue, sizeof(intValue)))
		return GPIFalse;
	fileIndex = atoi(intValue);
	if((fileIndex < 0) || (fileIndex >= ArrayLength(transfer->files)))
		return GPIFalse;
	if(fileIndex != transfer->currentFile)
		return GPITrue;

	file = (GPIFile *)ArrayNth(transfer->files, transfer->currentFile);

	if(transfer->sender)
	{
#ifdef GPI_CONFIRM_FILES
		assert(file->flags & GPI_FILE_CONFIRMING);

		arg = (GPTransferCallbackArg *)gsimalloc(sizeof(GPTransferCallbackArg));
		if(arg)
		{
			memset(arg, 0, sizeof(GPTransferCallbackArg));
			arg->transfer = transfer->localID;
			arg->index = transfer->currentFile;
			arg->type = GP_FILE_END;
			gpiAddCallback(connection, iconnection->callbacks[GPI_TRANSFER_CALLBACK], arg, NULL, GPI_ADD_TRANSFER_CALLBACK);
		}

		file->flags &= ~GPI_FILE_CONFIRMING;
		file->flags |= GPI_FILE_COMPLETED;
		transfer->currentFile++;
#endif
		return GPITrue;
	}

	if(file->flags & GPI_FILE_DIRECTORY)
	{
		file->flags |= GPI_FILE_COMPLETED;
	}
	else
	{
		assert(file->file);
		if(!file->file)
			return GPIFalse;

		if(!gpiValueForKey(headers, "\\md5\\", remoteMD5, sizeof(remoteMD5)))
			return GPIFalse;

		MD5Final(rawMD5, &file->md5);
		MD5Print(rawMD5, localMD5);

		md5Failed = (memcmp(localMD5, remoteMD5, 32) != 0) ? GPITrue:GPIFalse;

		if(md5Failed)
		{
			file->flags |= GPI_FILE_FAILED;
			file->reason = GP_FILE_DATA_ERROR;
		}
		else
			file->flags |= GPI_FILE_COMPLETED;

		fclose(file->file);
		file->file = NULL;

		if(md5Failed)
			remove(file->path);

#ifdef GPI_CONFIRM_FILES
		if(gpiPeerStartTransferMessage(connection, transfer->peer, GPI_BM_FILE_END, (GPITransferID_st)&transfer->transferID) != GP_NO_ERROR)
			return GPIFalse;
		gpiFinishTransferMessage(connection, transfer, NULL, 0);
#endif
	}

	arg = (GPTransferCallbackArg *)gsimalloc(sizeof(GPTransferCallbackArg));
	if(arg)
	{
		memset(arg, 0, sizeof(GPTransferCallbackArg));
		arg->transfer = transfer->localID;
		arg->index = transfer->currentFile;
		if(file->flags & GPI_FILE_DIRECTORY)
		{
			arg->type = GP_FILE_DIRECTORY;
		}
		else if(md5Failed)
		{
			arg->type = GP_FILE_FAILED;
			arg->num = GP_FILE_DATA_ERROR;
		}
		else
		{
			arg->type = GP_FILE_END;
		}
		gpiAddCallback(connection, iconnection->callbacks[GPI_TRANSFER_CALLBACK], arg, NULL, GPI_ADD_TRANSFER_CALLBACK);
	}

	transfer->currentFile++;

	if(transfer->currentFile == ArrayLength(transfer->files))
	{
		transfer->state = GPITransferComplete;

		arg = (GPTransferCallbackArg *)gsimalloc(sizeof(GPTransferCallbackArg));
		if(arg)
		{
			memset(arg, 0, sizeof(GPTransferCallbackArg));
			arg->transfer = transfer->localID;
			arg->type = GP_TRANSFER_DONE;
			gpiAddCallback(connection, iconnection->callbacks[GPI_TRANSFER_CALLBACK], arg, NULL, GPI_ADD_TRANSFER_CALLBACK);
		}
	}

	return GPITrue;
}

static GPIBool gpiHandleData
(
  GPConnection * connection,
  GPITransfer * transfer,
  const char * headers,
  const char * buffer,
  int bufferLen
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	GPTransferCallbackArg * arg;
	GPIFile * file;
	GPIBool writeFailed;
	char intValue[16];
	int fileIndex;

	if(transfer->currentFile == -1)
		return GPIFalse;

	if(!gpiValueForKey(headers, "\\file\\", intValue, sizeof(intValue)))
		return GPIFalse;
	fileIndex = atoi(intValue);
	if((fileIndex < 0) || (fileIndex >= ArrayLength(transfer->files)))
		return GPIFalse;
	if(fileIndex != transfer->currentFile)
		return GPITrue;

	file = (GPIFile *)ArrayNth(transfer->files, transfer->currentFile);

	if(file->flags & GPI_FILE_DIRECTORY)
		return GPIFalse;

#ifdef GPI_ACKNOWLEDGED_WINDOW
	if(transfer->sender)
	{
		char intValue[16];

		if(!gpiValueForKey(headers, "\\pro\\", intValue, sizeof(intValue)))
			return GPIFalse;
		file->acknowledged = atoi(intValue);

		return GPITrue;
	}
#endif

	assert(file->file);
	if(!file->file)
		return GPIFalse;

	gsDebugFormat(GSIDebugCat_GP, GSIDebugType_File, GSIDebugLevel_RawDump,
		"HNDLDATA(PT): %d\n", bufferLen);

	writeFailed = (GPIBool)(fwrite(buffer, 1, bufferLen, file->file) != (size_t)bufferLen);
	if(writeFailed)
	{
		file->flags |= GPI_FILE_FAILED;
		file->reason = GP_FILE_WRITE_ERROR;

		fclose(file->file);
		file->file = NULL;
		remove(file->path);
	}
	else
	{
		MD5Update(&file->md5, (unsigned char *)buffer, bufferLen);

		file->progress += bufferLen;
		transfer->progress += bufferLen;

#ifdef GPI_ACKNOWLEDGED_WINDOW
		if(gpiPeerStartTransferMessage(connection, transfer->peer, GPI_BM_FILE_DATA, (GPITransferID_st)&transfer->transferID) != GP_NO_ERROR)
			return GPIFalse;
		gpiSendOrBufferString(connection, transfer->peer, "\\pro\\");
		gpiSendOrBufferInt(connection, transfer->peer, file->progress);
		gpiFinishTransferMessage(connection, transfer, NULL, 0);
#endif
	}

	arg = (GPTransferCallbackArg *)gsimalloc(sizeof(GPTransferCallbackArg));
	if(arg)
	{
		memset(arg, 0, sizeof(GPTransferCallbackArg));
		arg->transfer = transfer->localID;
		arg->index = transfer->currentFile;
		if(!writeFailed)
		{
			arg->type = GP_FILE_PROGRESS;
			arg->num = file->progress;
		}
		else
		{
			arg->type = GP_FILE_FAILED;
			arg->num = GP_FILE_WRITE_ERROR;
		}
		gpiAddCallback(connection, iconnection->callbacks[GPI_TRANSFER_CALLBACK], arg, NULL, GPI_ADD_TRANSFER_CALLBACK);
	}

	if(writeFailed)
	{
		gpiSkipCurrentFile(connection, transfer, GPI_SKIP_WRITE_ERROR);
	}

	return GPITrue;
}

static GPIBool gpiHandleSkip
(
  GPConnection * connection,
  GPITransfer * transfer,
  const char * headers
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	GPTransferCallbackArg * arg;
	GPIFile * file;
	char intValue[16];
	int fileIndex;
	int reason;

	if(!gpiValueForKey(headers, "\\file\\", intValue, sizeof(intValue)))
		return GPIFalse;
	fileIndex = atoi(intValue);
	if((fileIndex < 0) || (fileIndex >= ArrayLength(transfer->files)))
		return GPIFalse;
	file = (GPIFile *)ArrayNth(transfer->files, fileIndex);

	if(!gpiValueForKey(headers, "\\reason\\", intValue, sizeof(intValue)))
		return GPIFalse;
	reason = atoi(intValue);

	if(fileIndex != transfer->currentFile)
	{
		if(fileIndex < transfer->currentFile)
			return GPIFalse;

		if(reason == GPI_SKIP_USER_SKIP)
		{
			file->flags |= GPI_FILE_SKIP;
		}
		else
		{
			file->flags |= GPI_FILE_FAILED;
			if(reason == GPI_SKIP_READ_ERROR)
				file->reason = GP_FILE_READ_ERROR;
			else
				file->reason = GP_FILE_WRITE_ERROR;
		}

		return GPITrue;
	}

	if(!transfer->sender && file->file)
	{
		fclose(file->file);
		file->file = NULL;
		remove(file->path);
	}

	transfer->currentFile++;

	arg = (GPTransferCallbackArg *)gsimalloc(sizeof(GPTransferCallbackArg));
	if(arg)
	{
		memset(arg, 0, sizeof(GPTransferCallbackArg));
		arg->transfer = transfer->localID;
		arg->index = fileIndex;
		if(reason == GPI_SKIP_USER_SKIP)
		{
			arg->type = GP_FILE_SKIP;
		}
		else
		{
			arg->type = GP_FILE_FAILED;
			if(reason == GPI_SKIP_READ_ERROR)
				arg->num = GP_FILE_READ_ERROR;
			else
				arg->num = GP_FILE_WRITE_ERROR;
		}
		gpiAddCallback(connection, iconnection->callbacks[GPI_TRANSFER_CALLBACK], arg, NULL, GPI_ADD_TRANSFER_CALLBACK);
	}

	return GPITrue;
}

static GPIBool gpiHandleTransferThrottle
(
  GPConnection * connection,
  GPITransfer * transfer,
  const char * headers
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	int throttle;
	char intValue[16];
	GPTransferCallbackArg * arg;

	if(!gpiValueForKey(headers, "\\rate\\", intValue, sizeof(intValue)))
		return GPIFalse;
	throttle = atoi(intValue);

	transfer->throttle = throttle;

	if(transfer->sender)
	{
		if(gpiPeerStartTransferMessage(connection, transfer->peer, GPI_BM_FILE_TRANSFER_THROTTLE, (GPITransferID_st)&transfer->transferID) != GP_NO_ERROR)
			return GPIFalse;
		gpiSendOrBufferString(connection, transfer->peer, "\\rate\\");
		gpiSendOrBufferInt(connection, transfer->peer, throttle);
		gpiFinishTransferMessage(connection, transfer, NULL, 0);
	}

	arg = (GPTransferCallbackArg *)gsimalloc(sizeof(GPTransferCallbackArg));
	if(arg)
	{
		memset(arg, 0, sizeof(GPTransferCallbackArg));
		arg->transfer = transfer->localID;
		arg->type = GP_TRANSFER_THROTTLE;
		arg->num = throttle;
		gpiAddCallback(connection, iconnection->callbacks[GPI_TRANSFER_CALLBACK], arg, NULL, GPI_ADD_TRANSFER_CALLBACK);
	}

	return GPITrue;
}

static GPIBool gpiHandleTransferCancel
(
  GPConnection * connection,
  GPITransfer * transfer
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	GPTransferCallbackArg * arg;


	transfer->state = GPITransferCancelled;

	arg = (GPTransferCallbackArg *)gsimalloc(sizeof(GPTransferCallbackArg));
	if(arg)
	{
		memset(arg, 0, sizeof(GPTransferCallbackArg));
		arg->transfer = transfer->localID;
		arg->type = GP_TRANSFER_CANCELLED;
		gpiAddCallback(connection, iconnection->callbacks[GPI_TRANSFER_CALLBACK], arg, NULL, GPI_ADD_TRANSFER_CALLBACK);
	}

	return GPITrue;
}

static GPIBool gpiHandleTransferKeepalive
(
  GPConnection * connection,
  GPITransfer * transfer
)
{
	GSI_UNUSED(connection);
	GSI_UNUSED(transfer);

	return GPITrue;
}

static GPResult gpiSendFileEnd
(
  GPConnection * connection,
  GPITransfer * transfer,
  GPIFile * file
)
{
	CHECK_RESULT(gpiPeerStartTransferMessage(connection, transfer->peer, GPI_BM_FILE_END, (GPITransferID_st)&transfer->transferID));

	gpiSendOrBufferStringLenToPeer(connection, transfer->peer, "\\file\\", 6);
	gpiSendOrBufferInt(connection, transfer->peer, transfer->currentFile);

	if(!(file->flags & GPI_FILE_DIRECTORY))
	{
		unsigned char md5Raw[16];
		char md5[33];

		MD5Final(md5Raw, &file->md5);
		MD5Print(md5Raw, md5);

		gpiSendOrBufferString(connection, transfer->peer, "\\md5\\");
		gpiSendOrBufferString(connection, transfer->peer, md5);
	}

	gpiFinishTransferMessage(connection, transfer, NULL, 0);

	return GP_NO_ERROR;
}

static GPResult gpiSendFileBegin
(
  GPConnection * connection,
  GPITransfer * transfer,
  GPIFile * file
)
{
	char buffer[64];

	if(!gpiGetTransferFileInfo(file->file, &file->size, &file->modTime))
		Error(connection, GP_PARAMETER_ERROR, "Can't get info on file.");

	CHECK_RESULT(gpiPeerStartTransferMessage(connection, transfer->peer, GPI_BM_FILE_BEGIN, (GPITransferID_st)&transfer->transferID));
	sprintf(buffer, "\\file\\%d\\size\\%d\\mtime\\%u", transfer->currentFile, file->size, (unsigned int)file->modTime);
	gpiSendOrBufferString(connection, transfer->peer, buffer);
	gpiFinishTransferMessage(connection, transfer, NULL, 0);

	return GP_NO_ERROR;
}

static GPResult gpiSendFileData
(
  GPConnection * connection,
  GPITransfer * transfer,
  unsigned char * data,
  size_t len
)
{
	CHECK_RESULT(gpiPeerStartTransferMessage(connection, transfer->peer, GPI_BM_FILE_DATA, (GPITransferID_st)&transfer->transferID));

	gpiSendOrBufferStringLenToPeer(connection, transfer->peer, "\\file\\", 6);
	gpiSendOrBufferInt(connection, transfer->peer, transfer->currentFile);

	gpiFinishTransferMessage(connection, transfer, (char *)data, len);

	return GP_NO_ERROR;
}

GPResult gpiProcessCurrentFile
(
  GPConnection * connection,
  GPITransfer * transfer
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	GPIFile * file;
	GPTransferCallbackArg * arg;
	size_t num;
	int i;
	int total;

	assert(transfer->currentFile >= 0);
	assert(transfer->currentFile < ArrayLength(transfer->files));

	file = (GPIFile *)ArrayNth(transfer->files, transfer->currentFile);

	assert(!(file->flags & GPI_FILE_FAILED));

#ifdef GPI_CONFIRM_FILES
	if(file->flags & GPI_FILE_CONFIRMING)
		return GP_NO_ERROR;
#endif

	if(file->flags & GPI_FILE_SKIP)
	{
		gpiSkipCurrentFile(connection, transfer, GPI_SKIP_USER_SKIP);

		arg = (GPTransferCallbackArg *)gsimalloc(sizeof(GPTransferCallbackArg));
		if(arg)
		{
			memset(arg, 0, sizeof(GPTransferCallbackArg));
			arg->transfer = transfer->localID;
			arg->index = transfer->currentFile;
			arg->type = GP_FILE_SKIP;
			gpiAddCallback(connection, iconnection->callbacks[GPI_TRANSFER_CALLBACK], arg, NULL, GPI_ADD_TRANSFER_CALLBACK);
		}
	}
	else
	{
		if(file->flags & GPI_FILE_DIRECTORY)
		{
			arg = (GPTransferCallbackArg *)gsimalloc(sizeof(GPTransferCallbackArg));
			if(arg)
			{
				memset(arg, 0, sizeof(GPTransferCallbackArg));
				arg->transfer = transfer->localID;
				arg->index = transfer->currentFile;
				arg->type = GP_FILE_DIRECTORY;
				gpiAddCallback(connection, iconnection->callbacks[GPI_TRANSFER_CALLBACK], arg, NULL, GPI_ADD_TRANSFER_CALLBACK);
			}

			gpiSendFileEnd(connection, transfer, file);
			file->flags |= GPI_FILE_COMPLETED;
			transfer->currentFile++;
		}
		else
		{
			static char buffer[GPI_DATA_SIZE];

			if(!file->file)
			{
				file->file = fopen(file->path, "rb");
				if(file->file)
				{
					CHECK_RESULT(gpiSendFileBegin(connection, transfer, file));

					MD5Init(&file->md5);

					arg = (GPTransferCallbackArg *)gsimalloc(sizeof(GPTransferCallbackArg));
					if(arg)
					{
						memset(arg, 0, sizeof(GPTransferCallbackArg));
						arg->transfer = transfer->localID;
						arg->index = transfer->currentFile;
						arg->type = GP_FILE_BEGIN;
						gpiAddCallback(connection, iconnection->callbacks[GPI_TRANSFER_CALLBACK], arg, NULL, GPI_ADD_TRANSFER_CALLBACK);
					}
				}
				else
				{
					arg = (GPTransferCallbackArg *)gsimalloc(sizeof(GPTransferCallbackArg));
					if(arg)
					{
						memset(arg, 0, sizeof(GPTransferCallbackArg));
						arg->transfer = transfer->localID;
						arg->index = transfer->currentFile;
						arg->type = GP_FILE_FAILED;
						arg->num = GP_FILE_READ_ERROR;
						gpiAddCallback(connection, iconnection->callbacks[GPI_TRANSFER_CALLBACK], arg, NULL, GPI_ADD_TRANSFER_CALLBACK);
					}

					gpiSkipCurrentFile(connection, transfer, GPI_SKIP_READ_ERROR);
					file->flags |= GPI_FILE_FAILED;
					file->reason = GP_FILE_READ_ERROR;

					return GP_NO_ERROR;
				}
			}


			total = 0;
			for(i = 0 ; (file->progress < file->size) && !transfer->peer->outputBuffer.len  ; i++)
			{
#ifdef GPI_ACKNOWLEDGED_WINDOW
				if((file->acknowledged + GPI_ACKNOWLEDGED_WINDOW) < file->progress)
					break;
#endif

				num = fread(buffer, 1, sizeof(buffer), file->file);
				if(num)
				{
					MD5Update(&file->md5, (unsigned char*)buffer, num);

					CHECK_RESULT(gpiSendFileData(connection, transfer, (unsigned char*)buffer, num));

					transfer->progress += num;
					file->progress += num;
					total += num;

					arg = (GPTransferCallbackArg *)gsimalloc(sizeof(GPTransferCallbackArg));
					if(arg)
					{
						memset(arg, 0, sizeof(GPTransferCallbackArg));
						arg->transfer = transfer->localID;
						arg->index = transfer->currentFile;
						arg->type = GP_FILE_PROGRESS;
						arg->num = file->progress;
						gpiAddCallback(connection, iconnection->callbacks[GPI_TRANSFER_CALLBACK], arg, NULL, GPI_ADD_TRANSFER_CALLBACK);
					}
				}

				if((num < sizeof(buffer)) && (file->progress != file->size))
				{
					gpiSkipCurrentFile(connection, transfer, GPI_SKIP_READ_ERROR);
					file->flags |= GPI_FILE_FAILED;
					file->reason = GP_FILE_READ_ERROR;

					return GP_NO_ERROR;
				}
			}

			if(total)
			{
				gsDebugFormat(GSIDebugCat_GP, GSIDebugType_File, GSIDebugLevel_RawDump,
					"SENTTOTL(PT): %d\n", total);
			}

			if(file->progress == file->size)
			{
				fclose(file->file);
				file->file = NULL;

				gpiSendFileEnd(connection, transfer, file);

#ifdef GPI_CONFIRM_FILES
				file->flags |= GPI_FILE_CONFIRMING;
#else
				arg = (GPTransferCallbackArg *)gsimalloc(sizeof(GPTransferCallbackArg));
				if(arg)
				{
					memset(arg, 0, sizeof(GPTransferCallbackArg));
					arg->transfer = transfer->localID;
					arg->index = transfer->currentFile;
					arg->type = GP_FILE_END;
					gpiAddCallback(connection, iconnection->callbacks[GPI_TRANSFER_CALLBACK], arg, NULL, GPI_ADD_TRANSFER_CALLBACK);
				}

				file->flags |= GPI_FILE_COMPLETED;
				transfer->currentFile++;
#endif
			}
		}
	}

	return GP_NO_ERROR;
}

GPResult gpiProcessTransfer
(
  GPConnection * connection,
  GPITransfer * transfer
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	int currentFile;
	int len;
	GPTransferCallbackArg * arg;
	unsigned long now;

	if(!transfer->sender)
		return GP_NO_ERROR;

	if(transfer->state >= GPITransferComplete)
		return GP_NO_ERROR;

	now = current_time();

	if(!transfer->peer)
	{
		if((now - transfer->lastSend) > GPI_PEER_TIMEOUT_TIME)
		{
			GPTransferCallbackArg * arg;

			transfer->state = GPITransferNoConnection;

			arg = (GPTransferCallbackArg *)gsimalloc(sizeof(GPTransferCallbackArg));
			if(arg)
			{
				memset(arg, 0, sizeof(GPTransferCallbackArg));
				arg->transfer = transfer->localID;
				arg->type = GP_TRANSFER_NO_CONNECTION;
				gpiAddCallback(connection, iconnection->callbacks[GPI_TRANSFER_CALLBACK], arg, NULL, GPI_ADD_TRANSFER_CALLBACK);
			}

			return GP_NO_ERROR;
		}
	}
	else
	{
		if((now - transfer->lastSend) > GPI_KEEPALIVE_TIME)
		{
			CHECK_RESULT(gpiPeerStartTransferMessage(connection, transfer->peer, GPI_BM_FILE_TRANSFER_KEEPALIVE, (GPITransferID_st)&transfer->transferID));
			gpiFinishTransferMessage(connection, transfer, NULL, 0);
		}
	}

	if(transfer->throttle == 0)
		return GP_NO_ERROR;

	if(transfer->state < GPITransferTransferring)
		return GP_NO_ERROR;

	if(ArrayLength(transfer->peer->messages))
		return GP_NO_ERROR;

	len = ArrayLength(transfer->files);
	while(transfer->currentFile < len)
	{
		currentFile = transfer->currentFile;
		CHECK_RESULT(gpiProcessCurrentFile(connection, transfer));
		if(currentFile == transfer->currentFile)
			break;
	}

	if(transfer->currentFile == len)
	{
		arg = (GPTransferCallbackArg *)gsimalloc(sizeof(GPTransferCallbackArg));
		if(arg)
		{
			memset(arg, 0, sizeof(GPTransferCallbackArg));
			arg->transfer = transfer->localID;
			arg->type = GP_TRANSFER_DONE;
			gpiAddCallback(connection, iconnection->callbacks[GPI_TRANSFER_CALLBACK], arg, NULL, GPI_ADD_TRANSFER_CALLBACK);
		}

		transfer->state = GPITransferComplete;
	}

	return GP_NO_ERROR;
}

GPResult gpiProcessTransfers
(
  GPConnection * connection
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	int len;
	int i;
	GPITransfer * transfer;

	len = ArrayLength(iconnection->transfers);
	for(i = 0 ; i < len ; i++)
	{
		transfer = (GPITransfer *)ArrayNth(iconnection->transfers, i);

		gpiProcessTransfer(connection, transfer);
	}

	return GP_NO_ERROR;
}

GPIBool gpiGetTransferFileInfo
(
  FILE * file,
  int * size,
  gsi_time * modTime
)
{
#ifdef _WIN32
	struct _stat stats;

	if(_fstat(_fileno(file), &stats) != 0)
		return GPIFalse;

	*size = (int)stats.st_size;
	*modTime = (gsi_time)stats.st_mtime;
#else
	if(fseek(file, 0, SEEK_END) != 0)
		return GPIFalse;

	*size = (int)ftell(file);
	if(*size == -1)
		return GPIFalse;

	*modTime = 0;

	fseek(file, 0, SEEK_SET);
#endif

	return GPITrue;
}

void gpiTransferPeerDestroyed
(
  GPConnection * connection,
  GPIPeer * peer
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	GPTransferCallbackArg * arg;
	GPITransfer * transfer;
	int i;
	int len;

	len = ArrayLength(iconnection->transfers);
	for(i = 0 ; i < len ; i++)
	{
		transfer = (GPITransfer *)ArrayNth(iconnection->transfers, i);

		if (transfer->peer == peer)
		{
			arg = (GPTransferCallbackArg *)gsimalloc(sizeof(GPTransferCallbackArg));
			if(arg)
			{
				memset(arg, 0, sizeof(GPTransferCallbackArg));
				arg->transfer = transfer->localID;
				arg->type = GP_TRANSFER_LOST_CONNECTION;
				gpiAddCallback(connection, iconnection->callbacks[GPI_TRANSFER_CALLBACK], arg, NULL, GPI_ADD_TRANSFER_CALLBACK);
			}
		
			transfer->state = GPITransferNoConnection;
		}
	}

}

void gpiTransfersHandlePong
(
  GPConnection * connection,
  GPProfile profile,
  GPIPeer * peer
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	GPITransfer * transfer;
	int i;
	int len;

	len = ArrayLength(iconnection->transfers);
	for(i = 0 ; i < len ; i++)
	{
		transfer = (GPITransfer *)ArrayNth(iconnection->transfers, i);
		assert(transfer);

		if((transfer->state == GPITransferPinging) && (transfer->profile == profile))
		{
			if(!peer)
			{
				GPTransferCallbackArg * arg;

				transfer->state = GPITransferNoConnection;

				arg = (GPTransferCallbackArg *)gsimalloc(sizeof(GPTransferCallbackArg));
				if(arg)
				{
					memset(arg, 0, sizeof(GPTransferCallbackArg));
					arg->transfer = transfer->localID;
					arg->type = GP_TRANSFER_NO_CONNECTION;
					gpiAddCallback(connection, iconnection->callbacks[GPI_TRANSFER_CALLBACK], arg, NULL, GPI_ADD_TRANSFER_CALLBACK);
				}
			}
			else
			{
				transfer->peer = peer;

				gpiSendTransferRequest(connection, transfer);

				transfer->state = GPITransferWaiting;
			}
		}
	}
}
#endif

GPResult gpiSendTransferReply
(
  GPConnection * connection,
  const GPITransferID * transferID,
  GPIPeer * peer,
  int result,
  const char * msg
)
{
	char buffer[32];

	if(!msg)
		msg = "";

	CHECK_RESULT(gpiPeerStartTransferMessage(connection, peer, GPI_BM_FILE_SEND_REPLY, transferID));

	sprintf(buffer, "\\version\\%d\\result\\%d", GPI_TRANSFER_VERSION, result);
	CHECK_RESULT(gpiSendOrBufferString(connection, peer, buffer));

	CHECK_RESULT(gpiPeerFinishTransferMessage(connection, peer, msg, -1));

	return GP_NO_ERROR;
}

void gpiHandleTransferMessage
(
  GPConnection * connection,
  GPIPeer * peer,
  int type,
  const char * headers,
  const char * buffer,
  int len
)
{
	char value[64];
	GPITransferID transferID;
#ifndef NOFILE
	GPITransfer * transfer;
	GPIBool success;
#endif

	if(!gpiValueForKey(headers, "\\xfer\\", value, sizeof(value)))
		return;
	if(sscanf(value, "%d %u %u", &transferID.profileid, &transferID.count, &transferID.time) != 3)
		return;

#ifdef NOFILE
	gpiSendTransferReply(connection, &transferID, peer, GPI_NOT_ACCEPTING, NULL);
#else

	if(type == GPI_BM_FILE_SEND_REQUEST)
	{
		if(!gpiHandleSendRequest(connection, peer, &transferID, headers, buffer, len))
			gpiSendTransferReply(connection, &transferID, peer, GPI_NOT_ACCEPTING, NULL);

		return;
	}

	transfer = gpiFindTransferByTransferID(connection, &transferID);
	if(!transfer || (transfer->peer != peer))
		return;

	switch(type)
	{
		case GPI_BM_FILE_SEND_REPLY:
			success = gpiHandleSendReply(connection, transfer, headers, buffer, len);
			break;
		case GPI_BM_FILE_BEGIN:
			success = gpiHandleBegin(connection, transfer, headers);
			break;
		case GPI_BM_FILE_END:
			success = gpiHandleEnd(connection, transfer, headers);
			break;
		case GPI_BM_FILE_DATA:
			success = gpiHandleData(connection, transfer, headers, buffer, len);
			break;
		case GPI_BM_FILE_SKIP:
			success = gpiHandleSkip(connection, transfer, headers);
			break;
		case GPI_BM_FILE_TRANSFER_THROTTLE:
			success = gpiHandleTransferThrottle(connection, transfer, headers);
			break;
		case GPI_BM_FILE_TRANSFER_CANCEL:
			success = gpiHandleTransferCancel(connection, transfer);
			break;
		case GPI_BM_FILE_TRANSFER_KEEPALIVE:
			success = gpiHandleTransferKeepalive(connection, transfer);
			break;
		default:
			success = GPITrue;
	}

	if(!success)
		gpiTransferError(connection, transfer);
#endif

	GSI_UNUSED(type);
	GSI_UNUSED(buffer);
	GSI_UNUSED(len);
}
