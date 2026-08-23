#include "gsCommon.h"
#include "gsDebug.h"



extern void  _gsDebugAssert	(const char * string);

static gsDebugAssertCallback gsDebugAssertHandler = _gsDebugAssert;

void gsDebugAssertCallbackSet(gsDebugAssertCallback theCallback)
{
	if (theCallback)
		gsDebugAssertHandler = theCallback;
	else
		gsDebugAssertHandler = _gsDebugAssert;
}


void  gsDebugAssert				(const char *szError,const char *szText, const char *szFile, int line)
{
	char String[256];
	sprintf(&String[0], szError,szText,szFile,line);

	(*gsDebugAssertHandler)(String);

}


#ifdef _XBOX
	void  _gsDebugAssert			(const char *string)
	{
		OutputDebugString( string);
		exit(0);
	}

#elif defined _WIN32
	#include <windows.h>
	#pragma warning(disable: 4127) 

	void  _gsDebugAssert			(const char *string)
	{

		

		#ifdef _CONSOLE   

			printf("%s",string);
			while(1)
			{
			};

		#else
		{		
			OutputDebugStringA( string);

			#ifndef GS_ASSERT_NO_MESSAGEBOX
			{
				int rcode = MessageBoxA(NULL,string,"Assert Failed",MB_ABORTRETRYIGNORE|MB_ICONHAND|MB_SETFOREGROUND|MB_TASKMODAL);

				if(rcode == IDABORT)
				{
					exit(0);
				}
				if(rcode == IDRETRY)
				{
					DebugBreak();
					return;
				}
			}
			#else
				DebugBreak();
			#endif
		}
		#endif
	}

#elif defined _PSP
	void  _gsDebugAssert			(const char *string)
	{
		printf("%s", string);
		
		while(1)
		{
		};

	}


#elif defined _PS2


	void  _gsDebugAssert			(const char *string)
	{

		scePrintf("%s", string);
		while(1);;
	}



#elif defined _MACOSX
	void  _gsDebugAssert			(const char *string)
	{
		printf("%s", string);
		
		while(1)
		{
		};

	}


#elif defined _LINUX
	void  _gsDebugAssert			(const char *string)
	{
		printf("%s", string);
		
		while(1)
		{
		};

	}


#elif defined _NITRO
	void  _gsDebugAssert			(const char *string)
	{
#if SDK_FINALROM != 1
		OS_TPanic("%s",string);
#else
		GSI_UNUSED(string);
#endif
	}

#elif defined(_REVOLUTION)
	void _gsDebugAssert(const char *string)
	{
		OSHalt(string);
		
	}
#else
	void  _gsDebugAssert			(const char *string)
	{

		printf("%s", string);
		while(1);;

	}

#endif




