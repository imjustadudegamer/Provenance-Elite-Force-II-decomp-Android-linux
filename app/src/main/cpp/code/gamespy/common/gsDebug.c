#include "gsCommon.h"
#include "gsDebug.h"


#ifdef GSI_COMMON_DEBUG

#if defined(_NITRO)
#include "../../common/nitro/screen.h"
#define printf Printf
#define vprintf VPrintf
#endif

static struct GSIDebugInstance gGSIDebugInstance; 

char* gGSIDebugCatStrings[GSIDebugCat_Count] =
{
	" APP", " GP ", "PEER", " QR2", "  SB", "  V2", "  AD", "  NN", "HTTP", "CDKY", " CMN"
};
char* gGSIDebugTypeStrings[GSIDebugType_Count] =
{
	" NET", "FILE", " MEM", "STAT", "MISC"
};
char* gGSIDebugLevelStrings[GSIDebugLevel_Count] =
{
	"*ERR", "****", "----", "    ", "    ", "    ", "  ->"
};
char* gGSIDebugLevelDescriptionStrings[8] =
{
	"None", "<None+1>", "<Normal>", "<Debug>", "<Verbose>", "<Verbose+1>", "<Verbose+2>", "<Hardcore>"
};

static gsi_u32 gsiDebugLog2(gsi_u32 theInt)
{
	gsi_u32 total = 0;
	while (theInt > 1)
	{
		theInt = theInt >> 1;
		total++;
	}
	return total;
}


static void gsiDebugCallback(GSIDebugCategory category, GSIDebugType type,
						GSIDebugLevel level, const char * format, va_list params)
{
	#if defined(_PSP)
		vprintf(format, params);
	#elif defined(_PS2)
		vprintf(format, params);

	#elif defined(_PS3)
		vprintf(format, params);

	#elif defined(_WIN32)
		static char string[256];
		vsprintf(string, format, params); 			
		OutputDebugStringA(string);

	#elif defined(_LINUX) || defined(_MACOSX)
		vprintf(format, params);
	#elif defined(_NITRO)
		VPrintf(format, params);
	#elif defined(_REVOLUTION)
		static char string[256];
		vsprintf(string, format, params);
		OSReport(string);
	#else
		va_list argptr;
		static char    string[256];
		va_start(argptr, format);
		vsprintf(string, format, argptr); 
		va_end(argptr);
		gsDebugTTyPrint(string);
	#endif
	
	GSI_UNUSED(category);
	GSI_UNUSED(type);
	GSI_UNUSED(level);
}





void gsDebugVaList(GSIDebugCategory theCat, GSIDebugType theType, 
					  GSIDebugLevel theLevel, const char* theTokenStr, 
					  va_list theParamList)
{
	GSIDebugLevel aCurLevel;

	assert(theCat   <= GSIDebugCat_Count);
	assert(theType  <= GSIDebugType_Count);
	assert(theLevel <= (1<<GSIDebugLevel_Count));
	assert(theTokenStr);

	if (gGSIDebugInstance.mInitialized == 0)
	{
		gGSIDebugInstance.mInitialized = 1;
		gsiInitializeCriticalSection(&gGSIDebugInstance.mDebugCrit);
	}

	gsiEnterCriticalSection(&gGSIDebugInstance.mDebugCrit);

	aCurLevel = gGSIDebugInstance.mGSIDebugLevel[theCat][theType];
	if (aCurLevel & theLevel) 
	{
#if !defined(_NITRO)
		if (gGSIDebugInstance.mGSIDebugFile)
		{
			fprintf(gGSIDebugInstance.mGSIDebugFile, "[%s][%s][%s] ", 
				gGSIDebugCatStrings[theCat], 
				gGSIDebugTypeStrings[theType],
				gGSIDebugLevelStrings[gsiDebugLog2(theLevel)]);
			
			vfprintf(gGSIDebugInstance.mGSIDebugFile, theTokenStr, 
				theParamList);
		}
#endif
		if (gGSIDebugInstance.mDebugCallback != NULL)
		{
			(*gGSIDebugInstance.mDebugCallback)(theCat, theType, theLevel,
			                                     theTokenStr, theParamList);
		}
		else
		{
			gsiDebugCallback(theCat, theType, theLevel,
			                                     theTokenStr, theParamList);
		}
	}
	
	gsiLeaveCriticalSection(&gGSIDebugInstance.mDebugCrit);
}


