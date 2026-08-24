/*
GameSpy PT SDK 
Dan "Mr. Pants" Schoenblum
dan@gamespy.com

Copyright 2000 GameSpy Industries, Inc

*/

#include <stdio.h>
#include "pt.h"
#include "../ghttp/ghttp.h"
#include "../common/gsStringUtil.h"
#include "../common/gsAvailable.h"

#define PTA_DEFAULT_VERCHECK_URL    "http://motd." GSI_DOMAIN_NAME "/motd/vercheck.asp"
#define PTA_DEFAULT_MOTD_URL        "http://motd." GSI_DOMAIN_NAME "/motd/motd.asp"
#define PTA_DEFAULT_FILEPLANET_URL  "http://www.fileplanet.com/dlfileraw.asp"
#define MAX_MIRRORS         32
#define PTA_MAX_STRING_SIZE 2048

char gPTAVercheckURL[PTA_MAX_STRING_SIZE];
char gPTAMOTDURL[PTA_MAX_STRING_SIZE];
char gPTAFilePlanetURL[PTA_MAX_STRING_SIZE];

typedef struct ptaPatchData
{
	ptPatchCallback callback;
	void * param;
} ptaPatchData;

typedef struct ptaFilePlanetInfoData
{
	int fileID;
	ptFilePlanetInfoCallback callback;
	void * param;
} ptaFilePlanetInfoData;


static const char * ptaGetKeyValue
(
	const char * buffer,
	const char * key
)
{
	static char value[PTA_MAX_STRING_SIZE];
	const char * str;
	int len;

	str = strstr(buffer, key);
	if(!str)
		return NULL;
	str += strlen(key);
	len = (int)strcspn(str, "\\");
	len = min(len, (int)sizeof(value) - 1);
	memcpy(value, str, (unsigned int)len);
	value[len] = '\0';

	return value;
}

static char Line[PTA_MAX_STRING_SIZE];
static int ptaFillLine
(
	const char * buffer
)
{
	char * str = Line;
	int i;

	for(i = 0 ; isspace(*buffer) ; i++)
		buffer++;

	if(*buffer == '\0')
		return EOF;

	for( ; *buffer && ((*buffer != 0x0A) && (*buffer != 0x0D)) ; i++)
	{
		if(i == sizeof(Line) - 1)
		{
			assert(0);
			return EOF;
		}
		*str++ = *buffer++;
	}
	*str = '\0';

	return i;
}

static void ptaCallPatchCallback
(
	ptaPatchData * data,
	PTBool available,
	PTBool mandatory,
	const char * versionName,
	int fileID,
	const char * downloadURL
)
{
	if(data->callback)
	{
#ifndef GSI_UNICODE
		data->callback(available, mandatory, versionName, fileID, downloadURL, data->param);
#else
		unsigned short versionName_W[255];
		unsigned short downloadURL_W[PTA_MAX_STRING_SIZE*2];
		UTF8ToUCS2String(versionName, versionName_W);
		AsciiToUCS2String(downloadURL, downloadURL_W);
		data->callback(available, mandatory, versionName_W, fileID, downloadURL_W, data->param);
#endif
	}

	gsifree(data);
}

static GHTTPBool ptaPatchFailed
(
	ptaPatchData * data
)
{
	ptaCallPatchCallback(data, PTFalse, PTFalse, "", 0, "");

	return GHTTPTrue;
}

static void ptaCallFilePlanetInfoCallback
(
	ptaFilePlanetInfoData * data,
	PTBool found,
	const char * description,
	const char * size,
	int numMirrors,
	char ** mirrorNames,
	char ** mirrorURLs
)
{
	int i;

	if(data->callback)
	{
		if (!found)
			data->callback(data->fileID, PTFalse, NULL, NULL, 0, NULL, NULL, data->param);
		else
		{
#ifndef GSI_UNICODE
			data->callback(data->fileID, found, description, size, numMirrors, (const char**)mirrorNames, (const char**)mirrorURLs, data->param);
#else
			unsigned short description_W[255];
			unsigned short size_W[1024];
			unsigned short** mirrorNames_W;
			unsigned short** mirrorURLs_W;
			int i;

			UTF8ToUCS2String(description, description_W);
			AsciiToUCS2String(size, size_W);
			mirrorNames_W = UTF8ToUCS2StringArrayAlloc((const UTF8String *)mirrorNames, numMirrors);
			mirrorURLs_W = UTF8ToUCS2StringArrayAlloc((const UTF8String *)mirrorURLs, numMirrors);
			data->callback(data->fileID, found, description_W, size_W, numMirrors, (const unsigned short**)mirrorNames_W, (const unsigned short**)mirrorURLs_W, data->param);

			for (i=0; i < numMirrors; i++)
			{
				gsifree(mirrorNames[i]);
				gsifree(mirrorURLs[i]);
			}
			gsifree(mirrorNames);
			gsifree(mirrorURLs);
#endif
		}
	}

	for(i = 0 ; i < numMirrors ; i++)
	{
		gsifree(mirrorNames[i]);
		gsifree(mirrorURLs[i]);
	}

	gsifree(data);
}

