#ifndef __GSIASSERT_H__
#define __GSIASSERT_H__

#if defined(__LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
extern "C" {
#endif
///////////////////////////////////////////////////////////////////////////////
//      1) #define _DEBUG to enable assert.  This should be set by compiler configuration.


// GS_ASSERT	Use this to trap any programming bugs, such as range checks, invalid parameters



/*
	***The reason for using a GameSpy assert or custom assert***
	
	 although assert is handled very gracefully on the windows platform, most consoles do very little of real use during assert.
	 Furthermore, the program counter is lost, along with the callstack sometimes.
	 By having a custom critical error function, an asm "break" can be set in it, or a debugger break point.  a call stack is
	 immediately available.  The error can be drawn onto the screen.  And the choice to ignore can also be given, in order
	 to continue stepping through code and further debug.
	
*/

extern  void  gsDebugAssert			(const char *format,const char *szError,const char *szFile, int line);
#ifndef _DEBUG

	#define GS_ASSERT(x)				{};		
	#define GS_ASSERT_STR(x, t)			{};		
	#define GS_ASSERT_ALIGN_16(x)		{}; 
	#define GS_FAIL()			
	#define GS_FAIL_STR(x)			

#else

	#define GS_ASSERT(x)				{ if(!(x))			{  gsDebugAssert("ASSERT on '" #x "' [%s] in %s line:%d\n",	"",					__FILE__,__LINE__); } };
	#define GS_ASSERT_STR(x,t)			{ if(!(x))			{  gsDebugAssert("ASSERT on '" #x "' [%s] in %s line:%d\n",	 t,					__FILE__,__LINE__); } };
	#define GS_ASSERT_ALIGN_16(x)		{ if(((U32)(x))%16) {  gsDebugAssert("ASSERT on '" #x "' [%s] in %s line:%d\n","16 byte misalign",	__FILE__,__LINE__); } };
	#define GS_FAIL()										{  gsDebugAssert("FAIL  [%s] ln %s line:%d\n",	"",					__FILE__,__LINE__);	};											
	#define GS_FAIL_STR(t)									{  gsDebugAssert("FAIL  [%s] ln %s line:%d\n",	t,					__FILE__,__LINE__);	};											
																				  


#endif 


// This is the default assert condition handler
typedef void (*gsDebugAssertCallback)	(const char *string);
	
//  Call this function to override the default assert handler  
void gsDebugAssertCallbackSet(gsDebugAssertCallback theCallback);


// This is like an assert, but test at compile, not run time.
#define GS_STATIC_CHECK(expr, msg)    { CompileTimeError<((expr) != 0)> ERROR_##msg; (void)ERROR_##msg; } 


#if defined(__LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
}
#endif

#endif 