void gsDebugFormat(GSIDebugCategory theCat, GSIDebugType theType, 
					  GSIDebugLevel theLevel, const char* theTokenStr, 
					  ...)
{
	va_list aParameterList;

	assert(theCat   <= GSIDebugCat_Count);
	assert(theType  <= GSIDebugType_Count);
	assert(theLevel <= (1<<GSIDebugLevel_Count));
	assert(theTokenStr);

	va_start(aParameterList, theTokenStr);
	
	gsDebugVaList(theCat, theType, theLevel, theTokenStr, aParameterList);
}


static void HexEncode16(const char* theInStream, char* theOutStream, 
				 unsigned int theInLen)
{
	const int  aRowWidth     = 64;     
	const char aReplaceChar  = '.';    
	const int  aTextOffSet   = 41;     
	char* aTextOutStream = (theOutStream+aTextOffSet); 
	const unsigned int aWriteBit = theInLen & 1; 

	assert(theInLen <= 16);

	memset(theOutStream, ' ', aRowWidth);

	while(theInLen--)
	{
		unsigned char aChar = (unsigned char)(*theInStream++);

		sprintf(theOutStream, "%02X", aChar);

		if (isgraph(aChar))
			*(aTextOutStream++) = (char)aChar;
		else
			*(aTextOutStream++) = aReplaceChar;

		theOutStream += 2;

		if ((theInLen & 1) == aWriteBit)
			*theOutStream++ = ' ';
	}

	*theOutStream = ' ';

	*(aTextOutStream) = '\0';
}


void gsDebugBinary(GSIDebugCategory theCat, GSIDebugType theType,
             GSIDebugLevel theLevel, const char* theBuffer, gsi_i32 theLength)
{
	int aBytesLeft = theLength;
	const char* aReadPos = theBuffer;
	char aHexStr[80];

	while(aBytesLeft > 0)
	{
		gsi_i32 aBytesToRead = min(aBytesLeft, 16);

		HexEncode16(aReadPos, aHexStr, (unsigned int)aBytesToRead);
		gsDebugFormat(theCat, theType, theLevel, "  %s\r\n", aHexStr);

		aReadPos   += aBytesToRead;
		aBytesLeft -= aBytesToRead;
	};
}


void gsSetDebugLevel(GSIDebugCategory theCat, GSIDebugType theType, 
					  GSIDebugLevel theLevel)
{
	assert(theCat   <= GSIDebugCat_Count);
	assert(theType  <= GSIDebugType_Count);

	if (theCat == GSIDebugCat_Count)
	{
		int i=0;
		for (; i<GSIDebugCat_Count; i++)
			gsSetDebugLevel((GSIDebugCategory)i, theType, theLevel);

		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Misc,
			GSIDebugLevel_Debug,
			"Debug level set to %s for all categories (SDKs)\r\n",
			gGSIDebugLevelDescriptionStrings[gsiDebugLog2(theLevel)]);

		return;
	}
	
	if (theType == GSIDebugType_Count)
	{
		int i=0;
		for (; i<GSIDebugType_Count; i++)
			gsSetDebugLevel(theCat, (GSIDebugType)i, theLevel);

		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Misc,
			GSIDebugLevel_Debug,
			"Debug level set to %s for all types\r\n",
			gGSIDebugLevelDescriptionStrings[gsiDebugLog2(theLevel)]);
		return;
	}

	if (gGSIDebugInstance.mGSIDebugLevel[theCat][theType] != theLevel)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Misc, 
			GSIDebugLevel_Comment,
			"Changing debug level: [%s][%s][%02X]\r\n",
			gGSIDebugCatStrings[theCat], 
			gGSIDebugTypeStrings[theType], 
			theLevel );
		gGSIDebugInstance.mGSIDebugLevel[theCat][theType] = theLevel;
	}
}

#if !defined(_NITRO)

void gsSetDebugFile(FILE* theFile)
{
	if (theFile != gGSIDebugInstance.mGSIDebugFile)
	{
		if (gGSIDebugInstance.mGSIDebugFile != NULL)
		{
			gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Misc,
				GSIDebugLevel_Comment, "Debug disabled in this file\r\n");
		}

		{
			gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Misc,
				GSIDebugLevel_Comment, "Debug enabled in this file\r\n");
		}

		gGSIDebugInstance.mGSIDebugFile = theFile;
	}
}


FILE* gsOpenDebugFile(const char* theFileName)
{
	FILE* aFile = NULL;

	assert(theFileName != NULL);

	aFile = fopen(theFileName, "w+");
	if (aFile != NULL)
		gsSetDebugFile(aFile);

	return aFile;
}


FILE* gsGetDebugFile()
{
	return gGSIDebugInstance.mGSIDebugFile;
}

#endif

void gsSetDebugCallback(GSIDebugCallback theCallback)
{
	gGSIDebugInstance.mDebugCallback = theCallback;
}


#endif 
