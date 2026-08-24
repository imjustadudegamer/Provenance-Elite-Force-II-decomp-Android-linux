
#include "../sys_local.h"

#include <Windows.h>
#include <dbghelp.h>

#ifdef _MSC_VER
#  ifdef _DEBUG_MEM
#    include <crtdbg.h>
#  endif
#endif

void Sys_PlatformInit_New() {
#ifdef _MSC_VER
#  ifdef _DEBUG_MEM
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#  endif
#endif
}

void Sys_PrepareBackTrace() {
}

void Sys_PrintFaultContext(int signum, void *siginfo, void *context) {
    (void)signum;
    (void)siginfo;
    (void)context;
}

void Sys_PrintBackTrace() {
    void* backtrace[128];
    SYMBOL_INFO* symbol;
    HANDLE hProcess;
    unsigned int i;
    unsigned short frames;

    hProcess = GetCurrentProcess();

    SymInitialize(hProcess, NULL, TRUE);

    frames = CaptureStackBackTrace(0, ARRAY_LEN(backtrace), backtrace, NULL);
    symbol = (SYMBOL_INFO*)malloc(sizeof(SYMBOL_INFO) + 128 * sizeof(char));
    symbol->MaxNameLen = 127;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    for (i = 0; i < frames; i++)
    {
        SymFromAddr(hProcess, (DWORD64)(backtrace[i]), 0, symbol);

        fprintf(stderr, "=> %s[0x%p]\n", symbol->Name, (void*)symbol->Address);
    }

    free(symbol);
}

void Sys_DebugPrint(const char* message) {
    OutputDebugStringA(message);
}

void Sys_PumpMessageLoop(void)
{
    MSG msg;

    while (PeekMessageA(&msg, 0, 0, 0, 0))
    {
        if (!GetMessageA(&msg, 0, 0, 0)) {
            Com_Quit_f();
        }

        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

void SetNormalThreadPriority(void)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
}

void SetBelowNormalThreadPriority(void)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
}