static GHTTPBool ptaFilePlanetInfoFailed
(
	ptaFilePlanetInfoData * data
)
{
	ptaCallFilePlanetInfoCallback(data, PTFalse, NULL, NULL, 0, NULL, NULL);

	return GHTTPTrue;
}

static GHTTPBool ptaPatchCompletedCallback
(
	GHTTPRequest request,
	GHTTPResult result,
	char * buffer,
	GHTTPByteCount bufferLen,
	void * param
)
{
	ptaPatchData * data = (ptaPatchData *)param;
	const char * value;
	PTBool mandatory;
	int fileID;
	char versionName[PTA_MAX_STRING_SIZE];
	char downloadURL[PTA_MAX_STRING_SIZE];

	GSI_UNUSED(bufferLen);
	GSI_UNUSED(request);

	if(result != GHTTPSuccess)
		return ptaPatchFailed(data);

	value = ptaGetKeyValue(buffer, "\\newver\\");
	if(!value || !atoi(value))
		return ptaPatchFailed(data);

	value = ptaGetKeyValue(buffer, "\\lockout\\");
	mandatory = (value && atoi(value));

	value = ptaGetKeyValue(buffer, "\\fpfileid\\");
	if(value)
		fileID = atoi(value);
	else
		fileID = 0;

	value = ptaGetKeyValue(buffer, "\\newvername\\");
	if(value)
	{
		strncpy(versionName, value, sizeof(versionName));
		versionName[sizeof(versionName) - 1] = '\0';
	}
	else
		versionName[0] = '\0';

	value = ptaGetKeyValue(buffer, "\\dlurl\\");
	if(value)
	{
		strncpy(downloadURL, value, sizeof(downloadURL));
		downloadURL[sizeof(downloadURL) - 1] = '\0';
	}
	else
		downloadURL[0] = '\0';

	ptaCallPatchCallback(data, PTTrue, mandatory, versionName, fileID, downloadURL);

	return GHTTPTrue;
}

PTBool ptCheckForPatchA
(
	int productID,
	const char * versionUniqueID,
	int distributionID,
	ptPatchCallback callback,
	PTBool blocking,
	void * param
)
{
	int charsWritten;
	char aURL[PTA_MAX_STRING_SIZE];
	ptaPatchData * data;

	if(__GSIACResult != GSIACAvailable)
		return PTFalse;

	assert(versionUniqueID);
	if(!versionUniqueID)
		return PTFalse;
	assert(callback);
	if(!callback)
		return PTFalse;

	if (gPTAVercheckURL[0] == '\0')
		sprintf(gPTAVercheckURL, PTA_DEFAULT_VERCHECK_URL);

	data = (ptaPatchData *)gsimalloc(sizeof(ptaPatchData));
	if(!data)
		return PTFalse;
	memset(data, 0, sizeof(ptaPatchData));
	data->callback = callback;
	data->param = param;

	charsWritten = 
		snprintf(aURL, PTA_MAX_STRING_SIZE,
		"%s?productid=%d&versionuniqueid=%s&distid=%d&gamename=%s",
		gPTAVercheckURL, productID, versionUniqueID, distributionID,
		__GSIACGamename);
	
	assert(charsWritten >= 0);
	if (charsWritten < 0)
		return PTFalse;

	if((ghttpGetFileA(aURL, (GHTTPBool)blocking, ptaPatchCompletedCallback, data) == (unsigned char)(-1)) && !blocking)
		return PTFalse;

	return PTTrue;
}
#ifdef GSI_UNICODE
PTBool ptCheckForPatchW
(
	int productID,
	const unsigned short* versionUniqueID,
	int distributionID,
	ptPatchCallback callback,
	PTBool blocking,
	void * param
)
{
	char versionUniqueID_A[255];
	UCS2ToUTF8String(versionUniqueID, versionUniqueID_A);
	return ptCheckForPatchA(productID, versionUniqueID_A, distributionID, callback, blocking, param);
}
#endif

