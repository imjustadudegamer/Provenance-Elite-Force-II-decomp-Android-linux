


#ifdef MEMDEBUG
#define GetMemory(size)				GetMemoryDebug(size, #size, __FILE__, __LINE__);
#define GetClearedMemory(size)		GetClearedMemoryDebug(size, #size, __FILE__, __LINE__);
void *GetMemoryDebug(unsigned long size, char *label, char *file, int line);
void *GetClearedMemoryDebug(unsigned long size, char *label, char *file, int line);
#define GetHunkMemory(size)			GetHunkMemoryDebug(size, #size, __FILE__, __LINE__);
#define GetClearedHunkMemory(size)	GetClearedHunkMemoryDebug(size, #size, __FILE__, __LINE__);
void *GetHunkMemoryDebug(unsigned long size, char *label, char *file, int line);
void *GetClearedHunkMemoryDebug(unsigned long size, char *label, char *file, int line);
#else
void *GetMemory(unsigned long size);
void *GetClearedMemory(unsigned long size);
#ifdef BSPC
#define GetHunkMemory GetMemory
#define GetClearedHunkMemory GetClearedMemory
#else
void *GetHunkMemory(unsigned long size);
void *GetClearedHunkMemory(unsigned long size);
#endif
#endif

void FreeMemory(void *ptr);
int AvailableMemory(void);
void PrintUsedMemorySize(void);
void PrintMemoryLabels(void);
int MemoryByteSize(void *ptr);
void DumpMemory(void);
