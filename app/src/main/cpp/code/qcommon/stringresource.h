#ifndef STRINGRESOURCE_H
#define STRINGRESOURCE_H

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

void        initializeStringResource(void);
void        uninitializeStringResource(void);
void        loadGlobalStrings(void);
void        clearAllStrings(void);
void        loadLevelStrings(const char *levelName);
const char *getStringResource(const char *stringId);
qboolean    isStringResource(const char *text);
void        convertStringResourceIds(const char *text, char *out, int size);

#ifdef __cplusplus
}
#endif

#endif 