PTBool ptTrackUsageA
(
	int userID,
	int productID,
	const char * versionUniqueID,
	int distributionID,
	PTBool blocking
)
{
	int charsWritten;
	char aURL[PTA_MAX_STRING_SIZE];
	
	if(__GSIACResult != GSIACAvailable)
		return PTFalse;

	assert(versionUniqueID);
	if(!versionUniqueID)
		return PTFalse;

	if (gPTAMOTDURL[0] == '\0')
		sprintf(gPTAMOTDURL, PTA_DEFAULT_MOTD_URL);

	charsWritten = snprintf(aURL, PTA_MAX_STRING_SIZE, 
		"%s?userid=%d&productid=%d&versionuniqueid=%s&distid=%d&uniqueid=%s&gamename=%s",
		gPTAMOTDURL, userID, productID, versionUniqueID,	distributionID,	GOAGetUniqueID(),
		__GSIACGamename);
	assert(charsWritten >= 0);
	if (charsWritten < 0)
		return PTFalse;
	if(ghttpGetFileA(aURL, (GHTTPBool)blocking, NULL, NULL) == -1)
		return PTFalse;

	return PTTrue;
}
#ifdef GSI_UNICODE
PTBool ptTrackUsageW
(
	int userID,
	int productID,
	const unsigned short* versionUniqueID,
	int distributionID,
	PTBool blocking
)
{
	char versionUniqueID_A[255];
	UCS2ToUTF8String(versionUniqueID, versionUniqueID_A);
	return ptTrackUsageA(userID, productID, versionUniqueID_A, distributionID, blocking);
}
#endif

PTBool ptCreateCheckPatchTrackUsageReqA
(
	int userID,
	int productID,
	const char * versionUniqueID,
	int distributionID,
	ptPatchCallback callback,
	PTBool blocking,
	int *request,
	void * param
)
{
	int charsWritten;
	char aURL[PTA_MAX_STRING_SIZE];
	ptaPatchData * data;
	
	if(__GSIACResult != GSIACAvailable)
		return PTFalse;

	assert(versionUniqueID);
	if(!versionUniqueID)
		return PTFalse;
	assert(callback);
	if(!callback)
		return PTFalse;

	data = (ptaPatchData *)gsimalloc(sizeof(ptaPatchData));
	if(!data)
		return PTFalse;
	memset(data, 0, sizeof(ptaPatchData));
	data->callback = callback;
	data->param = param;

	if (gPTAVercheckURL[0] == '\0')
		sprintf(gPTAVercheckURL, PTA_DEFAULT_VERCHECK_URL);

	charsWritten = snprintf(aURL, PTA_MAX_STRING_SIZE,  
		"%s?userid=%d&productid=%d&versionuniqueid=%s&distid=%d&uniqueid=%s&gamename=%s",
		gPTAVercheckURL, userID, productID, versionUniqueID, distributionID,	GOAGetUniqueID(),
		__GSIACGamename);

	assert(charsWritten >= 0);
	if (charsWritten < 0)
		return PTFalse;

	*request = ghttpGetFileA(aURL, (GHTTPBool)blocking, ptaPatchCompletedCallback, data);
	if (*request == -1)
		return PTFalse;
	return PTTrue;
}

#ifdef GSI_UNICODE
PTBool ptCreateCheckPatchTrackUsageReqW
(
	int userID,
	int productID,
	const unsigned short * versionUniqueID,
	int distributionID,
	ptPatchCallback callback,
	PTBool blocking,
	int *request,
	void * param
 )
{
	char versionUniqueID_A[255];
	UCS2ToUTF8String(versionUniqueID, versionUniqueID_A);
	return ptCreateCheckPatchTrackUsageReqA(userID, productID, versionUniqueID_A, distributionID, callback, blocking, request, param);
}
#endif

