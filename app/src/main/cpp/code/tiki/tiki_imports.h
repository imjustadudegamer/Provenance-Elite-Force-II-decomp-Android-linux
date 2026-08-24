

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    void TIKI_DPrintf(const char *fmt, ...) Q_PRINTF_FUNC(1, 2);

    void  TIKI_Free(void *ptr);
    void *TIKI_Alloc(size_t size);

    void TIKI_FreeFile(void *buffer);
    int  TIKI_ReadFileEx(const char *qpath, void **buffer, qboolean quiet);

    void TIKI_UpdateScreen(void);

#ifdef __cplusplus
}
#endif
