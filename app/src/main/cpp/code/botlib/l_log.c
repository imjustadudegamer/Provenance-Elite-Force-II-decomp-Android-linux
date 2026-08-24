

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "botlib.h"
#include "be_interface.h"			
#include "l_libvar.h"
#include "l_log.h"

#define MAX_LOGFILENAMESIZE		1024

typedef struct logfile_s
{
	char filename[MAX_LOGFILENAMESIZE];
	FILE *fp;
	int numwrites;
} logfile_t;

static logfile_t logfile;

void Log_Open(char *filename)
{
	char *ospath;
	if (!LibVarValue("log", "0")) return;
	if (!filename || !strlen(filename))
	{
		botimport.Print(PRT_MESSAGE, "openlog <filename>\n");
		return;
	} 
	if (logfile.fp)
	{
		botimport.Print(PRT_ERROR, "log file %s is already opened\n", logfile.filename);
		return;
	} 
	ospath = FS_BuildOSPath(Cvar_VariableString("fs_homestatepath"), Cvar_VariableString("fs_game"), filename);
	logfile.fp = fopen(ospath, "wb");
	if (!logfile.fp)
	{
		botimport.Print(PRT_ERROR, "can't open the log file %s\n", filename);
		return;
	} 
	Q_strncpyz(logfile.filename, filename, MAX_LOGFILENAMESIZE);
	botimport.Print(PRT_MESSAGE, "Opened log %s\n", logfile.filename);
} 
void Log_Close(void)
{
	if (!logfile.fp) return;
	if (fclose(logfile.fp))
	{
		botimport.Print(PRT_ERROR, "can't close log file %s\n", logfile.filename);
		return;
	} 
	logfile.fp = NULL;
	botimport.Print(PRT_MESSAGE, "Closed log %s\n", logfile.filename);
} 
void Log_Shutdown(void)
{
	if (logfile.fp) Log_Close();
} 
void QDECL Log_Write(char *fmt, ...)
{
	va_list ap;

	if (!logfile.fp) return;
	va_start(ap, fmt);
	vfprintf(logfile.fp, fmt, ap);
	va_end(ap);
	fflush(logfile.fp);
} 
void QDECL Log_WriteTimeStamped(char *fmt, ...)
{
	va_list ap;

	if (!logfile.fp) return;
	fprintf(logfile.fp, "%d   %02d:%02d:%02d:%02d   ",
					logfile.numwrites,
					(int) (botlibglobals.time / 60 / 60),
					(int) (botlibglobals.time / 60),
					(int) (botlibglobals.time),
					(int) ((int) (botlibglobals.time * 100)) -
							((int) botlibglobals.time) * 100);
	va_start(ap, fmt);
	vfprintf(logfile.fp, fmt, ap);
	va_end(ap);
	fprintf(logfile.fp, "\r\n");
	logfile.numwrites++;
	fflush(logfile.fp);
} 
FILE *Log_FilePointer(void)
{
	return logfile.fp;
} 
void Log_Flush(void)
{
	if (logfile.fp) fflush(logfile.fp);
} 