PTBool ptCheckForPatchAndTrackUsageA
(
	int userID,
	int productID,
	const char * versionUniqueID,
	int distributionID,
	ptPatchCallback callback,
	PTBool blocking,
	void * param
)
{
	GHTTPRequest aRequest;
	
	return ptCreateCheckPatchTrackUsageReqA(userID, productID, versionUniqueID, distributionID, callback, blocking, &aRequest, param);
	
}

#ifdef GSI_UNICODE
PTBool ptCheckForPatchAndTrackUsageW
(
	int userID,
	int productID,
	const unsigned short * versionUniqueID,
	int distributionID,
	ptPatchCallback callback,
	PTBool blocking,
	void * param
)
{
	char versionUniqueID_A[255];
	UCS2ToUTF8String(versionUniqueID, versionUniqueID_A);
	return ptCheckForPatchAndTrackUsageA(userID, productID, versionUniqueID_A, distributionID, callback, blocking, param);
}
#endif

static GHTTPBool ptaFilePlanetCompletedCallback
(
	GHTTPRequest request,
	GHTTPResult result,
	char * buffer,
	GHTTPByteCount bufferLen,
	void * param
)
{
	ptaFilePlanetInfoData * data = (ptaFilePlanetInfoData *)param;
	int len;
	char description[256];
	char size[64];
	char * mirrorNames[MAX_MIRRORS];
	char * mirrorURLs[MAX_MIRRORS];
	int i;
	char * str;

	if(__GSIACResult != GSIACAvailable)
		return GHTTPFalse;

	GSI_UNUSED(request);
	GSI_UNUSED(bufferLen);

	if(result != GHTTPSuccess)
		return ptaFilePlanetInfoFailed(data);

	len = ptaFillLine(buffer);
	if(len == EOF)
		return ptaFilePlanetInfoFailed(data);
	buffer += len;
	strncpy(description, Line, sizeof(description));
	description[sizeof(description) - 1] = '\0';

	len = ptaFillLine(buffer);
	if(len == EOF)
		return ptaFilePlanetInfoFailed(data);
	buffer += len;
	strncpy(size, Line, sizeof(size));
	size[sizeof(size) - 1] = '\0';

	for(i = 0 ; (i < MAX_MIRRORS) && ((len = ptaFillLine(buffer)) != EOF) ; )
	{
		buffer += len;

		str = strchr(Line, '\t');
		if(!str)
			continue;

		len = (str - Line);
		mirrorNames[i] = (char *)gsimalloc((unsigned int)len + 1);
		if(!mirrorNames[i])
			break;
		memcpy(mirrorNames[i], Line, (unsigned int)len);
		mirrorNames[i][len] = '\0';

		str++;
		len = (int)strlen(str);
		mirrorURLs[i] = (char *)gsimalloc((unsigned int)len + 1);
		if(!mirrorURLs[i])
		{
			gsifree(mirrorNames[i]);
			break;
		}
		strcpy(mirrorURLs[i], str);

		i++;
	}

	ptaCallFilePlanetInfoCallback(data, PTTrue, description, size, i, mirrorNames, mirrorURLs);

	return GHTTPTrue;
}

PTBool ptLookupFilePlanetInfo
(
	int fileID,
	ptFilePlanetInfoCallback callback,
	PTBool blocking,
	void * param
)
{
	char aURL[PTA_MAX_STRING_SIZE];
	ptaFilePlanetInfoData * data;

	assert(callback);
	if(!callback)
		return PTFalse;

	if (gPTAFilePlanetURL[0] == '\0')
		sprintf(gPTAFilePlanetURL, PTA_DEFAULT_FILEPLANET_URL);

	data = (ptaFilePlanetInfoData *)gsimalloc(sizeof(ptaFilePlanetInfoData));
	if(!data)
		return PTFalse;
	memset(data, 0, sizeof(ptaFilePlanetInfoData));
	data->callback = callback;
	data->param = param;
	data->fileID = fileID;

	snprintf(aURL, PTA_MAX_STRING_SIZE, 
		"%s?file=%d&gamename=%s", gPTAFilePlanetURL, fileID, __GSIACGamename);
	
	
	if((ghttpGetFileA(aURL, (GHTTPBool)blocking, ptaFilePlanetCompletedCallback, data) == -1) && !blocking)
		return PTFalse;

	return PTTrue;
}
