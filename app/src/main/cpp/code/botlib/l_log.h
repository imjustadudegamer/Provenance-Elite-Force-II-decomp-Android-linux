

void Log_Open(char *filename);
void Log_Close(void);
void Log_Shutdown(void);
void QDECL Log_Write(char *fmt, ...) Q_PRINTF_FUNC(1, 2);
void QDECL Log_WriteTimeStamped(char *fmt, ...) Q_PRINTF_FUNC(1, 2);
FILE *Log_FilePointer(void);
void Log_Flush(void);

