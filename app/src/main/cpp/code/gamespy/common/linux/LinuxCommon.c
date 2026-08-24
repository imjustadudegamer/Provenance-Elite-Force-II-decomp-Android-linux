#include "../gsCommon.h"
#include "../gsMemory.h"
#include "../gsDebug.h"

//extern void * __cdecl _aligned_malloc(size_t size, int boundary);

#ifdef GSI_COMMON_DEBUG
	static void DebugCallback(GSIDebugCategory theCat, GSIDebugType theType,
	                          GSIDebugLevel theLevel, const char * theTokenStr,
	                          va_list theParamList)

	{
		GSI_UNUSED(theLevel);
		{
			static char    string[256];
			vsprintf(string, theTokenStr, theParamList); 			
			printf(string);
		}
		printf("[%s][%s] ", 
				gGSIDebugCatStrings[theCat], 
				gGSIDebugTypeStrings[theType]);

		vprintf(theTokenStr, theParamList);
	}
#endif


extern int test_main(int argc, char ** argp); 


int main(int argc, char** argp)
{
	int		ret		= 0;

	#ifdef GSI_COMMON_DEBUG
		gsSetDebugCallback(DebugCallback);
		gsSetDebugLevel(GSIDebugCat_All, GSIDebugType_All,    GSIDebugLevel_Verbose);
	#endif

	ret = test_main(argc, argp);


	return ret;
}




